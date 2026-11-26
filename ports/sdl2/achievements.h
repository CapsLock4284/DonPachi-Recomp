/**
 *    Name:        achievements.h
 *    Description: goals met in the arcade game (clear an area without dying, all 13 bees, a 100 hit chain, ...),
 *                 kept in donpachi.achievements beside the program
 *    Version:     0.1.0 (draft)
 *    Created:     10-11-2026
 *
 *    license:BSD-3-Clause
 *    Watched from the game's memory after each frame. Only the arcade game counts: not training, not a game that
 *    had a cheat on or went back to a save state (from its ship select on).
 */
#ifndef DONPACHI_ACHIEVEMENTS_H
#define DONPACHI_ACHIEVEMENTS_H

#include <stdbool.h>

/* The file's achievements (missing or unreadable: none yet) */
void achievements_load(const char *path);

/* After each frame of the game: 'counts' false while the game may not earn them (training, cheats, a save state) */
void achievements_frame(bool counts);

/* The next achievement earned and not yet shown (its name), or NULL; and how many more wait after it */
const char *achievements_next_shown(void);
int achievements_waiting(void);

int achievements_count(void);
int achievements_done(void);
const char *achievements_name(int index);
const char *achievements_text(int index);       /* what it asks for */
const char *achievements_date(int index);       /* when it was earned ("2026-11-10"), or NULL */
int achievements_find(const char *id);          /* the index of the one kept as 'id' ("no_miss_area"), or -1 */

/* How far the game being played has come towards an achievement: for the gadget panels' goals (gadgets.h), the
   ones nearest to being earned shown */
enum { GOAL_GOING, GOAL_KEPT, GOAL_LOST };    /* under way; met (or held so far, an area's NO MISS); out of reach */

typedef struct
{
	const char *label;          /* short, as "CHAIN 100" */
	char value[24];             /* where it stands, as "BEST 64" or "2,859,440 TO GO" */
	float part;                 /* 0-1: how near */
	int state;                  /* GOAL_GOING, GOAL_KEPT, GOAL_LOST */
} achievement_goal;

/* The achievement 'index' as this game stands: false when it can't be had in this game (or with what can be read
   of it), or isn't the next of its kind (CHAIN 100 before CHAIN 30 is earned, say) */
bool achievements_goal(int index, achievement_goal *out);

#endif
