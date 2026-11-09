"""
Name:        find_gaps.py
Description: finds the stretches of the 68000 program that the listing does not cover and guesses what each one is
             (fill, text, a table of code addresses, code the analysis has not reached, or other data)

Usage (from the repository root):
    python tools/find_gaps.py [listing] [minimum bytes] [rom folder or zip]   (in any order)
    (defaults: build/listing/donpachi.s, 64, rom)

It works from the listing, whatever the core's coverage.py says: a gap starts after the last instruction of a run of
listed instructions. The length of that last instruction is estimated (a run can start up to a few bytes later than
shown), so the first bytes of a gap can still belong to the instruction before it.
"""
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402
from find_tables import read_table  # noqa: E402

OPERAND = re.compile(r"(?<![#\w$])\$([0-9a-fA-F]{3,6})\b")

INSTRUCTION = re.compile(r"^\s+([0-9A-Fa-f]{6})\s+(\S+)\s*(.*)$")
# words that are common in code and rare in data
OPCODES = {0x4E75, 0x4EB9, 0x4EF9, 0x4E71, 0x6000, 0x6100, 0x4E5E, 0x4E56, 0x41F9, 0x43F9, 0x45F9, 0x47F9, 0x49F9,
           0x33FC, 0x31FC, 0x11FC, 0x13FC, 0x303C, 0x323C, 0x343C, 0x4A79, 0x4A6E, 0x4A2D, 0x48E7, 0x4CDF, 0x4E73,
           0x2F0E, 0x2F0D, 0x2F0C, 0x588F, 0x548F, 0x4879, 0x23FC, 0x21FC, 0x2D7C, 0x3D7C, 0x1D7C}


def estimate_size(mnemonic, operands, address):
    """The bytes of an instruction that ends a run of code, as well as can be told from its text."""
    if mnemonic in ("rts", "rte", "rtr", "nop", "illegal"):
        return 2
    if mnemonic in ("jmp", "jsr"):
        if operands.endswith(".l"):
            return 6
        if operands.startswith("("):
            return 2 if operands.startswith("(A") and "," not in operands else 4
        return 4
    if mnemonic in ("bra", "bsr"):
        match = re.match(r"\$([0-9a-fA-F]+)", operands)
        if match:
            distance = int(match.group(1), 16) - (address + 2)
            return 2 if -128 <= distance <= 127 and distance not in (0, -1) else 4
    return 2


def covered_ranges(listing):
    """Runs of listed instructions as (start, end, text of the last instruction)."""
    entries = []
    for line in listing.read_text(errors="replace").splitlines():
        match = INSTRUCTION.match(line)
        if match:
            entries.append((int(match.group(1), 16), match.group(2), match.group(3)))
    entries.sort()
    ranges = []
    for index, (address, mnemonic, operands) in enumerate(entries):
        following = entries[index + 1][0] if index + 1 < len(entries) else None
        if following is not None and following - address <= 10:
            end = following
        else:
            end = address + estimate_size(mnemonic, operands, address)
        text = f"{mnemonic} {operands}".strip()
        if ranges and address <= ranges[-1][1]:
            last = text if end >= ranges[-1][1] else ranges[-1][2]
            ranges[-1] = (ranges[-1][0], max(ranges[-1][1], end), last)
        else:
            ranges.append((address, end, text))
    return ranges


def flow_note(text):
    """Whether the instruction before a gap ends the flow (the gap needs an entry of its own) or lets it fall through."""
    mnemonic = text.split()[0] if text else ""
    if not mnemonic:
        return ""
    return "ends flow" if mnemonic in ("rts", "rte", "rtr", "jmp", "bra", "illegal") else "FALLS THROUGH?"


def classify(program, start, end):
    """(kind, note) for the gap start-end."""
    data = program[start:end]
    if not data:
        return "empty", ""
    if data.count(0) >= 0.95 * len(data):
        return "fill (00)", ""
    if data.count(0xFF) >= 0.95 * len(data):
        return "fill (FF)", ""
    # a table of jump stubs (bra.w, jmp.l ...): the gap may start a word or two before the table
    for skip in (0, 2, 4):
        for size in (4, 8, 6):
            entries = read_table(program, start + skip, size)
            if len(entries) >= 6 and any(e[3] is not None for e in entries):
                return "jump stubs?", f"{len(entries)} x {size} at {start + skip:06X}"
    # a table of 32-bit code addresses
    for skip in (0, 2, 4):
        longs = [int.from_bytes(data[i:i + 4], "big") for i in range(skip, min(len(data), 128) - 3, 4)]
        if len(longs) >= 8 and all(not v & 1 and 0x420 <= v < rom.PROGRAM_SIZE for v in longs[:8]):
            return "table of addresses?", f"at {start + skip:06X}"
    printable = sum(1 for b in data if 0x20 <= b < 0x7F)
    terminators = sum(1 for b in data if b in (0, 0x0A, 0x0D))
    if len(data) >= 16 and printable >= 0.6 * len(data) and printable + terminators >= 0.9 * len(data):
        return "text?", ""
    words = [int.from_bytes(data[i:i + 2], "big") for i in range(start & 1, min(len(data), 4096) - 1, 2)]
    hits = sum(1 for w in words if w in OPCODES)
    if words and hits / len(words) >= 0.015:
        return "CODE?", ""
    return "data", ""


def main():
    # the arguments may come in any order: a number is the minimum size, a .s file the listing, anything else the ROM
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

    ranges = covered_ranges(listing)
    covered = sum(end - start for start, end, _ in ranges)
    gaps = []
    previous_end, previous_text = 0x420, ""
    for start, end, text in ranges:
        if start > previous_end:
            gaps.append((previous_end, start, previous_text))
        if end >= previous_end:
            previous_end, previous_text = end, text
    if previous_end < rom.PROGRAM_SIZE:
        gaps.append((previous_end, rom.PROGRAM_SIZE, previous_text))

    # who points at a gap: the listing's operands ($xxxx) and the 32-bit words in the gaps themselves
    named = Counter()
    for line in listing.read_text(errors="replace").splitlines():
        match = INSTRUCTION.match(line)
        if match:
            for text in OPERAND.findall(match.group(3)):
                named[int(text, 16)] += 1
    held = Counter()
    for start, end, _ in gaps:
        for offset in range(start + (start & 1), end - 3, 2):
            value = int.from_bytes(program[offset:offset + 4], "big")
            if not value & 1 and 0x420 <= value < rom.PROGRAM_SIZE:
                held[value] += 1

    totals = {}
    rows = []
    for start, end, after in gaps:
        kind, note = classify(program, start, end)
        totals[kind] = totals.get(kind, 0) + (end - start)
        if end - start >= minimum:
            near = range(start + (start & 1), start + 10, 2)
            rows.append((end - start, start, end, kind, note, sum(named[a] for a in near), sum(held[a] for a in near), after))

    print(f"Listed code: {covered} bytes in {len(ranges)} runs; {sum(e - s for s, e, _ in gaps)} bytes in {len(gaps)} gaps")
    print()
    print("What the gaps look like (by bytes):")
    for kind, size in sorted(totals.items(), key=lambda item: -item[1]):
        print(f"  {kind:22} {size:8}")
    print()
    print(f"Gaps of {minimum} bytes or more, largest first. refs = operands in the listing / words in the gaps that hold")
    print("the address of its first bytes. 'after' is the last instruction before the gap: if it falls through, the gap may")
    print("be code the analysis stopped short of (a call that never returns?); if it ends the flow, the gap needs an entry.")
    print(f"{'first':>8} {'last':>8} {'bytes':>7}  {'guess':20} {'refs':>7}  first bytes")
    for size, start, end, kind, note, listed, words, after in sorted(rows, reverse=True)[:80]:
        preview = " ".join(f"{b:02X}" for b in program[start:start + 12])
        print(f"{start:08X} {end - 1:08X} {size:7}  {kind:20} {listed:>3}/{words:<3}  {preview}" + (f"   [{note}]" if note else ""))
        if after:
            print(f"{'':>8} after: {after}  ({flow_note(after)})")
    if len(rows) > 80:
        print(f"... and {len(rows) - 80} more")


if __name__ == "__main__":
    main()
