"""
Name:        coverage.py
Description: runs the shared core's coverage.py (tools/core) for this game: every byte of the 68000 program as code, data or fill, and what is still unknown
Version:     0.1.0 (draft)
Created:     06-11-2026

The script is the core's (arcade-decomp, which tools/core holds); tools/game_profile.py tells it about this game.
Its usage is the core's. Same wrapper as tmntarcade-decomp's tools/coverage.py.

TMNT's version can also be given the bytes the reference read as data (--reads build/scenarios). That needs the
reference, which DonPachi does not have yet (there is no Cave reference), so run it without.
"""
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]
runpy.run_path(str(TOOLS / "core" / "coverage.py"), run_name="__main__")
