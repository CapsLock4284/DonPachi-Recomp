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

The sections below: graphics options, the player features, accuracy checks, polish.

## Player features (the owner's ideas)

- [x] **Hitbox dot**: the menu's HITBOX (`[game] hitbox`): each ship's real hitbox as a white glowing box, from the game's own test (`func_0055DA`: a bullet's centre, or an enemy's box, against the box at `+10`-`+16` of the ship's record around its position `+06`/`+08`, in 64ths of a pixel); drawn from the frame of the sprites on screen (a frame back, two with arcade timing) so it stays on the ship as it moves (`ports/sdl2/hitbox.c`)
- [ ] **Practice mode**: pick a part of a stage and warp straight to it (each boss and mid-boss) to practice
  - from the stage script's records and the stage variables (`stage` `101A40`, `scroll_position` `101A56`), or from save states made at those places
- [x] **Fast forward** as a real feature: held, LB or Tab (`[hotkeys]`, remappable in the menu), `fast_forward_speed` 2-16 (4 by default; the menu offers 2x, 3x, 4x, 8x, 16x), no sound meanwhile
- [ ] **Save states**: save and load the game at any point, like an emulator
  - the board's memories plus the decompiled program's own stack (it runs as a coroutine in C): saving that stack works, but a state only loads in the same build of the program; to plan carefully
- [ ] **Cheats in the menu**: turn MAME's cheats (`cheats/donpachi.xml`) on and off while playing
  - while cheats, practice mode or save states have been used, mark the run and keep it off any saved high scores (DonPachi's own high scores are not saved in its EEPROM; this matters once the port keeps scores)

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
  - [x] **Stage and loop** (area 1-10), with the **stage's progress** as a bar to the boss (right panel, under the score graph): `stage` `101A40` (0-4; 5 between the loops and the ending), `loop` `101A42`, `scroll_position` `101A56` (from -1; each boss's object appears at 555, 576, 629, 888, 539 in both loops; area 1 counts from 256, after its fly-in)
  - [x] **Boss health bar**: from the boss's object appearing, the progress bar is its health, all of it together (parts, pools and the reserves that refill them), emptying as it is shot; DEFEATED or TIME UP when it is gone. The objects: 64 of $40 bytes from `$100102` (+C the type, +6 the hit records: $20 bytes, +$18 the hit points). Bosses (type, routine, health): area 1 `31` (`0213B8`: core record 1 + reserve record 2 + parts 3-6), area 2 `58` (`026FA2`: record 0 + reserves at +$58, +$2D6), area 3 `4B` (`03985C`: long at +$30), area 4 `3B` (`024582`: record 0 + reserves object +$32, +$36, records +$3A), areas 5 and 10 `6B` (`03B0F4`: long at +$1C), the true last boss `7C` (`03E5C8`: long at +$1C; a gold bar, "TRUE BOSS")
  - [ ] Mid-boss health too, as an option: area 4's escort `3C` (`0258D4`), area 5's `77` and `8B`
  - [x] **Input display** (left panel, at the bottom; one player only, two leave no room): the stick as a 3 x 3 and the S/B/C buttons, as the board is given them (`cave_board.in0`/`in1`)
  - [x] **Rank** (left panel, as a bar 0-63): the low byte of `$101A32`, worked out again as the stage scrolls from the area, the loop, the time since the last miss, the bees, the lives and each player's power, bombs and best chain
  - [x] **Run statistics** (left panel): time played (the game's mode 7), bombs used (`$106E14` bit 15 rising as the stock goes down), lives lost in each area ("LIVES LOST"; "DEATHS" with two players) (a life lost while the ship is down: lives `$101A2E`/`$101A30`; with two players only the totals), the session's best score (kept through a reset)
  - [x] **Settings in use** (right panel, at the bottom): input timing (or the fast forward speed while held), C AUTO (the EEPROM's "C BUTTON FULL-AUTO", word 4 bit 2), free play, cheats
  - read from the game's memory each frame (score, the high score table, the combo, `stage` `101A40`, `scroll_position` `101A56`); the window gets wider to make room; on or off in the options menu, and which panels on which side

## Graphics

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
