# Test tools

Small programs that run the game without a window to check things: they were used to test training, achievements,
the bees panel, the menus and the ROM extraction, and to make the README's screenshots. They run on Linux (where
Claude's workspace is); on Windows they'd need small changes.

Nothing from the ROM is in here: like the game, they read `rom/donpachi.zip` when they run (or the file named by the
`DONPACHI_ROM` environment variable). They need the decompiled program in `src/` (`python tools/decompile.py`).

## Building

```sh
sh tests/harness/build.sh          # into tests/harness/bin (git-ignored)
```

Needs gcc and SDL2's development files (`sdl2-config`, or set `SDL2_INCLUDE` to SDL's include folder and
`SDL2_LIB` to its library). Run the tools from the project's folder, so `rom/donpachi.zip` is found; what they write
(`.ppm` pictures, test files) goes in the current folder.

## The tools

| Tool | What it does |
|---|---|
| `autoplay FRAMES [SHIELD]` | Plays from the attract mode with a simple player (first ship, sweeping, firing); prints stage changes, deaths, bees and the achievements earned, in the order the pop-ups would show them. `autoplay 15500 1` clears area 1 shielded: AREA 1 CLEAR, UNTOUCHABLE, NO BOMBS NEEDED |
| `training_check AREA PLACE SHIP POWER ...` | Runs training's way to each place (area 0-9; place 0 start, 1 boss, 2 true boss) and prints where it landed (stage, loop, scroll), with a picture 400 frames later (`training_N.ppm`) |
| `panels_shot AREA FRAMES [TOAST]` | A wide-screen picture as the port shows it, panels and game together (`panels.ppm`); with TOAST, an achievement pop-up on it. The README's pictures: `panels_shot 2 1800`, `panels_shot 3 2200` |
| `menu_shot` | Pictures of the main menu, Training, Achievements, Game Options and Graphics pages (`menu_*.ppm`) |
| `romdata_check [ZIP]` | The ROM extraction: the .zip's regions saved as data and read back the same, and a damaged copy refused |
| `drop_window_test` | The first run's drop window, with a wrong file then the ROM set dropped on it (run with a desktop, or `xvfb-run -a`) |

`common.h` has what they share: the ROM set's loading, reading the game's memory, the simple player's inputs and
saving pictures. The pictures are PPM files (open them with most viewers, or convert:
`convert panels.ppm panels.png`).
