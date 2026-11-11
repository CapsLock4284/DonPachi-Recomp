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
| `autoplay FRAMES [SHIELD]` | Plays from the attract mode with a simple player (first ship, sweeping, firing); prints stage changes, deaths, bees, the bosses shot down and the achievements earned, in the order the pop-ups would show them, then the lifetime statistics it made (`autoplay.stats`). `autoplay 15500 1` clears area 1 shielded: AREA 1 CLEAR, UNTOUCHABLE, NO BOMBS NEEDED |
| `training_check AREA PLACE SHIP POWER ...` | Runs training's way to each place (area 0-9; place 0 start, 1 mid-boss, 2 boss, 3 true boss; power 0-3 levels 1-4, 4 on pace, 5 max) and prints where it landed (stage, loop, scroll, power, the enemy bullets and enemies on screen), with a picture just after (`training_N_a.ppm`) and 400 frames later (`training_N_b.ppm`) |
| `pickups AREA` | Fights through an area from its start, collecting items, and logs each power-up taken with its scroll (`SPRITES=1`: each item as it appears). The counts behind training's ON PACE |
| `botcmp AREA SCROLL BOT` | Compares ways of aiming for the mid-boss run: fights from the area's start to the scroll and counts the enemies and bullets left (bot 0 strongest first, the one training uses; 1 nearest; 2 alternating; 3 sweep) |
| `panels_shot AREA FRAMES [TOAST]` (`PLACE=1` from the mid-boss) | A wide-screen picture as the port shows it, panels and game together (`panels.ppm`); with TOAST, an achievement pop-up on it. The README's pictures: `panels_shot 2 1800`, `panels_shot 3 2200` |
| `hitbox_check` | Starts a game and prints each frame the hitbox appears or goes, with a picture as it first appears (`hitbox_N.ppm`): it should wait for the ship to fly in |
| `sfx_log` | Logs each sound effect the game starts (where it is in the effects chip's ROM data, its length), through the attract mode, the ship select and some play: how the menu sounds were found. `CODES=1` also plays a list of the game's sound codes one by one |
| `ui_sounds_wav` | The port's own sounds (menus, achievement) saved as `ui_sound_N_name.wav`, to listen to |
| `ui_sound_compare` | The menu's cursor sound as the board makes it against the port's decoding of it (should match closely) |
| `menu_shot` | Pictures of the main menu, Training, Achievements, Statistics (`STATS=autoplay.stats` to show those), Game Options and Graphics pages, and the options in a training game (`menu_*.ppm`) |
| `midboss_find AREA [FRAMES]` | From an area's mid-boss place (`START=1`: its start), the objects as they appear and those with big hit points: how the mid-bosses were found |
| `midboss_fight AREA` | From an area's mid-boss place, a bot shooting it: its hit records every 30 frames, until it is gone (`RESERVE=frame`: its object and records then, in full) |
| `romdata_check [ZIP]` | The ROM extraction: the .zip's regions saved as data and read back the same, and a damaged copy refused |
| `drop_window_test` | The first run's drop window, with a wrong file then the ROM set dropped on it (run with a desktop, or `xvfb-run -a`) |

`common.h` has what they share: the ROM set's loading, reading the game's memory, the simple player's inputs and
saving pictures. The pictures are PPM files (open them with most viewers, or convert:
`convert panels.ppm panels.png`).
