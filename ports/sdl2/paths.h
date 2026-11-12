/**
 *    Name:        paths.h
 *    Description: where the player's files are kept: the settings, the EEPROM, the high scores, the achievements,
 *                 the statistics, the replays and the game's data extracted from the ROM set
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    Beside the program, as the game unpacked anywhere has them (Windows, a Linux .tar.gz). When that folder can't be
 *    written (installed read-only: an AppImage, /usr, Program Files), the user's own folder for the game instead
 *    (SDL_GetPrefPath: ~/.local/share/DonPachi/ on Linux, %APPDATA%\DonPachi\ on Windows, ~/Library/Application
 *    Support/DonPachi/ on macOS).
 */
#ifndef DONPACHI_PATHS_H
#define DONPACHI_PATHS_H

#include <stddef.h>

/* The folder the player's files are kept in, with its separator ("" if none can be found: the working folder) */
const char *paths_folder(void);

/* A file or folder given by the settings ('path', e.g. "rom" or "cheats/donpachi.xml"): as given when it is there
   (from the working folder, or a full path); else in the files' folder or beside the program, if there. Written to
   'out' */
void paths_find(const char *path, char *out, size_t size);

#endif
