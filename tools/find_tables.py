"""
Name:        find_tables.py
Description: finds the jumps through a register and a PC-relative table in the listing (jmp/jsr (d,PC,Xn)), decodes
             the table of jump stubs each one reads and writes the symbols.txt lines for their targets: functions
             for the tables a jsr reads, code labels for those only a jmp reads

Usage (from the repository root):
    python tools/find_tables.py [listing] [symbols.txt] [rom folder or zip]
    (defaults: build/listing/donpachi.s, symbols.txt, rom)

It writes build/jump_table_symbols.txt: look through it, then add it to symbols.txt with
    Get-Content build\\jump_table_symbols.txt | Add-Content symbols.txt
The table of a jump (d,PC,Xn) at address A starts at A + 2 + d (PC is the address of the extension word).
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402
from probe_table import decode  # noqa: E402

SITE = re.compile(r"^\s+([0-9A-Fa-f]{6})\s+(jsr|jmp)\s+\((-?)\$([0-9a-fA-F]+),PC,[AD][0-7]\.[wl]\)")
SYMBOL = re.compile(r"^([0-9A-Fa-f]{6})\s+(?:func|code|data)\b")
SIZES = (4, 2, 6, 8)   # the sizes of record tried, in the order that wins a tie
LIMIT = 64


def natural_size(text):
    """The bytes the stub takes, or None if it is not a jump stub."""
    if text.startswith("bra.s") or text == "rts":
        return 2
    if text.startswith(("bra.w", "bsr.w")):
        return 4
    if text.startswith(("jmp", "jsr")):
        return 4 if text.endswith(".w") else 6
    return None


def read_table(program, base, size):
    entries = []
    while len(entries) < LIMIT:
        address = base + len(entries) * size
        if address + size > rom.PROGRAM_SIZE:
            break
        text, target = decode(program, address)
        natural = natural_size(text)
        if natural is None or natural > size:
            break
        if target is not None and (target & 1 or not 0x420 <= target < rom.PROGRAM_SIZE):
            break
        entries.append((len(entries), address, text, target))
    return entries


def best_table(program, base):
    best = (0, None, [])
    for size in SIZES:
        entries = read_table(program, base, size)
        if len(entries) > best[0]:
            best = (len(entries), size, entries)
    return best[1], best[2]


def main():
    listing = Path(sys.argv[1] if len(sys.argv) > 1 else "build/listing/donpachi.s")
    symbols = Path(sys.argv[2] if len(sys.argv) > 2 else "symbols.txt")
    source = sys.argv[3] if len(sys.argv) > 3 else "rom"
    program = rom.build_program(rom.load_set(source))

    known = set()
    if symbols.exists():
        for line in symbols.read_text(errors="replace").splitlines():
            match = SYMBOL.match(line)
            if match:
                known.add(int(match.group(1), 16))

    bases = {}
    for line in listing.read_text(errors="replace").splitlines():
        match = SITE.match(line)
        if match:
            site = int(match.group(1), 16)
            displacement = int(match.group(4), 16) * (-1 if match.group(3) else 1)
            bases.setdefault(site + 2 + displacement, []).append((site, match.group(2)))

    out = ["# Jump tables found by tools/find_tables.py from the listing (a jmp or jsr (d,PC,Xn) reads a table of jump stubs).",
           "# Names are placeholders: case_<table>_<entry>. A table read by a jsr gives functions (the routines it calls);",
           "# one read only by a jmp gives code labels (its cases lie inside the routine that jumps). Review each table."]
    for base in sorted(bases):
        sites = ", ".join(f"{s:06X}" for s, _ in bases[base])
        # a jsr calls a routine (a function); a jmp goes to a case inside the routine it is in (a code label)
        kind = "func" if any(op == "jsr" for _, op in bases[base]) else "code"
        size, entries = best_table(program, base)
        if not entries:
            print(f"{base:06X}  read at {sites}: no jump stubs found here: look at it by hand")
            out.append(f"# {base:06X}: read at {sites}: no jump stubs found, to look at by hand")
            continue
        note = ""
        if len(entries) < 3:
            note = "  <- only a few: check by hand"
        if len(entries) >= LIMIT:
            note = "  <- hit the limit: it may be longer, or not a table"
        print(f"{base:06X}  read at {sites}: {len(entries)} records of {size} bytes{note}")
        print("        " + "  ".join(f"{i}:{t:X}" if t is not None else f"{i}:rts" for i, _, _, t in entries))
        out.append("")
        out.append(f"# the table at {base:06X} (read at {sites}): {len(entries)} records of {size} bytes, {kind}s{note}")
        seen = set()
        for index, _, _, target in entries:
            if target is None or target in known or target in seen:
                continue
            seen.add(target)
            out.append(f"{target:06X}  {kind:<4}  case_{base:06X}_{index:02d}")

    destination = Path("build/jump_table_symbols.txt")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(out) + "\n")
    print()
    print(f"Wrote {destination}. Look through it, then:")
    print("    Get-Content build\\jump_table_symbols.txt | Add-Content symbols.txt")


if __name__ == "__main__":
    main()
