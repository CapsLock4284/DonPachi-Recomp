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
 *    area is the place's. A boss: on, shielded, until its WARNING starts (object $3F in $100102's list; the true boss:
 *    its own object, $7C, area 10's first boss shot down on the way). There the player's run is made a fresh one:
 *    score 0, the chain gone, 3 lives and 3 bombs, the power chosen, the shield taken off.
 */
#ifndef DONPACHI_TRAINING_H
#define DONPACHI_TRAINING_H

#include <stdbool.h>

enum { TRAINING_START, TRAINING_BOSS, TRAINING_TRUE_BOSS, TRAINING_POINTS };

typedef struct
{
	int area;                   /* 0-9 (areas 1-10) */
	int point;                  /* TRAINING_START ... (TRAINING_TRUE_BOSS: area 10 only) */
	int ship;                   /* 0-2: types A, B, C */
	bool max_power;
} training_setup;

/* The game run to the place: NULL (the game is there, between frames: the port saves its state), or what went
   wrong. The game is reset first */
const char *training_prepare(const training_setup *setup);

/* The HUD's player 1 score and HIGH drawn again from their values (the game draws them as they change: the port
   that changes them, as the high score table is put back, calls this) */
void training_hud(void);

/* The place's name for the screen ("AREA 3 BOSS") */
const char *training_name(const training_setup *setup);

#endif
