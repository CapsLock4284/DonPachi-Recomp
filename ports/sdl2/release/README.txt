DonPachi (arcade, US version) for Windows - version {VERSION}
=============================================================

A native Windows version of Cave's 1995 arcade shooter DonPachi, rebuilt from the
arcade program. It does not come with the game's graphics, sound or data: you need
your own copy of the arcade ROM set.


GETTING STARTED
---------------
1. Unzip this folder somewhere you can write to (your Documents or Desktop, for
   example; not inside "Program Files").
2. Double-click donpachi.exe.
   Windows may say "Windows protected your PC" the first time, because the program
   is not signed. Click "More info", then "Run anyway".
3. A window asks for the ROM set. Drag MAME's donpachi.zip (the US version) onto
   it. The game takes what it needs from it once and saves it as donpachi_usa.dat
   beside the program; after that the .zip isn't needed. Merged and non-merged MAME
   sets both work.


CONTROLS (change them in SETTINGS > CONTROLS)
---------------------------------------------
                  Player 1        Player 2        Gamepad
  Move            Arrow keys      R D F G         D-pad / left stick
  Shot (hold:     Z               A               X
   laser)
  Bomb            X               S               A
  Auto fire       C               Q               B
  Start           1               2               Start
  Coin            5               6               -

  Esc / gamepad Back     the menu (pause, settings, save states, training)
  F3                     reset the game
  F4                     reset into the arcade's test (service) menu
  F5 / F8                save state / load state (for this session)
  Tab / LB (held)        fast forward
  F11 or Alt+Enter       full screen

Free play is on by default: Start drops in a credit.


FEATURES
--------
  - Main menu: START GAME, TRAINING (start at any area, mid-boss or boss, with
    your choice of ship, power, bombs and difficulty), SETTINGS
  - Side panels on wide screens: chain gauge, rank, run statistics, bees
    collected, score graph against the top score, stage progress and boss and
    mid-boss health
  - Save states, fast forward, hitbox display, scanlines, borderless or exclusive
    full screen, difficulty and the arcade's hidden big HIT display in GAME OPTIONS
  - High scores kept between sessions (not while cheats or save states were used)
  - 23 achievements (main menu > ACHIEVEMENTS): area clears, no-miss and no-bomb
    clears, all 13 bees, long chains, scores and more. Earned in the arcade game
    only: not in training, or in a game with a cheat or a loaded save state
  - Replays (main menu > REPLAYS): every arcade game is recorded; when it ends
    you're asked whether to save it (in the replays folder). Watch them from
    the menu, or share the .dpreplay files
  - Lifetime statistics (main menu > STATISTICS): games, play time, clears,
    bosses, deaths, bombs, bees, best chain and scores, per ship and per area
    (arcade games only, as the achievements)


CHEATS (optional)
-----------------
The cheats page (SETTINGS > GAME OPTIONS > CHEATS) reads MAME's cheat file for
DonPachi, which isn't included here. To add it:
  1. Download MAME's cheat collection, cheat.7z (from Pugsy's Cheats,
     https://www.mamecheat.co.uk).
  2. Open it with 7-Zip and take out donpachi.xml.
  3. Make a folder called "cheats" beside donpachi.exe and put donpachi.xml in it,
     so you have:   cheats\donpachi.xml   in the same folder as donpachi.exe.
     (The game looks there when started by double-clicking donpachi.exe. Started
     from a shortcut, it looks in the shortcut's "Start in" folder instead; or set
     the full path as "cheats =" under [files] in donpachi.ini.)
High scores aren't saved while a cheat is on.


FILES THE GAME KEEPS BESIDE donpachi.exe
----------------------------------------
  donpachi_usa.dat   the game data taken from your ROM set
  donpachi.ini       your settings (delete it to go back to the defaults)
  donpachi.eeprom    the arcade board's own settings (the test menu's)
  donpachi.hiscore   the high score table
  donpachi.achievements   the achievements you've earned, and when
  donpachi.stats     your lifetime statistics
  replays\           your replays (.dpreplay files: share them, or delete the
                     ones you don't want)

Keep these if you move the game to another folder or computer. Don't share
donpachi_usa.dat: it is the contents of the ROM set.


LICENSES
--------
See the licenses folder: this program's notices, and SDL2's license (SDL2.dll).
DonPachi is (c) 1995 Cave / Atlus. This is an unofficial fan project, not
connected with or endorsed by them.
