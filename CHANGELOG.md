# Changelog

What has changed in each release of DonPachi Recomp, newest first. Changes made since the last release go under
**Unreleased**; when a release is made, that section becomes its version (and is pasted into the GitHub release's
notes). The release zip carries this file as `CHANGES.txt`.

## Unreleased

### New
- **C AUTO FIRE** in GAME OPTIONS: button C's rapid fire on or off (the arcade's test mode setting "C BUTTON
  FULL-AUTO", kept in the EEPROM as before; on by default). Changed there, it is in use at once, no reset needed;
  a replay keeps the change.
- **ARCADE SETTINGS** in GAME OPTIONS, in place of SERVICE MENU: the arcade test menu's other settings in the
  port's own menu, no trip to the test menu needed. CONTINUE (on or off), CONTINUE COST (normal or discount), LIVES
  (1, 2, 3 or 5), EXTEND (2M only; 1M and 6M; 5M, 8M, 11M and on; none), COIN A and COIN B (the credits a coin
  gives) and DEMO SOUND. Kept in the EEPROM as the test menu keeps them; in use from the next reset (F3) or start.
  The test menu's INVERT SCREEN is left out (ROTATE does that). F4 still opens the test menu itself.

### Fixed
- **F4** (the arcade's test menu) went back to the game instead since v0.10.0: the service switch was let go during
  the board's self-test, which is run at once since then. It is now held until 4 seconds after the self-test.

## v0.12.0 (2026-11-21)

### New
- **Bullet points** (GAME OPTIONS > BULLETS, `[game] bullets`, off by default): each enemy bullet's point, a white
  pixel in a small green cross. The game tests a bullet only by that point, the middle of its sprite, so a bullet
  hits you only when its point gets inside your hitbox, however big it looks: with HITBOX on you see exactly what
  can hit you. Drawn over the picture only; the game is not changed.
- **Both versions from the first run**: a merged `donpachi.zip` (with the Japanese set's files in it) or `donpachij.zip`
  beside it now gives the Japanese version's data on the first run too, nothing more asked; and when the first run's
  window has read `donpachi.zip`, it asks for `donpachij.zip` as well (Enter or a click skips it). So choosing JAPAN on
  the main menu no longer opens a window over it, unless the Japanese set was skipped.
- **Faster xBR filters**: xBR 2x, 3x and 4x take about a quarter less time a frame (the same picture, pixel for
  pixel), and all the filters now share their work across up to 8 of your processor's cores instead of 4.
- **FPS meter** (F9, or SETTINGS > GRAPHICS > FPS METER), as DoDonPachi Recomp's: the game's frames a second, the
  board's, the pictures shown, the frames your PC ran late and its time a frame, on the right side panel (never over
  the game; in the picture's corner without the panels). While it is on, each second goes into `donpachi_fps.log`
  with the time each part of a frame takes, to send if the game ever stutters.
- **FPS on the right panel** in place of the slowdown's percentage: the game's frames over the last second (57.5 at
  full speed, lower and yellow while the arcade's slowdown is on).
- **The high score chase on the score graph**, as DoDonPachi Recomp has: the table's #5 is chased first, then #4,
  up to #1 ("NEXT #3" and its score under the graph). The place being chased is a dashed line on the graph, and each
  one you pass leaves a line in its colour where you passed it, with its number.
- **Bullets on screen** on the right side panel, in place of C AUTO (the C button's auto fire is how it is played
  anyway): the number of enemy bullets the game is testing against your ship, yellow from 50.
- **Filters by layer** (SETTINGS > GRAPHICS > BY LAYER, `[video] filter_layers`, off by default): the FILTER now
  runs on each of the game's layers and its sprites on their own, then puts them back together the way the arcade
  board does. Sprite outlines stay clean instead of smearing into a busy background, and lines that cross keep their
  shape. Works with Scale2x, Scale3x and MMPX (xBR mixes colours, so it stays on the whole picture). It costs a few
  more milliseconds a frame; the game itself is unchanged.
- **The side panels fill a wide screen**: they used to be a fixed width, leaving black bars at the sides of a 16:9
  screen. Now they widen to fill it (the game's picture keeps its size and shape, nothing is stretched): the bars,
  the score graph and the rows of boxes spread out to the new width. A very wide screen still keeps some black.
- **A crash report you can send**: if the game ever stops by itself, a message now says so (Windows) instead of the
  window just closing, and two files are left beside the program: `donpachi.log` (what happened) and
  `donpachi_crash_DATE_TIME.inp` (your inputs since the last reset, so the run can be played again up to the crash and
  the problem found). Sending both with a report helps. The log of the run before is kept as `donpachi_previous.log`.

### Fixed
- **A short hitch about once a minute** in play: the lifetime statistics were saved every minute in the middle of
  a frame, and on some PCs writing the file takes 10-20 ms (once over 100 ms, a visible stutter). The port's files
  (statistics, achievements, high scores) are now written by a helper thread, so the game never waits for the
  disk.

## v0.11.0 (2026-11-20)

### New
- **Separate volumes for the music, the sound effects and the voices** (SETTINGS > SOUND, `[sound]` in
  `donpachi.ini`): VOLUME sets all of it as before; MUSIC, EFFECTS and VOICES set each on its own, under it, in
  steps of 5%. While you change one, an example plays at the level you are setting (a piece of a stage's music, shots
  and an explosion, a voice), over and over until you move off it. Players asked for it. The game itself is
  untouched: only what you hear changes (checked: the board the same, frame by frame, whatever the levels; at 100%
  the sound the same as before, sample for sample).

### Fixed
- **A crackle in the sound during a stage with autobomb on.** Autobomb looks a few frames ahead every frame to see
  whether a ship is about to be hit, then puts the board back; those frames' sound was thrown away, but the sound's
  own filters went on to where the look ahead had got, so each real frame started with a jump: a crackle whenever
  something was playing. The look ahead's frames now leave the sound alone (checked: with autobomb on, the sound is
  now the same as with it off).
- **The SOUND entry in SETTINGS had no ">"** to show it opens a page, as the others do.

## v0.10.0 (2026-11-18)

### New
- **Updates from inside the game**: as it starts, the game looks up the newest release on GitHub (in the
  background; the game doesn't wait). If there is one, the main menu shows **UPDATE TO V...** at its top in blue;
  choosing it asks first, then downloads the release (checked against the SHA-256 GitHub publishes for it), puts
  the new files in place and starts the game again. Your settings, high scores, replays, achievements, statistics
  and the extracted ROM data are kept. GAME OPTIONS > CHECK FOR UPDATES (`check_updates` in `donpachi.ini`) turns
  the look-up off. This release is the last one you need to download by hand: from here on the game updates itself.
- **Less display lag on Windows** (GRAPHICS > RENDERER, `[video] renderer`): the picture now reaches the screen
  through Direct3D 12 by default. Before, SDL chose Direct3D 9, which in a borderless window waits for Windows'
  compositor: 27 ms from a finished frame to the screen on the owner's PC, against 1.7 ms now (measured with
  PresentMon; the README has the table). RENDERER can also pick direct3d, direct3d11, opengl or software, used from
  the next start; `donpachi.log` names the one in use. With vsync off you may now see tearing (Windows' compositor
  hid it); VSYNC ON stops it for a little delay.
- **Faster picture drawing**: the board's picture is now drawn in about a quarter of the time (the layers in one
  pass, the sprites clipped once), which leaves more of each frame to spare on slower PCs. The pictures are the same,
  checked frame by frame.

### Behind the scenes
- The board runtime (`runtime/`) is now the separate Cave68kSDK project, included as a git submodule
  (https://github.com/CapsLock4284/Cave68kSDK), so that other Cave 68000 games can share it. DonPachi is described
  there as data (its memory map, ROM sets, picture and sound settings). The game is unchanged: the board, sound and
  picture were checked identical over 30,000 frames of play in both versions, and the extracted data file is the
  same byte for byte, so existing installs keep working. Clone with `--recursive`, or run
  `git submodule update --init` in an existing clone.

## v0.9.0 (2026-11-15)

### New
- **Autobomb** (GAME OPTIONS > AUTOBOMB, off by default): when your ship is about to be hit, a bomb goes off by
  itself, as in many newer shooters. It only fires if you have a bomb left and the bomb will really save you: the
  game looks a few frames ahead each frame to check. It is an assist, so games played with it on don't count for
  high scores, achievements or statistics. Replays keep its bombs. It sets off a normal bomb, not the laser bomb you
  get by holding the shot into it: it lets go of the shot for one frame first, and only falls back to the laser bomb
  when the hit is too close for that.
  The side panel's settings list shows AUTOBOMB ON or OFF.
- **donpachi.log** beside the program on Windows: the game's messages, and if it ever closes by itself, why (send it
  with a bug report).

### Fixed
- The statistics' BOMBS USED and BOMB ITEMS never counted, and the NO BOMBS NEEDED achievement could be earned while
  bombing: the bomb count was read from the wrong half of its memory word.

### Behind the scenes
- 158 more names in the decompiled code: the routine of each of the game's 167 object types (enemies, their parts and
  effects), named after its type and the areas it was seen in, with the three already known (the heavy planes, the
  hover boat, the extend building); and the enemies' parts' script commands, by what each does. The game itself is
  unchanged.
- A first step towards a run played by the computer alone, for the best score: a test tool that searches the game
  (no cheats or help of any kind) and saves what it finds as a replay to watch in REPLAYS. So far it clears area 1
  without a miss, taking 8 of its 13 bees, for 476,510 points.

## v0.8.0 (2026-11-14)

### New
- **The Japanese version:** the original Japanese release of DonPachi (MAME's `donpachij`, Ver. 1.01), with its
  smaller hitbox, can now be played. Choose it with VERSION on the main menu; the first time it needs MAME's
  `donpachij.zip` (beside `donpachi.zip` in the `rom` folder, or dropped on the window that asks), and keeps what it
  needs as `donpachi_jpn.dat`. It runs the Japanese program itself, decompiled like the US one and checked frame for
  frame against MAME. Training, replays, the side panels, cheats and the AI player all work on it. Each version keeps
  its own high scores and arcade settings; achievements and statistics are shared. Replays remember their version.
  The game starts in the version you chose last (`--japan` or `--usa` for one run).
- **Pixel-art filters:** SETTINGS > GRAPHICS > FILTER smooths the picture's edges while keeping the pixel art sharp:
  Scale2x and Scale3x (the lightest touch), MMPX (keeps the art's own look) and xBR at 2x, 3x and 4x (the smoothest).
  Off by default. They run in a few milliseconds before each picture is shown, so they add no frames of input lag.
  They can be combined with SMOOTHING and SCANLINES; the CRT look uses the plain picture.

### Fixed
- The black screen at the start is now skipped after MAIN MENU or F3 (a reset) too, not only when the game is
  first started.

### Behind the scenes
- 37 more names in the decompiled code: the HUD (scores, chain display, lives and bombs), the stage's tasks, the
  items and their kinds, explosions and small effects, the enemies' parts, how objects move, the three ship types and
  the Start buttons. The game itself is unchanged.

## v0.7.0 (2026-11-13)

### New
- **Training anywhere in an area:** START AT now offers points part way through it, from 5% to 95% of the way to the
  boss in steps of 5, the same percentage the side panel's STAGE PROGRESS shows. The game fights its way there for
  you and hands over with the enemy bullets cleared, as it does for a mid-boss. Training replays keep the place.

### Changed
- **No more black screen at the start:** the arcade board's power-on self-test (about 4 seconds of black while it
  checks its memory and adds up its program ROM) now runs in an instant, so the first logo appears straight away
  after START GAME or a reset. The game itself is the same, replays too.
- **Closer to the arcade, checked against MAME:**
  - **Sound effects and music land where the arcade plays them.** Before, every sound started at the beginning of its
    frame, up to 17 ms early. Now each one starts at the moment in the frame that the game asks for it. The sound is
    also made the way MAME makes it, smoothed between samples and through the speaker's filter, instead of the
    simpler way before.
  - **Fast timing's picture matches the arcade's frame.** The background scroll and colour changes now line up with
    the sprites. Before, the ground was a frame behind the ships (about a pixel), and fades were a frame late.
  - **The board's settings file (donpachi.eeprom) is now MAME's format.** A copy of MAME's nvram/donpachi/eeprom can
    be dropped in as is, and the port's file works in MAME. Older files from the port are still read correctly.

### Behind the scenes
- About 120 more of the game's routines and memory locations now have names in the decompiled code (the sound
  driver, the text on screen, the game's modes, the players' ships, the score and chain, the enemies and bosses), so
  it reads more easily. The game itself is unchanged.

## v0.6.0 (2026-11-12)

### New
- **CRT look:** Settings > Graphics > CRT makes the picture look like the arcade's monitor: a gently curved screen,
  its scanlines, the red, green and blue phosphor stripes, a soft glow and darker corners. LIGHT or FULL; it works
  with the side panels and with the picture turned (TATE). Best in full screen.
- **AI player 2:** the computer can fly player 2 beside you (Cheats > AI PLAYER 2). It joins by itself while you play
  (and again after a game over), dodges the bullets it sees coming, goes for the strongest enemy, uses its laser on a
  boss or mid-boss once the other enemies are gone, and bombs when it's cornered. It counts as a cheat, and the
  player 2 cheats work for it too.
- **Save states in 5 slots:** F6 and F7 (or the options' STATE SLOT) pick the slot that F5 saves to and F8 loads
  from. Both keys can be changed on the Controls page.
- **The developers' debug tools:** Game Options > Debug Tools brings back the debug mode left in the arcade program:
  pause and move through a stage, stage select, an object spawner, a palette editor (and the ship can't be shot down),
  plus the developers' RAM editor. The page lists their controls. They count as cheats.

### Fixed
- When a second player joins a game partway through (a coin and P2 Start), the game no longer seems to end: no
  "save the replay?" question in the middle of play, and the statistics, side panels and training carry on. A game
  started by player 2 alone is followed properly too.

## v0.5.0 (2026-11-12)

### New
- **Cheats built in:** the Cheats page now works without MAME's cheat file. It has the game's own cheats for each
  player: infinite lives, infinite bombs, invincibility, MAX power and an endless chain (the GET POINT chain never
  runs out). MAME's cheat file still adds its cheats after them when it's there, and the page scrolls when the list
  is long.
- **SLOWDOWN in the side panel:** the right panel's settings now show the slowdown: OFF, or how much of this game
  has been slowed down so far (e.g. 2.4%).

### Fixed
- With slowdown on, a slowed-down frame no longer feeds half-finished game data to the achievements, the lifetime
  statistics, the side panels, the high score file or the hitbox display: they skip it and read the next whole
  frame. Play time still counts real time, slowdown included.

## v0.4.0 (2026-11-12)

### New
- **Slowdown, as on the arcade board:** when the screen gets busy, the game now slows down exactly as the real board
  does. Every instruction of the game's code counts its time on the board's 68000, so a frame with too much to do
  misses its moment and the game waits a frame more, at the same moments as on the arcade (checked against MAME's
  68000 for about half an hour of play: every frame the same). GAME OPTIONS > SLOWDOWN turns it off for full speed
  all the time (`[game] slowdown` in `donpachi.ini`). Replays keep the setting they were recorded with; replays
  from earlier versions play back with it off, as they were made.

## v0.3.0 (2026-11-11)

### New
- **Linux version:** `DonPachi-x.y.z-linux-x64.tar.gz`, for 64-bit Linux. Unpack it and run `./donpachi`; it uses
  the system's SDL2. Everything works as on Windows, and the first-run window takes the ROM set the same way.
- **3 new achievements** (26 in all), for the game's hidden rewards: **HEAVY METAL**, shoot down all three of area 2's
  heavy planes without bombing; **PATIENCE**, shoot area 3's hover boat after it stops (the game gives a bomb item
  for it); **HIDDEN EXTEND**, shoot down area 4's extend building without a bomb for its 1-Up (first loop only).

### Changed
- If the game's folder can't be written to (installed somewhere read-only), its settings, saves, replays and data
  are kept in your own folder instead: `~/.local/share/DonPachi/` on Linux, `%APPDATA%\DonPachi\` on Windows.
- The `rom` folder and the cheat file are now also looked for beside the program, so the game finds them however
  it is started (from a shortcut, a launcher or another folder).
- **Incomplete ROM sets:** when the ROM set you give the game (dropped on the first-run window, or in the `rom`
  folder) is missing files, the window now lists which ones, e.g. "THE SET IS MISSING 2 OF ITS 9 FILES: TEXT.U58,
  ATDP.U33", and marks a file that is there but is a different version or a bad copy as "(WRONG VERSION)". A file
  that isn't the DonPachi set at all still says so.

### Behind the scenes
- `tools/make_release.sh` packages the Linux release, with the same allow list and checks as the Windows script.
- Both release scripts now refuse to package while this changelog's Unreleased section lists changes, so a package
  can't carry changes its version number doesn't.
- New test tools for finding enemies and their rewards in the game's memory (`targets_find`, `targets_hunt`) and for
  checking the new achievements both ways (`achievement_targets`).

## v0.2.0 (2026-11-11)

### New
- **Achievements:** 23 goals, from area clears and one-credit clears to no-miss and no-bomb areas, all 13 bees, long
  chains, big scores, max rank and more. An "ACHIEVEMENT UNLOCKED" pop-up with a trophy appears on the right side
  panel (never over the game), with "+N MORE" when several are earned at once. An ACHIEVEMENTS page on the main menu
  and in Game Options lists them all with the date each was earned. Earned in arcade games only: not in training, or
  in a game that used a cheat or loaded a save state. Kept in `donpachi.achievements` beside the program.
- **Version number** on the title screen (bottom right).
- **Training: MID-BOSS start** for every area. The training run fights its way through the area at full power and
  hands over at the calm spot just before the mid-boss, with the enemy bullets cleared and score 0.
- **Menu tooltips:** with the side panels on, the right panel explains the highlighted menu entry while a menu is
  open.
- **Replays:** every arcade game is recorded as you play. When it ends (or when you leave it for the main menu) you
  are asked "DO YOU WANT TO SAVE THE REPLAY?"; saved ones go in a `replays` folder beside the program. Watch them
  from the new REPLAYS page on the main menu, which lists each with its date (or its name), the area reached and the
  score; choose one to WATCH, RENAME (type a name, or pick letters with the gamepad) or DELETE it. While watching:
  Space (or A) pauses, Right (or the d-pad's right) steps a frame at a time, fast forward works as in a game. A
  replay is a small file you can share; it plays the game back exactly from the same inputs, and says so if it goes
  out of step. Training runs too: SAVE REPLAY in the pause menu keeps the attempt (or the one before, just after a
  miss), and clearing the area asks as a game's end does. Games with cheats or a loaded save state are not recorded.
- **Mid-boss health bar:** the side panel's stage progress bar shows each area's mid-boss's health (orange) while it
  is on screen, then DEFEATED or ESCAPED.
- **Lifetime statistics:** a STATISTICS page on the main menu and in Game Options: games played, credits, play time,
  areas cleared, one-credit loops and clears, mid-bosses, bosses and the true boss shot down, enemies destroyed,
  deaths, extra lives, bombs used, bees, power-ups and bomb items collected; records (best chain, longest life,
  highest rank, best score for each difficulty); and each ship's and each area's numbers. Arcade games only, as the
  achievements. Kept in `donpachi.stats` beside the program.
- **Menu and achievement sounds**, borrowed from the game itself: a tick when moving through the menus or changing a
  setting, a sound for choosing and for going back, and a chime with the achievement pop-up. They follow the VOLUME
  setting.
- **Training: POWER range.** Choose level 1, 2, 3, 4 or MAX, or **ON PACE**: the power you would have by that point
  if you had collected every power-up on the way (shown as e.g. "ON PACE (3)").

### Changed
- **Pause menu in training:** RETRY TRAINING (back to the start of the training you are on) and TRAINING MENU are
  now at the top. RESET, SAVE STATE and LOAD STATE are left out there (F5 / F8 still work).
- **Fast forward speed** moved to Game Options (its key stays on the Controls page).
- The player README explains how to add MAME's cheat file for the Cheats page.

### Fixed
- **Boss health bar:** an area 1 boss shot down while one of its side parts was left showed TIME UP; it now shows
  DEFEATED (a boss counts as shot down when its core's points are scored).
- **Hitbox:** it no longer appears on screen before your ship does at the start of a game.
- Menu text with an odd number of letters (QUIT TO DESKTOP, VERSION: USA, the OPTIONS title) is now exactly centred.
- Training at full power: the game now knows it is at MAX, so the next power-up gives the MAX bonus as it should.
- The two hint lines at the bottom of the menus no longer overlap.

### Behind the scenes
- A reset before the program's first frame (as training does when it is the first thing chosen after starting the
  program) no longer delays the game by a frame: training's way to a place now comes out exactly the same every
  time, which training replays need.
- Release script: the zip now uses `/` in its paths (readable by Mac and Linux unzip tools); the version comes from
  `CMakeLists.txt`, the same one the game shows; achievement files are refused like other personal files; this
  changelog is included as `CHANGES.txt`.
- GitHub README with screenshots, download, build steps and credits; the cheat file is no longer in the repository.

## v0.1.0 (2026-11-10)

The first release.

- The arcade game (US version), decompiled to C and running natively: checked frame by frame against MAME's 68000
  core through both loops. Low input lag by default, with the arcade board's own timing as an option.
- First run: drop your own `donpachi.zip` onto the window; the game extracts what it needs once
  (`donpachi_usa.dat`), and the .zip isn't needed after that.
- Main menu: START GAME, TRAINING, SETTINGS, QUIT. Training starts at any area, its boss or area 10's true boss, with
  your choice of ship, power, bombs and difficulty, and can retry on a miss.
- Side panels on wide screens: chain (GP) gauge, rank, run statistics, bees collected, score graph against the top
  score, stage progress and boss health, input display, settings in use.
- Save states (F5 / F8), fast forward, hitbox display, high scores saved between sessions.
- Graphics: scaling modes, smoothing, scanlines, vsync, TATE rotation, windowed / borderless / exclusive full screen.
- Game options: difficulty, free play, the arcade's hidden big HIT display, the arcade's test menu, MAME cheats.
- Keyboard and gamepads (Xbox layout, two players), everything remappable; settings in `donpachi.ini`.
- The program's own icon.
