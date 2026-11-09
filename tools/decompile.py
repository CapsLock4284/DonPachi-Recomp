"""
Name:        decompile.py
Description: runs the shared core's decompile.py (tools/core) for this game: the 68000 program as readable C in
             src/, with the names of symbols.txt
Version:     0.2.0 (draft)
Created:     07-11-2026

The script is the core's (arcade-decomp, which tools/core holds); tools/game_profile.py tells it about this game
(DECOMPILER: the 68000's driver, m68000/decompile.py; the board's memory map, its interrupt handler's frame).
Its usage is the core's:
  python tools/decompile.py            src/ (game.h, ram.h, table.c and the functions' files)
  python tools/decompile.py --check    what does not lift
Same wrapper as tmntarcade-decomp's tools/decompile.py, with one addition: tools/call_tables.py teaches the core's
68000 analysis the calls and jumps DonPachi makes straight into tables of stubs, which the core does not follow
(it is installed here, before the core runs; tools/core itself is not changed). DonPachi has no runtime yet (there
is no Cave board model), so the C it writes has nothing to run on.
"""
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import call_tables  # noqa: E402

call_tables.install()
runpy.run_path(str(TOOLS / "core" / "decompile.py"), run_name="__main__")
