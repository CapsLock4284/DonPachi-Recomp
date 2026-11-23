DonPachi (arcade, US and Japanese versions) for Windows and Linux - version {VERSION}
=====================================================================================

A native Windows and Linux version of Cave's 1995 arcade shooter DonPachi,
rebuilt from the arcade program. It does not come with the game's graphics, sound
or data: you need your own copy of the arcade ROM set.


GETTING STARTED
---------------
On Windows (DonPachi-{VERSION}-win64.zip):
1. Unzip this folder somewhere you can write to (your Documents or Desktop, for
   example; not inside "Program Files").
2. Double-click donpachi.exe.
   Windows may say "Windows protected your PC" the first time, because the program
   is not signed. Click "More info", then "Run anyway".

On Linux (DonPachi-{VERSION}-linux-x64.tar.gz; a 64-bit Linux):
1. Unpack it somewhere in your home folder:
       tar xzf DonPachi-{VERSION}-linux-x64.tar.gz
2. Run ./donpachi in that folder (or double-click it in your file manager).
   It uses your system's SDL2. If it says libSDL2-2.0.so.0 is missing, install it:
       Debian / Ubuntu / Mint:  sudo apt install libsdl2-2.0-0
       Ubuntu 25.10 and later:  sudo apt install libsdl2-compat
       Fedora:                  sudo dnf install SDL2
       Arch:                    sudo pacman -S sdl2

Then, on both:
3. A window asks for the ROM set. Drag MAME's donpachi.zip (the US version) onto
   it. The game takes what it needs from it once and saves it as donpachi_usa.dat
   beside the program; after that the .zip isn't needed. Merged and non-merged MAME
   sets both work. If your set is incomplete, the window lists the files it is
   missing.

THE JAPANESE VERSION
The game also has the original Japanese release (MAME's donpachij, Ver. 1.01),
with its smaller hitbox. Choose VERSION on the main menu. Its data is taken on
the first run along with the US one if you have it: a merged donpachi.zip, or
donpachij.zip beside donpachi.zip in the rom folder, or dropped on the first
run's window after donpachi.zip (or skipped there). Otherwise the first time you
choose it, it asks for MAME's donpachij.zip: drag it onto the window that asks. It is saved as donpachi_jpn.dat and
the .zip isn't needed after that. Each version keeps its own high scores and
arcade settings; the achievements and statistics are shared. Replays remember
their version and switch to it when you watch them. The game starts in the
version you chose last (or add --japan / --usa when starting it).


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
  F6 / F7                the save state slot before / after (5 slots)
  Tab / LB (held)        fast forward
  F9                     the FPS meter (on the right side panel)
  F11 or Alt+Enter       full screen

Free play is on by default: Start drops in a credit.

Other gamepads and arcade sticks: the game knows most gamepads by their
layout (more of them from gamecontrollerdb.txt in this folder). One it doesn't
know still works, read as it is: its d-pad and stick move, buttons 1, 2 and 3
are shot, bomb and auto fire, its last button is Start and the one before Coin.
Set its buttons in SETTINGS > CONTROLS (they show as BTN 1, AXIS 2+, HAT UP);
its first button also chooses in the menus and its second goes back. Open the
menu with Esc. donpachi.log names each device the game found and how it reads it.


FEATURES
--------
  - The arcade's slowdown: when the screen is busy the game slows down at the
    same moments as the real board, as it counts the 68000's time instruction by
    instruction (checked against MAME). GAME OPTIONS > SLOWDOWN turns it off
  - Main menu: START GAME, TRAINING (start at any area, any point part way
    through it, its mid-boss or boss, with your choice of ship, power, bombs
    and difficulty), SETTINGS
  - Side panels on wide screens: chain gauge, rank, run statistics, bees
    collected, score graph against the top score, stage progress and boss and
    mid-boss health
  - A CRT look (SETTINGS > GRAPHICS > CRT): a curved arcade screen with its
    scanlines, phosphor stripes and glow
  - Pixel-art filters (SETTINGS > GRAPHICS > FILTER): Scale2x, Scale3x, xBR 2x,
    3x and 4x, and MMPX, smoothing the picture's edges while keeping it sharp;
    BY LAYER runs it on each of the game's layers and its sprites on their own,
    so sprite outlines stay clean against the background
  - Save states, fast forward, hitbox display, scanlines, borderless or exclusive
    full screen, difficulty and the arcade's hidden big HIT display in GAME OPTIONS
  - Bullet points (GAME OPTIONS > BULLETS): the one point of each enemy bullet
    the game tests; a bullet hits only when that point is inside your hitbox
  - Autobomb (GAME OPTIONS > AUTOBOMB): a bomb goes off by itself when your ship
    is about to be hit, if you have one (an assist: those games don't count for
    high scores, achievements or statistics)
  - High scores kept between sessions (not while cheats or save states were used)
  - 26 achievements (main menu > ACHIEVEMENTS): area clears, no-miss and no-bomb
    clears, all 13 bees, long chains, scores and more. Earned in the arcade game
    only: not in training, or in a game with a cheat or a loaded save state
  - Replays (main menu > REPLAYS): every arcade game is recorded; when it ends
    you're asked whether to save it (in the replays folder). In training,
    SAVE REPLAY in the pause menu keeps the attempt. Watch, rename or delete
    them from the menu, or share the .dpreplay files. While watching: Space
    (gamepad A) pauses, Right (d-pad right) steps one frame
  - Menu tooltips: with the side panels on, the right one explains the
    highlighted option while a menu is open
  - Lifetime statistics (main menu > STATISTICS): games, play time, clears,
    bosses, deaths, bombs, bees, best chain and scores, per ship and per area
    (arcade games only, as the achievements)
  - Sound (SETTINGS > SOUND): VOLUME for all of it, and MUSIC, EFFECTS and
    VOICES each on their own
  - Updates: as it starts, the game looks for a newer release on GitHub. If
    there is one, UPDATE TO V... shows at the top of the main menu: it asks
    first, then downloads it, checks it and starts the game again. Your files
    below are kept. GAME OPTIONS > CHECK FOR UPDATES turns the look-up off


CHEATS
------
The cheats page (SETTINGS > GAME OPTIONS > CHEATS) has the game's own: AI PLAYER
2 (the computer flies player 2 beside you, joining by itself), and infinite
lives, infinite bombs, invincibility, MAX power and an endless chain, for each
player. GAME OPTIONS > DEBUG TOOLS brings back the developers' debug mode and
RAM editor, left in the arcade program (the page lists their controls). For
more cheats, add MAME's cheat file for DonPachi (not included here); its
cheats are listed after the game's own:
  1. Download MAME's cheat collection, cheat.7z (from Pugsy's Cheats,
     https://www.mamecheat.co.uk).
  2. Open it with 7-Zip and take out donpachi.xml.
  3. Make a folder called "cheats" beside the program (donpachi.exe on Windows,
     donpachi on Linux) and put donpachi.xml in it, so you have
     cheats/donpachi.xml in the same folder as the program. (Or set the full path
     as "cheats =" under [files] in donpachi.ini.)
High scores aren't saved while a cheat is on.


FILES THE GAME KEEPS BESIDE THE PROGRAM
---------------------------------------
  donpachi_usa.dat   the game data taken from your ROM set
  donpachi_jpn.dat   the same for the Japanese version, if you've played it
  donpachi.ini       your settings (delete it to go back to the defaults)
  donpachi.eeprom    the arcade board's own settings (the test menu's)
  donpachi.hiscore   the high score table
  donpachi_jpn.eeprom, donpachi_jpn.hiscore   the same for the Japanese version
  donpachi.achievements   the achievements you've earned, and when
  donpachi.stats     your lifetime statistics
  donpachi.log       the program's messages (Windows): if the game ever closes by
                     itself, this says why, so it helps to send it with a report
  donpachi_previous.log   the same from the time before
  donpachi_crash_*.inp    left only if the game stopped by itself: your inputs
                     up to it, to play the run again (send it with the log)
  replays\           your replays (.dpreplay files: share them, or delete the
                     ones you don't want)
  *.old, update_old.txt   left by an update for a moment: the program's old
                     files, deleted when the game next starts

Keep these if you move the game to another folder or computer. Don't share
donpachi_usa.dat or donpachi_jpn.dat: they are the contents of the ROM sets.

donpachi.eeprom is in MAME's format: to bring your settings over from MAME,
copy MAME's nvram\donpachi\eeprom beside the program as donpachi.eeprom (and
the other way round).

If the game's folder can't be written to (installed somewhere read-only), it
keeps them in your own folder instead: ~/.local/share/DonPachi/ on Linux,
%APPDATA%\DonPachi\ on Windows.


LICENSES
--------
See the licenses folder: this program's notices, and SDL2's license.
DonPachi is (c) 1995 Cave / Atlus. This is an unofficial fan project, not
connected with or endorsed by them.
