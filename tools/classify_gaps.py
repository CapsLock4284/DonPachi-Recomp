"""
Name:        classify_gaps.py
Description: takes the stretches of the 68000 program that the listing does not cover and decodes each one with the
             core's disassembler (tools/core/m68000/disassembler.py): how far from its start it decodes as ordinary
             instructions tells code the analysis has not reached from data

Usage (from the repository root):
    python tools/classify_gaps.py [listing] [minimum bytes] [rom folder or zip]   (in any order)
    (defaults: build/listing/donpachi.s, 64, rom)

It prints the gaps largest first and writes build/gap_symbols.txt: the entry points it suggests for the gaps that
decode as code. Look through them (and through the listing they give) before adding any to symbols.txt.

How it decides: from the start of a gap, and from a few bytes after it (the listing ends a run of code by an
estimate), the instructions are decoded one after another until one is invalid. A run counts as code when it holds at
least 8 instructions, half of them the ordinary ones (move, lea, jsr, rts, bra, tst, cmp ...) and few of the odd ones
that small numbers decode as (ori, negx ...). Runs of jump stubs (bra.w, jmp.l) are left to probe_table.py.
"""
import re
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import donpachi_rom as rom  # noqa: E402
from find_gaps import covered_ranges, flow_note  # noqa: E402
from find_tables import read_table  # noqa: E402
from m68000 import disassembler as dis  # noqa: E402

SYMBOL = re.compile(r"^([0-9A-Fa-f]{6})\s+(?:func|code|data)\b")
OFFSETS = (0, 2, 4, 6, 8, 10)
LIMIT = 20000
TYPICAL = {"move", "movea", "moveq", "lea", "bsr", "jsr", "rts", "bra", "bcc", "dbcc", "tst", "cmp", "cmpi", "cmpa",
           "add", "adda", "addq", "sub", "suba", "subq", "and", "andi", "or", "clr", "btst", "bset", "bclr", "jmp",
           "movem", "asl", "asr", "lsl", "lsr", "ext", "swap", "pea", "eor", "eori", "not", "neg", "scc", "rte"}
ODD = {"ori", "negx", "nbcd", "abcd", "sbcd", "movep", "reset", "stop", "trapv", "chk", "addx", "subx", "roxl",
       "roxr", "cmpm", "tas", "exg", "bchg", "illegal", "halt", "trap"}


def sweep(mem, start, end):
    """Decode from start until an invalid instruction or the end of the gap: (bytes, instructions, typical, odd)."""
    address = start
    count = typical = odd = 0
    while count < LIMIT and address < end:
        instruction = dis.decode(mem, address)
        if instruction.flow == dis.FLOW_INVALID or address + instruction.length > end + 10:
            break
        count += 1
        typical += instruction.mnemonic in TYPICAL
        odd += instruction.mnemonic in ODD
        address += instruction.length
    return address - start, count, typical, odd


def best_run(mem, start, end):
    """The longest run that looks like code, from the gap's start or a few bytes after it: (bytes, count, offset)."""
    best = (0, 0, 0)
    for offset in OFFSETS:
        if start + offset >= end:
            break
        size, count, typical, odd = sweep(mem, start + offset, end)
        if count >= 8 and typical >= 0.5 * count and odd <= 0.2 * count and size > best[0]:
            best = (size, count, offset)
    return best


def main():
    listing = Path("build/listing/donpachi.s")
    minimum = 64
    source = "rom"
    for argument in sys.argv[1:]:
        if argument.isdigit() or argument.lower().startswith("0x"):
            minimum = int(argument, 0)
        elif argument.lower().endswith(".s"):
            listing = Path(argument)
        else:
            source = argument
    if not listing.exists():
        raise SystemExit(f"The listing {listing} does not exist: run python tools/disassemble.py first.")
    program = rom.build_program(rom.load_set(source))
    mem = dis.FlatMemory(program)

    known = set()
    symbols = Path("symbols.txt")
    if symbols.exists():
        for line in symbols.read_text(errors="replace").splitlines():
            match = SYMBOL.match(line)
            if match:
                known.add(int(match.group(1), 16))

    ranges = covered_ranges(listing)
    gaps = []
    previous_end, previous_text = 0x420, ""
    for start, end, text in ranges:
        if start > previous_end:
            gaps.append((previous_end, start, previous_text))
        if end >= previous_end:
            previous_end, previous_text = end, text
    if previous_end < rom.PROGRAM_SIZE:
        gaps.append((previous_end, rom.PROGRAM_SIZE, previous_text))

    totals = {}
    run_bytes = 0
    rows = []
    for start, end, after in gaps:
        size = end - start
        data = program[start:end]
        stubs = None
        for skip in (0, 2, 4):
            for stub_size in (4, 8, 6):
                entries = read_table(program, start + skip, stub_size)
                if len(entries) >= 6 and any(e[3] is not None for e in entries):
                    stubs = (start + skip, stub_size, len(entries))
                    break
            if stubs:
                break
        if data.count(0) >= 0.95 * size or data.count(0xFF) >= 0.95 * size:
            kind, run = "fill", (0, 0, 0)
        elif stubs:
            kind, run = "jump stubs", (0, 0, 0)
        else:
            run = best_run(mem, start, end)
            if run[0] >= 0.9 * size and run[0] >= 24:
                kind = "code"
            elif run[0] >= 64:
                kind = "code+data"
            else:
                kind = "data"
        if kind in ("code", "code+data"):
            run_bytes += run[0]
        totals[kind] = totals.get(kind, 0) + size
        if size >= minimum:
            rows.append((size, start, end, kind, run, stubs, after))

    print(f"{len(gaps)} gaps, {sum(e - s for s, e, _ in gaps)} bytes not covered by the listing")
    print()
    print("What the gaps are (by bytes):")
    for kind, size in sorted(totals.items(), key=lambda item: -item[1]):
        print(f"  {kind:12} {size:8}")
    print(f"  of which decode as code from the start: about {run_bytes} bytes")
    print()
    print(f"Gaps of {minimum} bytes or more, largest first:")
    print(f"{'first':>8} {'last':>8} {'bytes':>7}  {'class':10} decodes from start")
    suggestions = []
    for size, start, end, kind, run, stubs, after in sorted(rows, reverse=True)[:100]:
        detail = ""
        if kind in ("code", "code+data"):
            detail = f"{run[0]} bytes, {run[1]} instructions" + (f", starting {run[2]} bytes in" if run[2] else "")
        elif stubs:
            detail = f"{stubs[2]} stubs of {stubs[1]} at {stubs[0]:06X}: python tools/probe_table.py 0x{stubs[0]:X} {stubs[1]} {stubs[2]}"
        print(f"{start:08X} {end - 1:08X} {size:7}  {kind:10} {detail}")
        if after:
            print(f"{'':>8} after: {after}  ({flow_note(after)})")
    for size, start, end, kind, run, stubs, after in sorted(rows):
        if kind in ("code", "code+data") and run[0] >= 64:
            address = start + run[2]
            if address in known:
                continue
            mnemonic = after.split()[0] if after else ""
            entry = "func" if mnemonic in ("rts", "rte", "rtr") else "code"
            suggestions.append(f"{address:06X}  {entry:<4}  gap_{address:06X}   # {run[0]} bytes decode as code ({after})")
    if len(rows) > 100:
        print(f"... and {len(rows) - 100} more")

    destination = Path("build/gap_symbols.txt")
    destination.parent.mkdir(parents=True, exist_ok=True)
    header = ["# Entry points suggested by tools/classify_gaps.py for the gaps that decode as code. Review each one first:",
              "# after adding it, the instructions that follow should look like code in the listing; take out one that does not."]
    destination.write_text("\n".join(header + suggestions) + "\n")
    print()
    print(f"Wrote {destination} with {len(suggestions)} suggested entry points.")


if __name__ == "__main__":
    main()
