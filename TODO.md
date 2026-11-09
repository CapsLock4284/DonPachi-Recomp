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
- [x] Debug keys: V autofire, Tab fast forward (`DONPACHI_DEBUG_KEYS`)
- [x] Home-friendly defaults: a new `donpachi.eeprom` has "C BUTTON FULL-AUTO" on (EEPROM word 4 bit 2, word 1 as the test mode writes it)
- [x] Free play (DonPachi has no such setting): Start drops in a coin first (`runtime/src/free_play.c`); `--no-free-play` for the board's coins
- [x] Gamepads (SDL game controllers, hot-plugging, two players): Xbox layout X shot/laser, A bomb, B button C (rapid fire), Start, d-pad and left stick; View kept for the options menu
- [x] Settings file `donpachi.ini` beside the program (free play, timing, window, volume, files) and remapping of every key and gamepad button for both players (`ports/sdl2/settings.c`)
- [x] Game reset (`program_reset`: the board's reset, the EEPROM kept; identical to a fresh start, checked frame by frame): F3, and F4 "service menu" (the reset with the service switch held for you); both bindable to gamepad buttons in `[hotkeys]`
- [x] Options menu (`ports/sdl2/menu.c`): View or Escape opens it over the paused game, in the game's own font; resume, reset, service menu, free play, input timing, volume, window size, full screen, controls (remap by pressing the new key or button, per player), fast forward (debug), quit; changes saved to `donpachi.ini` at once; buttons held as it closes are ignored until let go

## Next

The sections below: graphics options, the player features, accuracy checks, polish.

## Player features (the owner's ideas)

- [ ] **Hitbox dot**: a white glowing dot over the player's ship showing its real hitbox, as in Touhou or Mushihimesama
  - find the hitbox's size and offset in the game's collision code; the port draws the dot over the picture at the ship's position (player 1 at `102158`, player 2 at `102198`), on or off in the options menu
- [ ] **Practice mode**: pick a part of a stage and warp straight to it (each boss and mid-boss) to practice
  - from the stage script's records and the stage variables (`stage` `101A40`, `scroll_position` `101A56`), or from save states made at those places
- [ ] **Fast forward** as a real feature (not only the debug key): a hotkey and a gamepad button, a speed choice, sound muted or sped up
- [ ] **Save states**: save and load the game at any point, like an emulator
  - the board's memories plus the decompiled program's own stack (it runs as a coroutine in C): saving that stack works, but a state only loads in the same build of the program; to plan carefully
- [ ] **Cheats in the menu**: turn MAME's cheats (`cheats/donpachi.xml`) on and off while playing
  - while cheats, practice mode or save states have been used, mark the run and keep it off any saved high scores (DonPachi's own high scores are not saved in its EEPROM; this matters once the port keeps scores)

## Graphics

- [ ] Window and fullscreen scaling: any window size and fullscreen, keeping the picture's aspect
- [ ] Integer scaling (sharp pixels, no uneven rows)
- [ ] Optional scanline / CRT filter
- [ ] Vsync option (no tearing) vs. lowest delay (no vsync)
- [ ] Rotated-monitor (TATE) option: show the 320 x 240 picture unrotated for a monitor turned on its side

## Accuracy, to check later

- [ ] Fast timing: sprites are a frame ahead of the background scroll compared with the board (at most ~1 pixel against the ground); arcade timing is exact
- [ ] Arcade timing: MAME draws 90 us into the vblank interrupt; we draw when it returns. Same except perhaps during a large palette fade (one frame)
- [ ] Sound: our resampling to 48 kHz is simpler than MAME's (straight lines between samples); compare against MAME recordings
- [ ] Sound: OKI commands take effect at the end of the frame (up to ~17 ms late; the reference runs agree)
- [ ] Check that MAME's `nvram/donpachi/eeprom` loads as is
- [ ] OKI status read: upper byte returns 0; check against MAME's unmapped-lane value

## Clean-up and polish

- [ ] Remove the debug keys (V autofire, Tab fast forward) once the options menu exists; C stays the arcade's own rapid fire
- [ ] App icon and window title
- [ ] Volume control (in the settings file and the options menu)
- [ ] Release build: the exe plus `SDL2.dll`, a short README for players (bring your own `donpachi.zip`)
- [ ] Name more functions and RAM in `symbols.txt` (sound driver, enemy scripts, player state)
- [ ] Startup message box listing what the ROM set is missing, when it is incomplete

## Future ideas

- [ ] A shared runtime / launcher for other first-generation Cave 68000 games (DoDonPachi, ESP Ra.De., Guwange, ...)
