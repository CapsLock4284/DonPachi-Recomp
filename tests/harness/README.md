# Test tools

Small programs that run the game without a window to check things: they were used to test training, achievements,
the bees panel, the menus and the ROM extraction, and to make the README's screenshots. They run on Linux (where
Claude's workspace is); on Windows they'd need small changes.

Nothing from the ROM is in here: like the game, they read `rom/donpachi.zip` when they run (or the file named by the
`DONPACHI_ROM` environment variable). They need the decompiled program in `src/` (`python tools/decompile.py`). With `DONPACHI_SET=japan` any of them runs the Japanese version instead (the `rom` folder's donpachij.zip and donpachi.zip; a build with `src_jpn/`, from `python tools/decompile.py --set japan`), e.g. `DONPACHI_SET=japan slowdown_check 100000` against `tools/cref/cref_timed --rom rom --set japan --frames 100000 --play --waits mame_jp.txt`.

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
| `panels_shot AREA FRAMES [TOAST]` (`PLACE=1` from the mid-boss) | A wide-screen picture as the port shows it, panels and game together (`panels.ppm`); with TOAST, an achievement pop-up on it. `WIDTH=156`: the panels as wide as on a 16:9 screen (120 to 256; 120, the default, is the narrowest) The README's pictures: `panels_shot 2 1800`, `panels_shot 3 2200` |
| `hitbox_check` | Starts a game and prints each frame the hitbox appears or goes, with a picture as it first appears (`hitbox_N.ppm`): it should wait for the ship to fly in |
| `sfx_log` | Logs each sound effect the game starts (where it is in the effects chip's ROM data, its length), through the attract mode, the ship select and some play: how the menu sounds were found. `CODES=1` also plays a list of the game's sound codes one by one |
| `ui_sounds_wav` | The port's own sounds (menus, achievement) saved as `ui_sound_N_name.wav`, to listen to |
| `ui_sound_compare` | The menu's cursor sound as the board makes it against the port's decoding of it (should match closely) |
| `menu_shot` | Pictures of the main menu, Training, Replays (those in `replays/` where it runs; the newest chosen, renamed, deleted), Achievements, Statistics (`STATS=autoplay.stats` to show those), Game Options and Graphics pages, and the options in a training game (`menu_*.ppm`) |
| `replay_check FRAMES` | Records a game of the simple player as a replay (`replays/` where it runs), plays it back from a reset and compares every check and the end (`TAMPER=frame`: playback's input changed there, which should be caught) |
| `training_replay_check AREA PLACE FRAMES` | A training place made, an attempt recorded from it as the port does, kept with SAVE REPLAY, played back (the place made again from the file) and compared; then renamed and deleted (`KEEP=1`: kept; `DEBUG=1`: where it first differs) |
| `kills_check FRAMES` | Autoplays (shielded) and counts the enemies destroyed as the statistics do (`LOG=1` each, `SCORES=1` each score change beside them, `TRACE=frame` the hit records changing for 40 frames from there) |
| `midboss_find AREA [FRAMES]` | From an area's mid-boss place (`START=1`: its start), the objects as they appear and those with big hit points: how the mid-bosses were found |
| `targets_find AREA [MINHP]` | Plays an area (shielded) and logs each enemy destroyed (its type, hit points, place, points, whether a bomb was going off), each item dropped and each life gained (`NOBOMB=1`: never bombs) |
| `targets_hunt AREA [MINHP MAXHP]` | Hunts each tough enemy as it comes (left at 1 hit point, shot from under it, no bombs) and logs what each kill gives (`DELAY=n`: n frames on screen first; `STILL=n`: standing still n frames; `LOOP=1`: the second loop's area): how the extend building's 1-Up and the hover boat's bomb were found |
| `achievement_targets CASE` | Plays one of the area-target achievements both ways and checks it is earned only when it should be (`planes`, `planes_bomb`, `boat`, `boat_early`, `building`, `building_bomb`): prints PASS or FAIL |
| `midboss_fight AREA` | From an area's mid-boss place, a bot shooting it: its hit records every 30 frames, until it is gone (`RESERVE=frame`: its object and records then, in full) |
| `eeprom_check` | The EEPROM's file: MAME's `nvram/donpachi/eeprom` (little endian words) read as is, the port's older files (big endian) still read right, a file saved in MAME's order; and the game reading a MAME file's C BUTTON FULL-AUTO |
| `sound_timing_check` | A command to the sound chips takes effect where it falls in the frame (by the 68000's clock, as MAME's stream update), not from the frame's start; and the attract mode's sound as `sound_timing_new.wav` and `sound_timing_frame.wav` (the old way) to listen to |
| `training_part_way_check [PERCENT ...]` | Training's points part way (START AT 5-95% OF THE WAY) in all 10 areas at each PERCENT (default 10 50 90): reached where the side panel's STAGE PROGRESS shows it, 3 lives, no shield or enemy bullets, still alive 2 seconds on; a replay's place byte read back. PASS or FAIL each |
| `items_probe AREA` | The simple player (unshielded) from AREA's start: each item as it comes (its kind and sprite) and each life lost: how the items' kinds were named |
| `fx_probe AREA` | The explosions (104AD4) and effects (106914) records as they start, their kinds and words, and whether an enemy was just shot down |
| `fx_shot AREA FRAME [FRAME ...]` | Pictures (fx_FRAME.ppm) with the explosions boxed red and the effects green; HITS=1 the enemies' hit boxes too (yellow, the bosses' magenta) |
| `fast_timing_check [FRAMES]` | The picture with fast timing (drawn as the program's frame ends, the scroll and palette put in as the coming vblank interrupt sets them: `ports/sdl2/fast_timing.c`) against arcade timing's, frame by frame (default 6000 frames): the same in every one |
| `japan_check [FOLDER]` | The Japanese program (a build with src_jpn/) from the Japanese set: the set and program's version, a coin and Start and a game played, the same inputs twice giving the same frames, training's way to area 3's boss; pictures japan_warning.ppm, japan_play.ppm, japan_training.ppm. PASS or FAIL each |
| `ai_run [FRAMES]` | The computer's own run, searched not played (save states: each action tried a few frames, the best by score and safety kept, back a few segments at a dead end), no help of any kind; saved as a replay (replays/) and checked: played again from the reset and through REPLAYS, the same board. Stage 1: area 1, no miss. PASS or FAIL |
| `autobomb_check [FRAMES]` | AUTOBOMB (ports/sdl2/autobomb.c) with the simple player: FRAMES of play without it and with it from the same start; the deaths, those with a bomb left, the bombs it set off and its time a frame. With it: no death with a bomb left, fewer deaths; its inputs played back give the same board. PASS or FAIL |
| `replay_crash FILE.inp` | A run's inputs as the window port writes them at a stop or crash (`donpachi_crash_DATE_TIME.inp`, ports/sdl2/crash_log.h) played again from the reset: the same version, EEPROM, slowdown setting, high score table put back and big HIT display; it stops where the run stopped, or says it got to the end, where the game was and the work RAM's sum (`ENDSHOT=FILE`: the last picture) |
| `crash_log_check [FRAMES]` | The crash log's run played again the same: the simple player's game kept as the port keeps it (the table put back, the big HIT display for a stretch; a reset part way in the second), ended by a stop of the 68000 (exit 3) and by a crash (SIGSEGV); each file played by `replay_crash`, the work RAM the same at the end; then a run with a save state put back is marked not replayable. Run where it may write its files. PASS or FAIL |
| `layer_shot AREA FRAMES [FILTER]` (`PLACE=1` from the mid-boss) | GRAPHICS > BY LAYER (ports/sdl2/layer_filter.c) beside the filter over the whole picture (FILTER: mmpx, the default, scale2x or scale3x): `layer_whole.ppm`, `layer_by_layer.ppm` and each one's time a frame; checks that the planes put together at the game's size are video_draw's picture, 200 frames. PASS or FAIL |
| `file_save_check` | The port's files written by file_save's helper thread (ports/sdl2/file_save.h): a newer copy of a file still waiting takes its place, a text file built in memory, a 4 KB save 51 times written whole, and a save keeping its caller under 1 ms. PASS or FAIL |
| `service_probe` | The service switch held from the reset until 0 to 130 frames after the board's self-test, then let go: whether the game comes up in its test mode (the port's SERVICE MENU holds it 60 frames after the self-test) |
| `scaler_shot [AREA FRAMES]` | The FILTER setting's pixel-art scalers (ports/sdl2/scaler.c) over a frame of the game: each one's picture (scaler_NAME.ppm) and its time a frame (the average of 200, the slowest); a picture of one colour kept as it is. PASS or FAIL |
| `version_switch_check [FOLDER]` | The game's version switched while it runs (`program_switch`, as the menu's VERSION does): the US set, the Japanese one over it from donpachij.zip, then the US one again; each from its reset (the boot skip's count), its frames the same as a reset run again, and the US game the same after the round trip. PASS or FAIL each |
| `romset_versions_check [FOLDER]` | The ROM set's two versions (`rom_set_load_version`): the US set and the Japanese one (donpachij.zip with donpachi.zip, its parent) from FOLDER (default `rom`), from donpachij.zip named alone, from a merged .zip (made here with zip and unzip), refused without the parent; each version's extracted data saved, read back the same, and refused as the other's. PASS or FAIL each |
| `romdata_check [ZIP]` | The ROM extraction: the .zip's regions saved as data and read back the same, and a damaged copy refused |
| `drop_window_test` | The first run's drop window, with a wrong file then the ROM set dropped on it (run with a desktop, or `xvfb-run -a`) |

`common.h` has what they share: the ROM set's loading, reading the game's memory, the simple player's inputs and
saving pictures. The pictures are PPM files (open them with most viewers, or convert:
`convert panels.ppm panels.png`).
