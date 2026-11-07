"""
Name:        verify_rom.py
Description: runs the shared core's verify_rom.py (tools/core) for this game: checks that the user-provided ROM is the game's complete set
Version:     0.1.0 (draft)
Created:     06-11-2026

The script is the core's (arcade-decomp, which tools/core holds); tools/game_profile.py tells it about this game.
Its usage is the core's. Same wrapper as tmntarcade-decomp's tools/verify_rom.py.
"""
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]
runpy.run_path(str(TOOLS / "core" / "verify_rom.py"), run_name="__main__")
