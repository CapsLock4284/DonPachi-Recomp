"""
Name:        game_profile.py
Description: what the shared decompiler core (tools/core) needs to know of DonPachi: where the game is, its ROM
             set, and its settings
Version:     0.1.0 (draft)
Created:     06-11-2026

Modelled on tmntarcade-decomp's tools/game_profile.py. Like it, this grows as the steps that need more settings
are reached; until then the core's neutral defaults stand. Memory map from donpachi_map in MAME's cave.cpp.

Items marked TODO are guesses or placeholders to settle by looking at the ROM or the core.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "core"))

import donpachi_rom as rom  # noqa: E402,F401

ROOT = Path(__file__).resolve().parents[1]
GAME = "DonPachi (arcade, Cave 1995)"                  # in the generated files' headers
ROM_SET_TITLE = "DonPachi (donpachi, US, Ver.1.12 1995/05/2x)"
CPU = "68000"
BINARY = "donpachi"
GUARD = "DONPACHI"                                     # the generated headers' include guards
# TODO: the core has kref (Konami) and s32ref (Sega System 32) but nothing for Cave. This is a placeholder until a
# Cave reference exists; steps that need the reference cannot run without it.
REFERENCE = None
ANALYSIS = "m68000.analysis"
DISASSEMBLER = "m68000.disassembler"
# TODO: 000000-0003FF are the vectors; MAME's notes put a version/date string at 000400-00041F. Look at the ROM for
# where the first instruction really is.
CODE_START = 0x000420

# The board as the program's C sees it (memory map: donpachi_map in cave.cpp; an end address is the first byte past)
WORK_RAM_AREA = (0x100000, 0x110000)
PROGRAM_AREA = (0x000000, 0x080000)
HARDWARE = [
    (0x200000, 0x208000, "LAYER1_VRAM"),
    (0x300000, 0x308000, "LAYER0_VRAM"),
    (0x400000, 0x404000, "LAYER2_VRAM"),               # mirrored at 404000-407FFF
    (0x500000, 0x510000, "SPRITE_RAM"),
    (0x600000, 0x600006, "LAYER1_CONTROL"),
    (0x700000, 0x700006, "LAYER0_CONTROL"),
    (0x800000, 0x800006, "LAYER2_CONTROL"),
    (0x900000, 0x900080, "VIDEO_REGS"),                # 900000-900007 also tells the interrupt's cause
    (0xA08000, 0xA09000, "PALETTE"),
    (0xB00000, 0xB00004, "OKI_0"),                     # the two M6295s are on the low byte of each word
    (0xB00010, 0xB00014, "OKI_1"),
    (0xB00020, 0xB00030, "OKI_BANK"),                  # the NMK 112's sample banking
    (0xC00000, 0xC00002, "IN0"),
    (0xC00002, 0xC00004, "IN1"),                       # also carries the EEPROM's data out
    (0xD00000, 0xD00001, "EEPROM_CONTROL"),            # also the coin counters and lockouts
]
# a pointer the code works out into one of the board's devices is the device's, reached through the bus
DEVICE_AREAS = [(start, end) for start, end, _ in HARDWARE]
# reading the interrupt cause does something (the read at 900004 clears the vblank interrupt, the read at 900006 the
# other one): a read there stays where its value goes unused
EFFECT_READ_AREAS = [(0x900000, 0x900008)]
# the board's memories, as game/support.h names them (addresses the code computes into them)
REGIONS = [(0xA08000, 0xA09000, "PALETTE_RAM"), (0x500000, 0x510000, "SPRITE_RAM"),
           (0x200000, 0x208000, "LAYER1_RAM"), (0x300000, 0x308000, "LAYER0_RAM"),
           (0x400000, 0x404000, "LAYER2_RAM")]

# Not here yet, to be settled as the steps that need them are reached (see TMNT's profile for what each does):
# the structures that lie one after another in work RAM (STRUCT_ARRAYS), the frame's wait loop (FRAME_WAIT), the
# routines that work on memory by its address (MEMORY_ROUTINES), supervisor code, the interrupt handlers, and the
# decompiler's feature settings.
