/**
 *    Name:        game_mode.h
 *    Description: the game's mode ($1018EC), as the port's watchers read it: a game being played, whoever plays it
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 *    The program's own values: 7 player 1 playing alone, 6 player 2 alone, 0 both (and the title screen as well:
 *    told apart by the lives, none there), 0xB the ship select, 8 the continue count, 1 and 5 the attract mode's
 *    demonstration, 2 and 4 the coins and start. A second player joining a game turns 7 into 0; the first one out
 *    of two turns it into 6 or 7
 */
#ifndef GAME_MODE_H
#define GAME_MODE_H

#include "cave/machine.h"

#include <stdbool.h>
#include <stdint.h>

#define GAME_MODE_ADDRESS 0x1018ECu
#define GAME_MODE_PLAY    7u        /* a game played: game_mode_now()'s value for 7, 6 and 0 in a game */

/* The mode as the program has it */
static inline unsigned game_mode_raw(void)
{
	return *(const uint16_t *)cave_work_ram_at(GAME_MODE_ADDRESS, 2);
}

/* A game is being played (one player, the other or both) */
static inline bool game_mode_playing(void)
{
	const unsigned mode = game_mode_raw();

	return mode == 7u || mode == 6u
		|| (mode == 0u && (*cave_work_ram_byte(0x101A2Fu) || *cave_work_ram_byte(0x101A31u)));    /* (the lives) */
}

/* The mode, a game played (7, 6, or 0 in a game) as GAME_MODE_PLAY */
static inline unsigned game_mode_now(void)
{
	return game_mode_playing() ? GAME_MODE_PLAY : game_mode_raw();
}

#endif
