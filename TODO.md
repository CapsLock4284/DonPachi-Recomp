# DonPachi port: to-do

The plan, roughly in order. Tick items off (`[x]`) as they are done.

## Done

- [x] Decompile the 68000 program (`tools/decompile.py`, `tools/call_tables.py`) and run it on the board model (`runtime/`)
- [x] Picture: the three tile layers, the sprites, the palette (after MAME's tmap038, sprite013, cave_v)
- [x] Full speed (frame wait at `0x2ED6`) and the board's 57.55 Hz pacing
- [x] Reference runs on MAME's 68000 (`tools/cref`, `tools/cref/compare.py`): matches the original frame for frame through both loops
- [x] Stage scripts: all of a frame's commands run (`pea` return addresses; fixed the stage 1 forest ground)
- [x] Cheats from MAME's cheat file (`--cheat`, `--list-cheats`)
- [x] Sound: two OKI M6295s and the NMK112 (`runtime/src/sound.c`), SDL audio, `--wav` on the headless port
- [x] EEPROM saved beside the program (`donpachi.eeprom`, MAME's format), `--eeprom`
- [x] Input delay measured (`tools/latency.py`); fast timing by default, `--arcade-timing` for the board's
- [x] Home-friendly defaults: a new `donpachi.eeprom` has "C BUTTON FULL-AUTO" on (EEPROM word 4 bit 2, word 1 as the test mode writes it)
- [x] Free play (DonPachi has no such setting): Start drops in a coin first (`runtime/src/free_play.c`); `--no-free-play` for the board's coins
- [x] Gamepads (SDL game controllers, hot-plugging, two players): Xbox layout X shot/laser, A bomb, B button C (rapid fire), Start, d-pad and left stick; View kept for the options menu
- [x] Settings file `donpachi.ini` beside the program (free play, timing, window, volume, files) and remapping of every key and gamepad button for both players (`ports/sdl2/settings.c`)
- [x] Game reset (`program_reset`: the board's reset, the EEPROM kept; identical to a fresh start, checked frame by frame): F3, and F4 "service menu" (the reset with the service switch held for you); both bindable to gamepad buttons in `[hotkeys]`
- [x] Options menu (`ports/sdl2/menu.c`): View or Escape opens it over the paused game, in the game's own font; resume, reset, service menu, free play, input timing, volume, window size, full screen, controls (remap by pressing the new key or button, per player), quit; changes saved to `donpachi.ini` at once; buttons held as it closes are ignored until let go
- [x] High scores kept in `donpachi.hiscore` beside the program (`ports/sdl2/hiscore.c`; the board keeps only its settings and starts from the default table each time): the table `$101846`-`$10189F` (scores, names, a word each for the entries) and the HUD's HIGH `$101782` put back over the defaults just after the game sets them up, then saved when it changes; not saved while cheats are on; SAVE HIGH SCORES in the options menu, `[game] save_high_scores`

## Next

- [x] **Training: a loading message**: START TRAINING shows LOADING... for a frame (on the screen) before the game is run there
- [x] **Training: the hitbox shown by default** in a training game: the Training page's HITBOX (`[training] hitbox`, on); the normal game keeps the options' HITBOX
- [x] **Training bug: area 6** (the picture wobbling, the ship flying from the start though the dropship comes in): area 6 was entered as a mere next area from area 1's end, the loop set by hand. Now area 1's end is made area 5's (the stage set to 4, loop 0), so the game goes through the first loop's end, its story between the loops, and starts area 6 itself, from the dropship

The sections below: graphics options, the player features, accuracy checks, polish.

## Player features (the owner's ideas)

- [x] **Difficulty in the options** (GAME OPTIONS): EASY / NORMAL / HARD / VERY HARD, the service menu's own setting (EEPROM word 4, bits 8-9); applies on an F3 reset or a restart
- [x] **ROM extraction on first run** (like other recomps): the first run reads the ROM set (`[files] rom`, `rom\donpachi.zip` by default) and saves its contents to `donpachi_usa.dat` beside the program (12.8 MB: a header, then the regions as `rom_set` holds them, checked by CRC32; written to `.part` first, then renamed). From then on the program reads that and the .zip isn't needed. A damaged file falls back to the .zip (and is written again); `--rom` on the command line reads and extracts again. `rom_set_save_data`/`rom_set_load_data` (`runtime/src/rom_set.c`). `*.dat` is in .gitignore. The main menu says ROM DATA EXTRACTED the first time
  - Checked: the data read back is identical to the .zip's, region by region; a damaged byte is refused; a merged-style .zip (the files in subfolders, with other versions' files beside them) gives the same set, since each file is found by its CRC32
  - [x] **VERSION** on the main menu (above START GAME): USA. Changing it says JAPAN VERSION: NOT YET
  - [ ] Japan: its own data file (`donpachi_jpn.dat`, header version 1) extracted from `donpachij`'s program ROM plus the shared chips. MAME: `donpachi` (US) is the parent, `donpachij` a clone (check), so a **merged** `donpachi.zip` has both and a **split** `donpachij.zip` needs `donpachi.zip` too. Then VERSION switches between those found, greyed when not extracted
- [ ] **AI second player** (just for fun; the idea comes from the Xbox 360 version of DoDonPachi SaiDaiOuJou): player 2 flown by the computer beside you. It would feed player 2's inputs (`cave_board.in1`) each frame from a simple bot that reads the game's memory: the object list at `$100102` for enemies and bullets, the ship record for its position. It would dodge, keep firing and follow player 1. Needs a second credit and P2 Start; no high scores from those games
- [ ] **Achievements** (low priority): goals kept in a file (clear an area with no miss, a chain of N, the true boss, ...), shown as they are earned
- [ ] **Online leaderboard** (low priority; only with free hosting): scores sent from unassisted games (no cheats, save states or training), with the run's details

- [x] **Hitbox dot**: the menu's HITBOX (`[game] hitbox`): each ship's real hitbox as a white glowing box, from the game's own test (`func_0055DA`: a bullet's centre, or an enemy's box, against the box at `+10`-`+16` of the ship's record around its position `+06`/`+08`, in 64ths of a pixel); drawn from the frame of the sprites on screen (a frame back, two with arcade timing) so it stays on the ship as it moves (`ports/sdl2/hitbox.c`)
- [x] **Main menu** (the title screen, as the program starts; `[game] main_menu`): START GAME (the arcade game from its reset), TRAINING, SETTINGS (the options that can be set before a game: timing, fast forward, hitbox, volume, game options, graphics, controls), QUIT TO DESKTOP; the in-game options have MAIN MENU (the game reset, back to it)
- [x] **Training** (the main menu's TRAINING; `ports/sdl2/training.c`, `[training]`): AREA 1-10, START AT (stage start, boss, area 10's true boss), SHIP (type A/B/C), POWER (normal/max), BOMBS (3, or 7 in 7 slots: `101A8A` the slots, the ship's +$24/+$22, the HUD drawn by the game's `func_005020`), RETRY ON MISS. The training game ends at the area's BONUS screen (the stage moves on) or a game over: back to the training page; the in-game options have TRAINING MENU. The game is run there unseen in about a second: from the reset, a coin and Start, the ship chosen on its select screen; area 1 played (shielded, not firing) to its boss's end, where the stage (`101A40`, as the game adds 1 to it) and loop are set to the place's, so the next area is the place's, entered as the game enters it (the HUD, the ship's launch); a boss: on, shielded, until its WARNING starts (object `3F`, made as the WARNING begins: scroll 580, 550, 550, 854, 536 in both loops; the true boss: its own object `7C`, area 10's first boss shot down on the way). There: score 0 (the HUD's digits made again, `func_006C24`'s lists at `101786` and `1017C6`), the chain cleared, 3 lives and bombs, the power chosen, the HIGH back to the table's #1. A save state is made there; a miss goes back to it 45 frames later (RETRY), F5 keeps another place. No high scores
  - [x] **DIFFICULTY on the Training page** (the owner; `[training] difficulty`, NORMAL by default): its own setting, for training only. `training_prepare` puts it in the EEPROM for the training game's boot, then puts the player's own straight back, so the settings file, the game options and the save state made at the place never get it. Checked: the game read 0-3 for each choice (`$101284`), and the EEPROM came back to its own value each time
  - [ ] **Mid-boss as a START AT place** (the owner): a MID-BOSS choice beside STAGE START and BOSS. Mid-bosses are just tough enemies in the level, with no WARNING screen or sound to stop at, so each area's start point needs finding by hand: play each area, note where it appears, then match that to its object type and scroll position (`101A56`). Known so far: area 4's escort `3C`, area 5's `77` and `8B`; the others to find
  - [ ] More places: a point by the stage's scroll position (`101A56`), as a percentage of the way to the boss
  - from the stage script's records and the stage variables (`stage` `101A40`, `scroll_position` `101A56`), or from save states made at those places
- [x] **Fast forward** as a real feature: held, LB or Tab (`[hotkeys]`, remappable in the menu), `fast_forward_speed` 2-16 (4 by default; the menu offers 2x, 3x, 4x, 8x, 16x), no sound meanwhile
- [x] **Save states** (in memory, for the session): F5 saves, F8 loads (`[hotkeys] save_state`, `load_state`, gamepad buttons bindable on the Controls page; SAVE STATE / LOAD STATE in the options menu), one slot, a notice on screen. `program_state_save`/`program_state_load` (`runtime/src/game_program.c`): the whole board (`cave_machine`), the 68000's status, the registers `next_frame` keeps with `setjmp` before it hands the frame over, and the game task's stack from there up to `run_program`'s frame; put back on the game task below the state's stack (`alloca`), then `longjmp` (MSVC: `Frame` 0, no unwinding). Checked frame by frame against the run it was saved in (the board's hash), loaded from later in the game and from just after a reset. The side panels' run goes back with it; a game that loaded a state keeps no high score (the file's table is put back when the next game starts)
  - [ ] More than one slot (for practice mode's places)
  - [ ] States in files, across runs: the stack holds the program's addresses, which change from run to run (Windows' ASLR); would need the stack rebuilt instead of copied
- [x] **Cheats in the menu**: GAME OPTIONS > CHEATS turns the cheat file's cheats on and off while playing (for the run, not saved; `cave/cheats.h`: `cheats_load`, `cheats_set`; the file read once into a table of cheats and their writes). Those that patch the program or need a value are left off the page ("Select Area to follow Area 1"). A game that had a cheat on, or went back to a save state, keeps no high score; the settings panel shows CHEATS ON
  - [ ] "Select Area to follow Area 1" in RAM terms (it patches the program's code): practice mode will cover it

- [ ] **Side panels ("widgets")** (`ports/sdl2/widgets.c`; the GRAPHICS page's SIDE PANELS, `[video] side_panels`): the empty space left and right of the picture on a wide screen, filled with information about the game being played, drawn as nice panels (like the M2 ShotTriggers ports' gadgets)
  - [x] **Score graph** against the #1 high score as the run goes on, with the stage marks and a percentage (as the RANKGRAPH picture: the run's line, the 50% and 100% lines, "ST" to "NOW")
  - [x] **Chain gauge** (the Get Point System): the time left to keep the chain going, as a bar that empties over the 30 frames (0.52 s) after each kill with the frames left counted down inside it, the hits and the chain's points so far; player 2's only when there is a player 2
  - [x] **Hit combo counter**, with the run's best combo (in the chain gauge: the hits large, MAX the best)
  - The game's chain, found (`func_0066C4` scores a kill, `006E3A` counts down once a frame), player 1 / player 2:
    - `$101742` / `$101760`: the chain timer, set to 30 on each kill, down 1 a frame; a kill while it is above 0 goes on with the chain, at 0 the chain ends
    - `$10174E` / `$10176C`: the hits of the chain (cleared 80 frames after the last kill: `$101744` / `$101762`)
    - `$101754` / `$101772`: the best chain (the TOP PLAYERS' "MAX GP COUNTS")
    - `$10174A` / `$101768`: the chain's points, the HUD's "+N GP" (BCD); `$101746` / `$101764`: the sum of the base values that builds them; `$10173A`: the last kill's base value
    - `$10177A` / `$10177E`: the score (BCD); `$101782`: the HUD's HIGH; `$101846`: the high score table's #1 (then #2-#5)
    - `$1018EC`: the game's mode (0xB ship select, 7 play, 0/1/5 the attract mode and its demonstration)
  - [ ] **Lives and bombs** left
  - [x] **Bees collected** (left panel, above the input display; one player only, as two leave no room): the area's 13 bees (the shmups wiki: 13 in each stage) as two rows of icons, greyed until taken, then in the bee's own colours. Counted from the game's own count of bees taken in the area (`$10379A` P1, `$10379E` P2; `func_008162` adds 1 as one is taken; a miss and the area's end set it back to 0, so the panel adds up the rises itself and keeps them through a death). The icon is the game's bee sprite read from the ROM at runtime (code `C2E`, 32 x 32, palette `3B`), turned as the picture is and drawn at half size: no image in the repo. The bee items are objects of type `16`
  - [x] **Stage and loop** (area 1-10), with the **stage's progress** as a bar to the boss (right panel, under the score graph): `stage` `101A40` (0-4; 5 between the loops and the ending), `loop` `101A42`, `scroll_position` `101A56` (from -1; each boss's object appears at 555, 576, 629, 888, 539 in both loops; area 1 counts from 256, after its fly-in)
  - [x] **Boss health bar**: from the boss's object appearing, the progress bar is its health, all of it together (parts, pools and the reserves that refill them), emptying as it is shot; DEFEATED or TIME UP when it is gone. The objects: 64 of $40 bytes from `$100102` (+C the type, +6 the hit records: $20 bytes, +$18 the hit points). Bosses (type, routine, health): area 1 `31` (`0213B8`: core record 1 + reserve record 2 + parts 3-6), area 2 `58` (`026FA2`: record 0 + reserves at +$58, +$2D6), area 3 `4B` (`03985C`: long at +$30), area 4 `3B` (`024582`: record 0 + reserves object +$32, +$36, records +$3A), areas 5 and 10 `6B` (`03B0F4`: long at +$1C), the true last boss `7C` (`03E5C8`: long at +$1C; a gold bar, "TRUE BOSS")
  - [ ] Mid-boss health too, as an option: area 4's escort `3C` (`0258D4`), area 5's `77` and `8B`
  - [x] **Input display** (left panel, at the bottom; one player only, two leave no room): the stick as a 3 x 3 and the S/B/C buttons, as the board is given them (`cave_board.in0`/`in1`)
  - [x] **Rank** (left panel, as a bar 0-63): the low byte of `$101A32`, worked out again as the stage scrolls from the area, the loop, the time since the last miss, the bees, the lives and each player's power, bombs and best chain
  - [x] **Run statistics** (left panel): time played (the game's mode 7), bombs used (`$106E14` bit 15 rising as the stock goes down), lives lost in each area ("LIVES LOST"; "DEATHS" with two players) (a life lost while the ship is down: lives `$101A2E`/`$101A30`; with two players only the totals), the session's best score (kept through a reset)
  - [x] **Settings in use** (right panel, at the bottom): input timing (or the fast forward speed while held), C AUTO (the EEPROM's "C BUTTON FULL-AUTO", word 4 bit 2), free play, cheats
  - read from the game's memory each frame (score, the high score table, the combo, `stage` `101A40`, `scroll_position` `101A56`); the window gets wider to make room; on or off in the options menu, and which panels on which side

## Graphics

- [x] **Borderless or exclusive full screen** (GRAPHICS > FULL SCREEN: OFF / BORDERLESS / EXCLUSIVE; `[window] fullscreen`, `fullscreen_type`): borderless is a window covering the desktop (it was already this, SDL's FULLSCREEN_DESKTOP); exclusive takes the display at the desktop's own mode (no resolution or refresh change). F11 / Alt+Enter switches between the window and the chosen kind

- [x] The options menu's GRAPHICS page and `[video]` (`ports/sdl2/main.c` show_picture):
  - [x] Scaling: fit (as large as the window allows), integer (whole times only, every game pixel the same size), stretch
  - [x] Smoothing: a soft filter, or sharp pixels
  - [x] Scanlines: off, light, medium, heavy; up and down the picture, as the cabinet's monitor stands on its side (across it when the picture is rotated)
  - [x] Vsync on (no tearing) or off (the least delay)
  - [x] Rotate (TATE): the picture turned right or left for a monitor on its side
  - [x] Window size and full screen moved to the page
- [ ] A CRT shader (curvature, glow, a shadow mask), beyond the scanlines

## Other versions

The last step: once the US version plays and works the way it should.

- [ ] **DonPachi (Japan)**, MAME's `donpachij` (Ver. 1.01, 1995/05/11): the original release, with the smallest hitbox (9x10 against the US version's 11x14)
  - its own program ROM (the other chips are shared with the US set): a second decompile of it, and the runtime choosing the program by the ROM set the player has (`rom_set.c` checks the CRCs)
  - the hitbox dot needs nothing: it reads the box the game sets up
  - later, perhaps the other versions the same way: Korea (`donpachikr`), Hong Kong (`donpachihk`, the largest hitboxes), Japan Satsuei (`donpachijs`)

## Accuracy, to check later

- [ ] Fast timing: sprites are a frame ahead of the background scroll compared with the board (at most ~1 pixel against the ground); arcade timing is exact
- [ ] Arcade timing: MAME draws 90 us into the vblank interrupt; we draw when it returns. Same except perhaps during a large palette fade (one frame)
- [ ] Sound: our resampling to 48 kHz is simpler than MAME's (straight lines between samples); compare against MAME recordings
- [ ] Sound: OKI commands take effect at the end of the frame (up to ~17 ms late; the reference runs agree)
- [ ] Check that MAME's `nvram/donpachi/eeprom` loads as is
- [ ] OKI status read: upper byte returns 0; check against MAME's unmapped-lane value

## Clean-up and polish

- [x] Debug keys removed (V autofire, Tab toggle): C is the arcade's own rapid fire
- [ ] App icon and window title
- [ ] Volume control (in the settings file and the options menu)
- [ ] Release build: the exe plus `SDL2.dll`, a short README for players (bring your own `donpachi.zip`)
- [ ] Name more functions and RAM in `symbols.txt` (sound driver, enemy scripts, player state)
- [ ] Startup message box listing what the ROM set is missing, when it is incomplete

## Future ideas

- [ ] A shared runtime / launcher for other first-generation Cave 68000 games (DoDonPachi, ESP Ra.De., Guwange, ...)
- [ ] **The developers' own debug tools, still in the ROM** (look into): switched off in the released game, but they come back with a few patches (sudden-desu.net, "DonPachi debug tools and more": https://sudden-desu.net/entry/donpachi-debug-tools-and-more/)
  - a RAM viewer and editor (addresses and values at the bottom of the screen; flag byte `$1018E8`)
  - a stage select (stages 2-5) and a pause with stage explore (P2 Start pauses; P1 button C with up and down scrolls the stage)
  - a palette editor, an object spawner, the sound test's auto-play
  - the US version's patch points: `0x35D98` (the second group of tools), `0x34334`/`0x34338`/`0x34420`/`0x34424` (skip the RAM/ROM check); the flags `$10F000` (pause and stage explore) and `$10F001` (palette editor, object spawner, stage select)
  - would be an option (off by default) that patches the program or sets the flags, never touching the normal game; useful for finding things in memory and for practice mode
