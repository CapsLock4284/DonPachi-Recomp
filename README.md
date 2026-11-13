<p align="center"><img src="ports/sdl2/icon/donpachi.png" width="96" alt="DonPachi Recomp icon"></p>

# DonPachi Recomp

*A huge thank you to [cheviman](https://github.com/cheviman), author of
[arcade-decomp](https://github.com/cheviman/arcade-decomp): without it, this project would never have started.*

A native Windows and Linux port of Cave's 1995 arcade shooter **DonPachi** (US version), made by decompiling the
arcade board's 68000 program into C and running it on a model of the Cave hardware. No emulator: the game's own logic
runs natively, frame for frame the same as the arcade original.

**This repository contains no game data.** You need your own copy of the arcade ROM set (MAME's `donpachi.zip`).
The game reads its graphics, sound and data from it.

## The arcade's slowdown, exactly

In DonPachi, slowdown is part of the game. When the screen fills with bullets, the board's 16 MHz 68000 can't finish
a frame's work before the screen refreshes, so the game waits a frame and everything slows for a moment. Players
lean on it to thread the densest patterns, and the boss fights feel different without it.

A native port normally runs the game's code in no time at all, so it never slows down. DonPachi Recomp puts the
slowdown back the way the board makes it:

- **Every instruction counts its time.** The decompiled C carries each 68000 instruction's cycle count, including the
  ones that depend on their data (multiplies, divides, shifts) and on which way a branch goes.
- **The VBLANK lands between the same two instructions as on the board.** The 68000 decides whether to take an
  interrupt at one step inside each instruction; the port knows that point for every instruction form, taken from
  MAME's cycle-exact 68000 core.
- **Checked against MAME, frame by frame:** about 29 minutes of play (100,000 screen refreshes), and every one of the
  96,389 frames of the game matched MAME's 68000: the same board state, the same frames slowed down, every interrupt
  on the same cycle.

It's on by default. GAME OPTIONS > SLOWDOWN turns it off if you'd rather play at full speed all the time.

## Screenshots

<p align="center">
  <img src="docs/screenshots/area3.png" width="49%" alt="Area 3, with the side panels: chain gauge, rank, run statistics, bees, score graph and stage progress">
  <img src="docs/screenshots/boss.png" width="49%" alt="Area 1's boss, with its health bar in the right panel">
</p>
<p align="center">
  <img src="docs/screenshots/area4.png" width="49%" alt="Area 4">
  <img src="docs/screenshots/menus.png" width="49%" alt="The main menu and the training page">
</p>

## Download and play

1. Get the latest release from [**Releases**](https://github.com/CapsLock4284/DonPachi-Recomp/releases/latest):
   `DonPachi-x.y.z-win64.zip` for Windows, `DonPachi-x.y.z-linux-x64.tar.gz` for Linux.
2. **Windows:** unzip it somewhere you can write to (not inside *Program Files*) and run `donpachi.exe`.
   Windows may show *"Windows protected your PC"* because the program isn't signed: click **More info → Run anyway**.<br>
   **Linux:** unpack it (`tar xzf DonPachi-x.y.z-linux-x64.tar.gz`) and run `./donpachi`. It uses the system's SDL2
   (`sudo apt install libsdl2-2.0-0` on Debian/Ubuntu, `libsdl2-compat` on Ubuntu 25.10 and later, `dnf install SDL2`
   on Fedora, `pacman -S sdl2` on Arch, if it isn't there already).
3. On the first run a window asks for the ROM set: drag `donpachi.zip` onto it. The game extracts what it needs once
   (`donpachi_usa.dat`, beside the program); after that the .zip isn't needed. Merged and non-merged MAME sets both work.

**Cheats:** the Cheats page (Settings > Game Options > Cheats) has the game's own: infinite lives, infinite bombs,
invincibility, MAX power and an endless chain, for each player. For more, add MAME's cheat file for DonPachi (not
included): take `donpachi.xml` out of MAME's `cheat.7z` (from [Pugsy's Cheats](https://www.mamecheat.co.uk)) and put
it in a `cheats` folder beside the program, as `cheats/donpachi.xml`; its cheats are listed after the game's own.

The release's `README.txt` has the controls, hotkeys and the list of files the game keeps. What's new in each
version: [CHANGELOG.md](CHANGELOG.md).

## Features

- **The arcade game, exactly:** checked frame by frame against MAME's 68000 core through both loops
- **The arcade's slowdown**, at exactly the same moments as the real board (see above); can be turned off
- **Low input lag** by default, with the board's own timing as an option
- **Main menu** with **Training**: start at any area, its mid-boss, its boss or area 10's true boss, with your choice
  of ship, power (any level, or "on pace" for that point), bombs and difficulty, and retry on a miss
- **Side panels** on wide screens: chain (GP) gauge, rank, run statistics, bees collected, score graph against the
  top score, stage progress and boss and mid-boss health
- **Save states**, fast forward, hitbox display, high scores saved between sessions
- **26 achievements**, with a pop-up when one is earned (arcade games only: not training, cheats or save states)
- **Replays**: every arcade game (and training run) is recorded; save it as a small file you can share, and watch
  them from the main menu's REPLAYS, with pause and frame-by-frame stepping; rename or delete them there
- **Lifetime statistics**: games, play time, clears, bosses, deaths, bombs, bees, best chain and scores, per ship and
  per area (arcade games only)
- **Menu tooltips** on the side panel, explaining each option
- **Graphics:** a CRT look (a curved screen with scanlines, phosphor stripes and glow), scaling modes, smoothing,
  scanlines, vsync, TATE rotation, borderless or exclusive full screen
- **Game options:** difficulty, free play, the arcade's hidden big HIT display, the arcade's test menu,
  cheats (built in, and MAME's cheat file's)
- Keyboard and gamepads (Xbox layout, two players), everything remappable, settings in `donpachi.ini`

## Building from source

The decompiled C isn't in the repository (it comes from the ROM), so a build makes it from your own ROM set first.

**Windows needs:** Git, Python 3, CMake 3.20+, Visual Studio 2022 (C), and SDL2's development package
(`SDL2-devel-2.x.x-VC.zip` from [libsdl.org](https://github.com/libsdl-org/SDL/releases)).

```powershell
git clone --recursive https://github.com/CapsLock4284/DonPachi-Recomp.git
cd DonPachi-Recomp
# 1. your ROM set:            rom\donpachi.zip
# 2. SDL2's package unpacked: build\downloads\sdl2\SDL2-2.x.x\
python tools/decompile.py                       # writes src\ (the game's C) from the ROM
cmake -S . -B build/msvc
cmake --build build/msvc --config Release      # build\bin\donpachi.exe
```

**Linux needs:** Git, Python 3, CMake 3.20+, gcc or clang, and SDL2's development files (WSL's Ubuntu works too):

```sh
sudo apt install git python3 cmake build-essential libsdl2-dev     # (Debian / Ubuntu; on Ubuntu 25.10 and later: libsdl2-compat-dev)
git clone --recursive https://github.com/CapsLock4284/DonPachi-Recomp.git
cd DonPachi-Recomp
# your ROM set: rom/donpachi.zip
python3 tools/decompile.py                                         # writes src/ (the game's C) from the ROM
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux -j                                       # build/bin-linux/donpachi
```

`tools\make_release.ps1` (Windows) and `tools/make_release.sh` (Linux) package a release (an allow list of files,
checked for anything from the ROM before it's packed).

## Project layout

| Folder | What's in it |
|---|---|
| `runtime/` | the board model: memory map, video (tile layers, sprites), sound (two OKI M6295s, NMK112), EEPROM, ROM set reader |
| `ports/sdl2/` | the game (Windows and Linux): window, menus, settings, training, side panels, save states |
| `ports/headless/` | a windowless build for testing and frame-by-frame checks |
| `tools/` | the decompiler's game profile, analysis tools, the reference checker (`tools/cref`), packaging |
| `symbols.txt` | names for the program's functions and RAM (addresses and names only) |
| `TODO.md` | the plan, and notes on what's been found in the game |

`rom/`, `src/`, `assets/`, `build/`, `release/` and `*.dat` are git-ignored: anything derived from the ROM stays on
your own machine.

## Credits

- **DonPachi** is © 1995 Cave / Atlus. This is an unofficial fan project, not connected with or endorsed by them.
- The decompiler core is [arcade-decomp](https://github.com/cheviman/arcade-decomp) (in `tools/core`, from
  [a fork](https://github.com/CapsLock4284/arcade-decomp/tree/timing-model) that adds the 68000's timing for the
  slowdown); the runtime and build follow tmntarcade-decomp's (Sebastian Toyos, BSD-3-Clause).
- The hardware model follows [MAME](https://github.com/mamedev/mame)'s Cave drivers (Luca Elia and the MAME team,
  BSD-3-Clause).
- [SDL2](https://www.libsdl.org/) (zlib license).
- Bee counts from the [shmups wiki](https://shmups.wiki/library/DonPachi).

## License

The project's own code is under the BSD 3-Clause license (see each file's header). The game itself is not included and
not covered by it.
