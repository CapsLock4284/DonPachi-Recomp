/**
 *    Name:        hiscore.h
 *    Description: the high score table kept in a file (donpachi.hiscore beside the program), as MAME's high score
 *                 plugin does: the arcade board keeps only its settings (the EEPROM), and its table goes back to
 *                 the defaults each time it is switched on
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The table, in the game's memory ($101846-$10189F, set up once at boot by start_buttons_task's first run):
 *      $101846  the five scores (BCD longs, #1 first: 800000 ... 400000 by default)
 *      $10185A  the five names (3 words each)
 *      $101878  $101882  $10188C  $101896   a word each for the five (the entry's ship, stage and the like)
 *      $101782  the HUD's HIGH: #1's score, until a player's goes over it
 *    The file's table is put back over the defaults as soon as the game has set them up (after its boot, and
 *    after each reset); then the table is saved whenever the game changes it. Nothing else is touched.
 */
#ifndef DONPACHI_HISCORE_H
#define DONPACHI_HISCORE_H

#include <stdbool.h>

/* The file ('path', or NULL: donpachi.hiscore beside the program), read now: NULL, or what was wrong with it (a
   file that is not there is no problem: the game keeps its defaults until there is one) */
const char *hiscore_open(const char *path);

/* The board was reset: its table is set up again (to put the file's back over) */
void hiscore_reset(void);

/* A save state was gone back to (cave/program.h), or a game that went back to one is over: the file's table put
   back over the game's (its scores stay the file's), and kept as it is */
void hiscore_state_loaded(void);

/* After each of the game's frames. 'enabled': the table is put back and saved ([game] save_high_scores); 'saving':
   changes may go into the file (not while cheats are on) */
void hiscore_frame(bool enabled, bool saving);

/* The file's table was put back in the game's memory (at boot) since the last call: once (a replay records what was
   written: $101846-$10189F and the HUD's HIGH) */
bool hiscore_take_put_back(void);

#endif
