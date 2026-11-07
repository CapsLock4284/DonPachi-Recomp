"""
Name:        show_vectors.py
Description: prints the 68000 vector table of the DonPachi program (grouped by target) and the first bytes of the
             program after the vectors and version string, to settle CODE_START and find the interrupt handlers.

Run from the repository root:  python tools/show_vectors.py [rom folder or zip]   (default: rom)
"""
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402

NAMES = {0: "initial stack pointer", 1: "reset (first instruction)", 2: "bus error", 3: "address error",
         4: "illegal instruction", 5: "divide by zero", 6: "CHK", 7: "TRAPV", 8: "privilege violation",
         9: "trace", 10: "line A", 11: "line F", 15: "uninitialised interrupt", 24: "spurious interrupt"}
for level in range(1, 8):
    NAMES[24 + level] = f"interrupt level {level}"
for trap in range(16):
    NAMES[32 + trap] = f"TRAP #{trap}"


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else "rom"
    program = rom.build_program(rom.load_set(source))

    groups = defaultdict(list)
    for number in range(256):
        target = int.from_bytes(program[number * 4:number * 4 + 4], "big")
        groups[target].append(number)

    print("Vector table (targets shared by several vectors are listed together):")
    for target, numbers in sorted(groups.items(), key=lambda item: item[1][0]):
        if len(numbers) > 4:
            label = f"{len(numbers)} vectors (the first is number {numbers[0]})"
        else:
            label = ", ".join(f"{n} at {n * 4:03X}" + (f" ({NAMES[n]})" if n in NAMES else "") for n in numbers)
        print(f"  {target:08X}  {label}")

    print()
    print("The version string:", bytes(program[0x400:0x420]))
    start = 0x420
    while start < len(program) and program[start] in (0x00, 0xFF):
        start += 1
    print(f"First byte after 0x420 that is not 00 or FF: {start:06X}")
    print("  ", " ".join(f"{b:02X}" for b in program[start:start + 32]))


if __name__ == "__main__":
    main()
