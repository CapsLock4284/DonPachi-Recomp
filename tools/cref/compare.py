"""
Name:        compare.py
Description: compares the decompiled program's run with the original's on MAME's 68000 (tools/cref): the first frame
             whose memories differ, and with the two runs' dumps of a frame, where they differ
Version:     0.1.0 (draft)
Created:     09-11-2026

Usage:
    python tools/cref/compare.py hashes OWN.txt REFERENCE.txt
        the first frame the two --hashes files differ in
    python tools/cref/compare.py dumps OWN.bin REFERENCE.bin [MOST]
        where the two --dump files differ: each run of differing bytes, by the 68000's address, at most MOST (40)

The dumps are cave_dump's (runtime/src/machine.c): the regions below, one after the other. The 68000's stack (STACK)
is left out: the decompiled program keeps its own in C.
"""
import sys

# (name, its 68000 address or None, its size in bytes, how its bytes are stored)
REGIONS = [
    ("work RAM", 0x100000, 0x10000, "bytes"),
    ("layer 0 RAM", 0x300000, 0x8000, "words"),
    ("layer 1 RAM", 0x200000, 0x8000, "words"),
    ("layer 2 RAM", 0x400000 - 0x4000, 0x8000, "words"),    # (its 8x8 tiles at 0x4000: 400000)
    ("layer registers", None, 18, "words"),
    ("sprite RAM", 0x500000, 0x10000, "words"),
    ("video registers", 0x900000, 0x80, "words"),
    ("palette", 0xA08000, 0x1000, "words"),
]


def first_difference(own_path, reference_path):
    def read(path):
        frames = {}
        for line in open(path):
            parts = line.split()
            if len(parts) == 2:
                frames[int(parts[0])] = parts[1]
        return frames
    own, reference = read(own_path), read(reference_path)
    common = sorted(set(own) & set(reference))
    for frame in common:
        if own[frame] != reference[frame]:
            print(f"the first frame that differs: {frame} (of the {len(common)} both runs have)")
            return frame
    print(f"the runs agree in all the {len(common)} frames both have")
    return None


def where(offset):
    """(region, the 68000's address of the byte at 'offset' of a dump)"""
    start = 0
    for name, address, size, kind in REGIONS:
        if offset < start + size:
            inside = offset - start
            if kind == "words":
                inside ^= 1    # (the host's words, little endian)
            return name, (address + inside) if address is not None else inside
        start += size
    return "past the dump", offset


STACK = (0x10F800, 0x110000)    # (the 68000's stack: the decompiled program keeps its own in C)


def compare_dumps(own_path, reference_path, most=40):
    own, reference = open(own_path, "rb").read(), open(reference_path, "rb").read()
    shown = 0
    offset = 0
    total = 0
    while offset < min(len(own), len(reference)):
        if own[offset] == reference[offset] or STACK[0] - 0x100000 <= offset < STACK[1] - 0x100000:
            offset += 1
            continue
        first = offset
        while offset < len(own) and own[offset] != reference[offset] \
                and not STACK[0] - 0x100000 <= offset < STACK[1] - 0x100000:
            offset += 1
        total += 1
        if shown < most:
            region, address = where(first)
            length = offset - first
            print(f"{region:16} {address:06X}  {length:5} bytes  own {own[first:first + min(length, 8)].hex()}"
                  f"  reference {reference[first:first + min(length, 8)].hex()}")
            shown += 1
    print(f"{total} stretches differ")


def main():
    if len(sys.argv) >= 4 and sys.argv[1] == "hashes":
        first_difference(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 4 and sys.argv[1] == "dumps":
        compare_dumps(sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 40)
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
