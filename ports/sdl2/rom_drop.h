/**
 *    Name:        rom_drop.h
 *    Description: the first run's window when there is no game data and no ROM set: "drop the arcade ROM set here"
 *    Version:     0.1.0 (draft)
 *    Created:     10-11-2026
 *
 *    license:BSD-3-Clause
 *    Drawn with its own small font (the game's font is in the ROMs it is waiting for).
 */
#ifndef DONPACHI_ROM_DROP_H
#define DONPACHI_ROM_DROP_H

#include <stdbool.h>

/* A window that waits for donpachi.zip (or a folder of the set) to be dropped onto it: read with rom_set_load,
   then saved as the game's data at 'data_path' (rom_set_save_data). 'why' is shown small, under the request (what
   was wrong with the data or the 'rom' setting). true: the set is read ('saved': and saved as the data); false:
   the window was closed */
bool rom_drop_screen(const char *data_path, const char *why, bool *saved);

#endif
