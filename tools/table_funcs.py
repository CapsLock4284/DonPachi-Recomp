"""
Name:        table_funcs.py
Description: reads a 32-bit field (plus a constant) out of each record of a table and writes a symbols.txt "func"
             line for each distinct address: for a code pointer that a "data ... code=" line cannot express, such as
             the object types' init routines (the record's first pointer, plus 2 for the word the code skips)

Usage (from the repository root):
    python tools/table_funcs.py ADDRESS RECORD_SIZE COUNT FIELD_OFFSET ADDEND PREFIX [FIRST_INDEX] [symbols.txt] [rom]
e.g. the 168 object types (the table starts at the second record, type 1):
    python tools/table_funcs.py 0x1275E 8 168 0 2 object_init 1

It writes build/table_func_symbols.txt: look through it, then add it to symbols.txt with
    Get-Content build\\table_func_symbols.txt | Add-Content symbols.txt
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402

SYMBOL = re.compile(r"^([0-9A-Fa-f]{6})\s+(?:func|code|data)\b")


def main():
    if len(sys.argv) < 7:
        raise SystemExit(__doc__)
    start = int(sys.argv[1], 0)
    size = int(sys.argv[2], 0)
    count = int(sys.argv[3], 0)
    offset = int(sys.argv[4], 0)
    addend = int(sys.argv[5], 0)
    prefix = sys.argv[6]
    first = int(sys.argv[7], 0) if len(sys.argv) > 7 else 0
    symbols = Path(sys.argv[8] if len(sys.argv) > 8 else "symbols.txt")
    source = sys.argv[9] if len(sys.argv) > 9 else "rom"
    program = rom.build_program(rom.load_set(source))

    known = set()
    if symbols.exists():
        for line in symbols.read_text(errors="replace").splitlines():
            match = SYMBOL.match(line)
            if match:
                known.add(int(match.group(1), 16))

    lines = []
    seen = set()
    bad = []
    for index in range(count):
        address = start + index * size + offset
        target = int.from_bytes(program[address:address + 4], "big") + addend
        if target & 1 or not 0x420 <= target < rom.PROGRAM_SIZE:
            bad.append((first + index, target))
            continue
        if target in known or target in seen:
            continue
        seen.add(target)
        lines.append(f"{target:06X}  func  {prefix}_{first + index:03d}")

    print(f"{count} records read, {len(lines)} new addresses to add")
    for index, target in bad:
        print(f"  record {index}: {target:X} is not an even address in the program: left out")
    for line in lines[:8]:
        print("  " + line)
    if len(lines) > 8:
        print(f"  ... and {len(lines) - 8} more")

    destination = Path("build/table_func_symbols.txt")
    destination.parent.mkdir(parents=True, exist_ok=True)
    header = [f"# from the table at {start:06X}: the field at +{offset} of each record, plus {addend} (tools/table_funcs.py)"]
    destination.write_text("\n".join(header + lines) + "\n")
    print()
    print(f"Wrote {destination}. Look through it, then:")
    print("    Get-Content build\\table_func_symbols.txt | Add-Content symbols.txt")


if __name__ == "__main__":
    main()
