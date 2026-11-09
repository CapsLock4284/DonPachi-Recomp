"""
Name:        find_pointer_tables.py
Description: finds the tables of 32-bit code addresses that the listing reads with "movea.l (d,PC,Dn), An" and then
             jumps through ("jmp (An)" or "jsr (An)"), reads each one and writes the symbols.txt line that describes it

Usage (from the repository root):
    python tools/find_pointer_tables.py [listing] [symbols.txt] [rom folder or zip]
    (defaults: build/listing/donpachi.s, symbols.txt, rom)

It writes build/pointer_table_symbols.txt: look through it, then add it to symbols.txt with
    Get-Content build\\pointer_table_symbols.txt | Add-Content symbols.txt
The table of a "movea.l (d,PC,Dn), An" at address A starts at A + 2 + d. A table is read until an entry is not an
even address in the program, or until the table reaches the lowest address it jumps to above itself (the first case
usually follows the table).
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402

MOVEA = re.compile(r"^\s+([0-9A-Fa-f]{6})\s+movea\.l\s+\((-?)\$([0-9a-fA-F]+),PC,[AD][0-7]\.[wl]\),\s*(A[0-7])\b")
JUMP = re.compile(r"^\s+[0-9A-Fa-f]{6}\s+(?:jmp|jsr)\s+\((A[0-7])\)")
SYMBOL = re.compile(r"^([0-9A-Fa-f]{6})\s+(?:func|code|data)\b")
LOOKAHEAD = 4        # instructions between the movea.l and the jump
LIMIT = 256


def read_table(program, base):
    targets = []
    lowest_forward = None
    while len(targets) < LIMIT:
        address = base + 4 * len(targets)
        if address + 4 > rom.PROGRAM_SIZE:
            break
        if lowest_forward is not None and address >= lowest_forward:
            break
        value = int.from_bytes(program[address:address + 4], "big")
        if value & 1 or not 0x420 <= value < rom.PROGRAM_SIZE:
            break
        targets.append(value)
        if value > base and (lowest_forward is None or value < lowest_forward):
            lowest_forward = value
    return targets


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

    lines = listing.read_text(errors="replace").splitlines()
    bases = {}
    for number, line in enumerate(lines):
        match = MOVEA.match(line)
        if not match:
            continue
        register = match.group(4)
        followed = False
        seen = 0
        for after in lines[number + 1:number + 1 + 3 * LOOKAHEAD]:
            if after.startswith("    ") and after.strip():
                seen += 1
                jump = JUMP.match(after)
                if jump and jump.group(1) == register:
                    followed = True
                    break
                if seen > LOOKAHEAD:
                    break
        if followed:
            site = int(match.group(1), 16)
            displacement = int(match.group(3), 16) * (-1 if match.group(2) else 1)
            bases.setdefault(site + 2 + displacement, []).append(site)

    out = ["# Tables of code addresses found by tools/find_pointer_tables.py: a movea.l (d,PC,Dn), An read one, a jmp or",
           "# jsr (An) went through it. Names are placeholders. Review each table before keeping it."]
    for base in sorted(bases):
        sites = ", ".join(f"{s:06X}" for s in bases[base])
        targets = read_table(program, base)
        if base in known:
            print(f"{base:06X}  read at {sites}: already in symbols.txt")
            continue
        if not targets:
            print(f"{base:06X}  read at {sites}: no addresses found here: look at it by hand")
            out.append(f"# {base:06X}: read at {sites}: no addresses found, to look at by hand")
            continue
        note = ""
        if len(targets) < 3:
            note = "  <- only a few: check by hand"
        if len(targets) >= LIMIT:
            note = "  <- hit the limit: not a table?"
        print(f"{base:06X}  read at {sites}: {len(targets)} entries, to {base + 4 * len(targets):06X}{note}")
        print("        " + "  ".join(f"{i}:{t:X}" for i, t in enumerate(targets)))
        out.append("")
        out.append(f"# read at {sites}: {len(targets)} entries{note}")
        out.append(f"{base:06X}  data  jump_table_{base:06X}  record=4 count={len(targets)} code=0")

    destination = Path("build/pointer_table_symbols.txt")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(out) + "\n")
    print()
    print(f"Wrote {destination}. Look through it, then:")
    print("    Get-Content build\\pointer_table_symbols.txt | Add-Content symbols.txt")


if __name__ == "__main__":
    main()
