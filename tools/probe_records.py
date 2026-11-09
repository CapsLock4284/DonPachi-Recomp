"""
Name:        probe_records.py
Description: reads a table of records made of 32-bit pointers (such as 012756's: a data pointer and a code pointer a
             record), finds where it ends and suggests the symbols.txt line that describes it

Usage (from the repository root):
    python tools/probe_records.py ADDRESS RECORD_SIZE CODE_OFFSET [NAME] [rom folder or zip]
e.g.
    python tools/probe_records.py 0x12756 8 4 object_types

A record is taken as part of the table while each of its 32-bit fields is an even address inside the program and the
record is not all zeros. The end it finds is a guess: check the last records it shows.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402

LIMIT = 512


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    start = int(sys.argv[1], 0)
    size = int(sys.argv[2], 0)
    code = int(sys.argv[3], 0)
    name = sys.argv[4] if len(sys.argv) > 4 else "records"
    source = sys.argv[5] if len(sys.argv) > 5 else "rom"
    program = rom.build_program(rom.load_set(source))

    records = []
    reason = "the limit"
    while len(records) < LIMIT:
        address = start + len(records) * size
        if address + size > rom.PROGRAM_SIZE:
            reason = "the end of the program"
            break
        fields = [int.from_bytes(program[address + k:address + k + 4], "big") for k in range(0, size, 4)]
        if not any(fields):
            reason = "an all-zero record"
            break
        bad = [f for f in fields if f & 1 or f >= rom.PROGRAM_SIZE]
        if bad:
            reason = f"a field that is not an even address in the program ({bad[0]:08X})"
            break
        records.append((address, fields))

    for index, (address, fields) in enumerate(records):
        print(f"{index:4}  {address:06X}  " + "  ".join(f"{f:08X}" for f in fields))
    end = start + len(records) * size
    print()
    print(f"{len(records)} records, to {end:06X}; it stopped at {reason}.")

    first = 0
    if records and start <= records[0][1][code // 4] < end:
        first = 1
        print("The first record's code pointer points into the table itself: probably an unused entry, so the line")
        print("below starts at the second record.")
    if len(records) > first:
        print()
        print("For symbols.txt (check the last records above first):")
        print(f"{start + first * size:06X}  data  {name}  record={size} count={len(records) - first} code={code}")


if __name__ == "__main__":
    main()
