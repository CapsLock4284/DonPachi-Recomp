"""
Name:        probe_table.py
Description: decodes a table of jump stubs in the DonPachi program (JMP/BRA/JSR/RTS records), to settle what a
             dispatch through a register jumps to, and suggests the symbols.txt lines for it

Usage (from the repository root):
    python tools/probe_table.py ADDRESS RECORD_SIZE COUNT [NAME_PREFIX] [rom folder or zip]
e.g.
    python tools/probe_table.py 0x43A 8 61 task
    python tools/probe_table.py 0x1662 4 16 command
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402


def s16(value):
    return value - 0x10000 if value & 0x8000 else value


def s8(value):
    return value - 0x100 if value & 0x80 else value


def decode(program, address):
    """Return (text, target or None) for the stub at address."""
    word = int.from_bytes(program[address:address + 2], "big")
    ext = int.from_bytes(program[address + 2:address + 4], "big")
    if word == 0x4EF9:
        target = int.from_bytes(program[address + 2:address + 6], "big")
        return f"jmp ${target:X}.l", target
    if word == 0x4EB9:
        target = int.from_bytes(program[address + 2:address + 6], "big")
        return f"jsr ${target:X}.l", target
    if word == 0x4EF8:
        target = s16(ext) & 0xFFFFFF
        return f"jmp ${target:X}.w", target
    if word == 0x6000:
        target = address + 2 + s16(ext)
        return f"bra.w ${target:X}", target
    if word == 0x6100:
        target = address + 2 + s16(ext)
        return f"bsr.w ${target:X}", target
    if word >> 8 == 0x60 and word & 0xFF not in (0, 0xFF):
        target = address + 2 + s8(word & 0xFF)
        return f"bra.s ${target:X}", target
    if word == 0x4E75:
        return "rts", None
    if word == 0x4E71:
        return "nop", None
    return "?", None


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    start = int(sys.argv[1], 0)
    size = int(sys.argv[2], 0)
    count = int(sys.argv[3], 0)
    prefix = sys.argv[4] if len(sys.argv) > 4 else "entry"
    source = sys.argv[5] if len(sys.argv) > 5 else "rom"
    program = rom.build_program(rom.load_set(source))

    decoded = []
    for index in range(count):
        address = start + index * size
        text, target = decode(program, address)
        raw = " ".join(f"{b:02X}" for b in program[address:address + size])
        print(f"{index:3}  {address:06X}  {raw:<30}  {text}")
        decoded.append((index, address, text, target))

    print()
    unknown = [d for d in decoded if d[2] == "?"]
    if unknown:
        print(f"{len(unknown)} record(s) did not decode as a jump, so they may not be jump stubs: "
              "do not add this table to symbols.txt as it is.")
        return
    if all(text.startswith("jmp $") and text.endswith(".l") for _, _, text, _ in decoded) and size >= 6:
        print("Every record is a jmp with a long address at offset 2, so one line is enough:")
        print(f"{start:06X}  data  {prefix}_table  record={size} count={count} code=2")
        return
    print("Lines for symbols.txt, one function a target (names are placeholders):")
    seen = set()
    for index, _, text, target in decoded:
        if target is not None and target not in seen and 0x420 <= target < rom.PROGRAM_SIZE:
            seen.add(target)
            print(f"{target:06X}  func  {prefix}_{index:02d}")


if __name__ == "__main__":
    main()
