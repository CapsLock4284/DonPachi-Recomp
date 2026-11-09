"""
Name:        find_callbacks.py
Description: finds the routines the stage scripts hand to 02D9A2's "jmp (A0)": script command 15 (02DE46) stores the
             long of its record in $10885C, and 02D996 jumps to it. Walks the scripts the way the game does and lists
             the command 15 records, for review
Version:     0.2.0 (draft)
Created:     08-11-2026
Modified:    08-11-2026 (0.2.0: walks the scripts from their table instead of scanning the whole ROM)

Usage:
    python tools/find_callbacks.py [rom folder or zip]   (default: rom)
    python tools/find_callbacks.py | Out-File -Encoding utf8 build\\callbacks_found.txt

How the game reads them (02D334-02D360): the table at 02D3F6 holds, for each stage ($101A40, 0 to 5), a pointer to 3
pointers, one to each of the stage's 3 scripts. A script is records of 8 bytes: a word, the position (the scroll's,
$101A56) where the record runs, then the command word, whose bits 2-6 pick the command (02DA44 "moveq #$7c, D0; and.w
(A0), D0"), then a long, the command's argument (02DE4C "move.l ($2,A0), (A1)+"). A script runs while the positions
go up; it is walked here until a position goes down or is FFFF, or MAX_RECORDS records. Nothing here changes any file.
"""
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402

STAGE_TABLE = 0x02D3F6
STAGES = 6           # (the jump tables 007850 and 007962, read by the stage, have 6 entries)
SCRIPTS = 3          # (02D34C "moveq #$2, D2" ... dbra)
COMMAND = 15
MAX_RECORDS = 4000


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else "rom"
    program = rom.build_program(rom.load_set(source))

    def word(address):
        return int.from_bytes(program[address:address + 2], "big")

    def long(address):
        return int.from_bytes(program[address:address + 4], "big") & 0xFFFFFF

    def in_rom(address):
        return not address & 1 and 0 < address < rom.PROGRAM_SIZE - 8

    found = {}
    print("# the stage scripts walked by tools/find_callbacks.py")
    for stage in range(STAGES):
        scripts = long(STAGE_TABLE + 4 * stage)
        if not in_rom(scripts):
            print(f"# stage {stage}: {scripts:06X} is no place in the ROM: stop")
            break
        for number in range(SCRIPTS):
            start = long(scripts + 4 * number)
            if not in_rom(start):
                print(f"# stage {stage} script {number}: {start:06X} is no place in the ROM")
                continue
            commands = Counter()
            place, last, records, why = start, -1, 0, f"{MAX_RECORDS} records"
            while records < MAX_RECORDS and in_rom(place):
                position = word(place)
                if position == 0xFFFF:
                    why = "position FFFF"
                    break
                if position < last:
                    why = f"position {position:04X} after {last:04X}"
                    break
                command = (word(place + 2) & 0x7C) >> 2
                commands[command] += 1
                if command == COMMAND:
                    found.setdefault(long(place + 4), []).append((stage, number, place))
                last = position
                place += 8
                records += 1
            used = " ".join(f"{command}:{count}" for command, count in sorted(commands.items()))
            print(f"# stage {stage} script {number}: {start:06X}-{place:06X}, {records} records (stops: {why});"
                  f" commands {used}")
    print(f"# command {COMMAND} arguments ({len(found)}): routine, then where (stage, script, record)")
    for target in sorted(found):
        places = ", ".join(f"{stage}/{number}/{place:06X}" for stage, number, place in found[target])
        odd = "" if in_rom(target) and target >= 0x420 else "   <- not code: check"
        print(f"{target:06X}  # {places}{odd}")


if __name__ == "__main__":
    main()
