/**
 *    Name:        autobomb.c
 *    Description: the bomb set off for a player whose ship is about to be shot down (autobomb.h)
 *    Version:     0.1.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's memory it reads: $1018EC the game's mode (7 playing); $102158 / $102198 the ships' records, +0 bit 15
 *    flying (cleared as the ship is shot down), +$22 the bombs left.
 */
#include "autobomb.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/sound.h"

#include <stddef.h>
#include <stdint.h>

#define MODE        0x1018ECu
#define PLAYING     7u
#define TRY_FRAMES  (AUTOBOMB_AHEAD + 2)    /* with the bomb: the ship still flying this long */

static const uint32_t records[2] = { 0x102158u, 0x102198u };
static unsigned set_off;

static bool flying(int player)
{
	return (*(const uint16_t *)cave_work_ram_at(records[player], 2) & 0x8000u) != 0;
}

static unsigned bombs(int player)
{
	return *(const uint16_t *)cave_work_ram_at(records[player] + 0x22u, 2);
}

/* the board's inputs: player 0's or 1's (active low) */
static uint16_t *inputs(int player)
{
	return player ? &cave_board.in1 : &cave_board.in0;
}

/* 'frames' frames from where the board is, the inputs as they are now ('bomb_for' >= 0: that player's bomb button
   pressed in the first of them): the frame (1-) each player's ship was shot down in, 0 if it wasn't */
static void run_ahead(int frames, int bomb_for, int shot_down[2])
{
	const uint16_t in0 = cave_board.in0, in1 = cave_board.in1;

	shot_down[0] = shot_down[1] = 0;
	for (int frame = 1; frame <= frames; frame++)
	{
		if (frame == 1 && bomb_for >= 0)
			*inputs(bomb_for) &= (uint16_t)~CAVE_BUTTON2;
		program_run_frame();
		cave_board.in0 = in0;
		cave_board.in1 = in1;
		cave_sound_drop();    /* (what it played isn't heard) */
		for (int player = 0; player < 2; player++)
			if (!shot_down[player] && !flying(player))
				shot_down[player] = frame;
	}
}

bool autobomb_frame(bool player2)
{
	const int players = player2 ? 2 : 1;
	bool watch[2] = { false, false }, any = false, pressed = false;

	if (*(const uint16_t *)cave_work_ram_at(MODE, 2) != PLAYING)
		return false;
	for (int player = 0; player < players; player++)
	{
		/* (its bomb button already held: the player's own) */
		watch[player] = flying(player) && bombs(player) > 0u && (*inputs(player) & CAVE_BUTTON2);
		any |= watch[player];
	}
	if (!any)
		return false;
	{
		void (*const after_vblank)(void) = program_after_vblank;
		const uint16_t in0 = cave_board.in0, in1 = cave_board.in1;
		program_state *const now = program_state_save();
		int shot_down[2];

		if (!now)
			return false;
		program_after_vblank = NULL;    /* (nothing drawn while it looks ahead) */
		run_ahead(AUTOBOMB_AHEAD, -1, shot_down);
		program_state_load(now);
		for (int player = 0; player < players; player++)
		{
			int with_bomb[2];

			/* shot down in this very frame: too late for a bomb (the game has the button a frame later) */
			if (!watch[player] || shot_down[player] < 2)
				continue;
			run_ahead(TRY_FRAMES, player, with_bomb);
			program_state_load(now);
			if (!with_bomb[player])
			{
				*inputs(player) &= (uint16_t)~CAVE_BUTTON2;
				pressed = true;
				set_off++;
			}
		}
		program_after_vblank = after_vblank;
		program_state_free(now);
		/* (the board as it was: the inputs with the bombs that save) */
		if (!pressed)
		{
			cave_board.in0 = in0;
			cave_board.in1 = in1;
		}
	}
	return pressed;
}

unsigned autobomb_count(void)
{
	return set_off;
}
