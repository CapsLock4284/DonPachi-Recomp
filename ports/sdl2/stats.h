/**
 *    Name:        stats.h
 *    Description: lifetime statistics: totals and records across every arcade game played, kept in donpachi.stats
 *                 beside the program and shown on the STATISTICS page
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    Watched from the game's memory after each frame, as the achievements are (the same games count: not training,
 *    not a game that had a cheat on or went back to a save state). Saved as each area ends, as each game ends and as
 *    the program closes.
 */
#ifndef DONPACHI_STATS_H
#define DONPACHI_STATS_H

#include <stdbool.h>

/* The file's statistics (missing or unreadable: all 0) */
void stats_load(const char *path);
void stats_save(void);

/* After each frame of the game: 'counts' false while the game may not count (training, cheats, a save state);
   'defeats' the bosses shot down this frame (widgets_take_defeats: 1 a boss, 2 the true boss, 4 a mid-boss) */
void stats_frame(bool counts, unsigned defeats);

/* The STATISTICS page's lines: a heading (value NULL) or a label and its value */
int stats_lines(void);
const char *stats_label(int line);
const char *stats_value(int line);    /* NULL: the line is a heading */

#endif
