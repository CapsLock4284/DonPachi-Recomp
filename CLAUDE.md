# Working on DonPachi Recomp

Notes for Claude on how the owner wants this project handled. Read this before making changes.

## The project

A native Windows port of DonPachi (arcade, US version, MAME set `donpachi`): the 68000 program decompiled to C by
[arcade-decomp](https://github.com/cheviman/arcade-decomp) (`tools/core`, a submodule) and run on a model of the Cave
board (`runtime/`). The Windows game is `ports/sdl2/`. `TODO.md` is the plan and the notes on what has been found in the
game (RAM addresses, routines); read the relevant part before starting on something.

- Build (the owner uses PowerShell on Windows): `cmake --build build/msvc --config Release`, then `.\build\bin\donpachi.exe`
- The decompiled C (`src/`) is made locally from the ROM: `python tools/decompile.py`
- Test tools: `tests/harness/` (see its README): `autoplay` (plays by itself, reports achievements, bees, deaths),
  `training_check`, `panels_shot`, `menu_shot`, `romdata_check`, `drop_window_test`. Build with
  `sh tests/harness/build.sh` (Linux). Use and extend them to check changes before handing them over
- The version is set in one place: `project(donpachi VERSION x.y.z ...)` in `CMakeLists.txt` (the main menu shows it;
  `tools/make_release.ps1` packages from it)

## Standing rules

- **Nothing from the ROM is ever committed or published.** `rom/`, `src/`, `assets/`, `build/`, `release/`, `*.zip`,
  `*.dat` stay git-ignored. Never add game graphics, sound, data or decompiled code to the repo. (The exception the owner
  chose: the screenshots in `docs/screenshots/`.) Release zips are made only by `tools/make_release.ps1` (an allow list
  and checks), never by hand.
- **Keep `CHANGELOG.md` up to date.** When a change players would notice is finished (a feature, a fix, a change in
  behaviour), add a line under `## Unreleased` in the same step, and tell the owner in the reply that it was logged, quoting
  the line. TODO-only edits and internal tidying don't need an entry.
- **Give git commands with a description** of what each does whenever the owner should commit, e.g.
  `git add ...` / `git commit -m "..."` / `git push`. The owner commits and pushes himself.
- **Ask when a message could be a question or a request.** If the owner asks how something works, explain it to him; only
  change files (README, code) when he asks for that.
- **Never put the owner's real name in anything that goes into git** (files, comments, TODO, changelog, commit
  messages): call him "the owner"; his ideas in `TODO.md` are marked "(owner)".
- **Keep `TODO.md` current:** add the owner's ideas when he gives them (marked "(owner)"), tick items off when done, with what
  was found.

## Game decisions already made

- Fast input timing is the default; the arcade board's timing is an option.
- Auto fire (button C) must behave exactly like the arcade's own rapid fire.
- High scores and achievements count only for the arcade game: not training, not a game that used a cheat or loaded
  a save state, and not with the AI second player (it counts as a cheat).
- Achievement pop-ups and other overlays may cover the side panels, never the game picture.
- Priorities: the Japan version comes last (after the US features); online multiplayer is the lowest priority of all
  (last, if at all). The Steam Deck and macOS versions are off the TODO: only if someone asks for them.
