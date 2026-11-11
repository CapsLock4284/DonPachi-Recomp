# Changelog

What has changed in each release of DonPachi Recomp, newest first. Changes made since the last release go under
**Unreleased**; when a release is made, that section becomes its version (and is pasted into the GitHub release's
notes). The release zip carries this file as `CHANGES.txt`.

## Unreleased

### New
- **Stage map:** the right side panel now shows a map of the whole area, with a bright box over the part on screen
  that moves up as the area scrolls; the rest of the map is slightly darker, with the mid-boss and the boss marked.
  The maps are made from your own ROM set the first time the game starts (about 10 seconds, "BUILDING STAGE MAPS"),
  and kept in `donpachi_maps.dat` beside the program. STAGE MAP in Settings > Graphics switches back to the score
  graph.
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
