/**
 *    Name:        training.h
 *    Description: the main menu's TRAINING: the game started straight at a chosen place (an area's start, its boss,
 *                 area 10's true boss), with the ship and power chosen, as a save state to go back to
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The game is run there unseen and unheard, as fast as it goes: from the board's reset, a coin and Start, the ship
 *    chosen on its select screen (down presses); area 1 played with the ship shielded (the respawn shield $102196
 *    held), not firing, to its boss's end, where the stage and loop are set ($101A40, $101A42) so that the game's next
 *    area is the place's (area 6: area 5's, so that the game goes through the loop's end into the second, as it starts
 *    area 6 with the dropship; set straight to it, the stage began as a mere next area, the ship already flying). A
 *    mid-boss: the area played from its start at full power, shielded, firing at the strongest enemy on the screen,
 *    to the calm spot before the mid-boss (the area's scroll), where the enemies' bullets are taken off. A boss: on,
 *    shielded, until its WARNING starts (object $3F in $100102's list; the true boss:
 *    its own object, $7C, area 10's first boss shot down on the way). There the player's run is made a fresh one:
 *    score 0, the chain gone, the bees taken back to the area's start's, 3 lives, 3 bombs (or 7 in 7 slots), the
 *    power chosen, the shield taken off.
 */
#ifndef DONPACHI_TRAINING_H
#define DONPACHI_TRAINING_H

#include <stdbool.h>

enum { TRAINING_START, TRAINING_MIDBOSS, TRAINING_BOSS, TRAINING_TRUE_BOSS, TRAINING_POINTS };

/* The power: levels 1-4 (0-3), ON PACE (the power-ups there are on the way to the place, all taken: from area 3 on,
   MAX), MAX */
enum { TRAINING_POWER_ON_PACE = 4, TRAINING_POWER_MAX, TRAINING_POWERS };

typedef struct
{
	int area;                   /* 0-9 (areas 1-10) */
	int point;                  /* TRAINING_START ... (TRAINING_TRUE_BOSS: area 10 only) */
	int ship;                   /* 0-2: types A, B, C */
	int power;                  /* 0-3 (levels 1-4), TRAINING_POWER_ON_PACE, TRAINING_POWER_MAX */
	bool max_bombs;             /* 7 slots, all full (the game's most: 3 to start, one more each 3 bombs used) */
	int difficulty;             /* the test mode's: 0 normal, 1 easy, 2 hard, 3 very hard (for this game only) */
} training_setup;

/* The game run to the place: NULL (the game is there, between frames: the port saves its state), or what went
   wrong. The game is reset first */
const char *training_prepare(const training_setup *setup);

/* The difficulty's name ("VERY HARD"), for the EEPROM's value 0-3 */
const char *training_difficulty_name(int difficulty);

/* The HUD's player 1 score and HIGH drawn again from their values (the game draws them as they change: the port
   that changes them, as the high score table is put back, calls this) */
void training_hud(void);

/* The power's name for the menu ("2", "ON PACE (3)", "MAX"): ON PACE's level is the place's */
const char *training_power_name(const training_setup *setup);

/* The place's name for the screen ("AREA 3 BOSS") */
const char *training_name(const training_setup *setup);

#endif
