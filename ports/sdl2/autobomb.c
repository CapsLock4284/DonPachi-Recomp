/**
 *    Name:        autobomb.c
 *    Description: the bomb set off for a player whose ship is about to be shot down (autobomb.h)
 *    Version:     0.1.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's memory it reads: $1018EC the game's mode (7 playing); $102158 / $102198 the ships' records, +0 bit 15
 *    flying (cleared as the ship is shot down), +$22 the bombs left; $106E14 bit 15 a bomb going off, with bit 8 a
 *    normal one (all around the ship: $8100), without it the laser's (a column of fire ahead of the ship: the shot
 *    held into the bomb; its low bits change as it goes).
 *    The laser's bomb: a bomb pressed while the laser is on (the shot held a while). Letting the shot go for one frame
 *    before the bomb makes it a normal bomb, all around the ship (the shot may be held again in the bomb's own frame).
 *    So where a bomb now would be the laser's, it lets the shot go now and presses the bomb next frame, if the ship
 *    is still saved that way; only if it isn't (the hit a frame too near) does it set off the laser's bomb.
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
#define BOMB        0x106E14u               /* bit 15 a bomb going off; bit 8 a normal one (clear: the laser's) */

static const uint32_t records[2] = { 0x102158u, 0x102198u };

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

/* a bomb going off, and a laser bomb (the shot held into it: the laser's bomb, a column ahead of the ship) */
static bool laser_bomb(void)
{
	const unsigned bomb = *(const uint16_t *)cave_work_ram_at(BOMB, 2);

	return (bomb & 0x8000u) && !(bomb & 0x0100u);
}

/* 'frames' frames from where the board is, the inputs as they are now; for player 'plan_for' (or -1: none) its shot
   let go in frame 'let_go' (0: not) and its bomb pressed in frame 'bomb_at' (0: not). The frame (1-) each player's ship
   was shot down in, 0 if it wasn't; and whether the bomb was a laser bomb */
static void run_ahead(int frames, int plan_for, int let_go, int bomb_at, int shot_down[2], bool *laser)
{
	const uint16_t in0 = cave_board.in0, in1 = cave_board.in1;

	shot_down[0] = shot_down[1] = 0;
	for (int frame = 1; frame <= frames; frame++)
	{
		if (plan_for >= 0 && frame == let_go)
			*inputs(plan_for) |= CAVE_BUTTON1;    /* (active low: let go) */
		if (plan_for >= 0 && frame == bomb_at)
			*inputs(plan_for) &= (uint16_t)~CAVE_BUTTON2;
		program_run_frame();
		cave_board.in0 = in0;
		cave_board.in1 = in1;
		cave_sound_drop();    /* (what it played isn't heard) */
		for (int player = 0; player < 2; player++)
			if (!shot_down[player] && !flying(player))
				shot_down[player] = frame;
	}
	if (laser)
		*laser = laser_bomb();
}

static bool looking;       /* in its look ahead (crash_log.c) */
static bool pending[2];    /* the shot let go last frame: the bomb this one (a normal bomb, not the laser's) */
static unsigned set_off, set_off_laser;

bool autobomb_frame(bool player2)
{
	const int players = player2 ? 2 : 1;
	bool watch[2] = { false, false }, any = false, pressed = false;

	/* (a reset or a save state asked for: looking ahead would use it up and put the board back before it) */
	if (*(const uint16_t *)cave_work_ram_at(MODE, 2) != PLAYING || program_change_pending())
	{
		pending[0] = pending[1] = false;
		return false;
	}
	for (int player = 0; player < players; player++)
	{
		if (pending[player])
		{
			/* the bomb planned last frame, its shot let go then (the shot may be held again now) */
			pending[player] = false;
			if (flying(player) && bombs(player) > 0u)
			{
				*inputs(player) &= (uint16_t)~CAVE_BUTTON2;
				set_off++;
				pressed = true;
			}
			continue;
		}
		/* (its bomb button already held: the player's own) */
		watch[player] = flying(player) && bombs(player) > 0u && (*inputs(player) & CAVE_BUTTON2);
		any |= watch[player];
	}
	if (!any)
		return pressed;
	{
		void (*const after_vblank)(void) = program_after_vblank;
		program_state *const now = program_state_save();
		int shot_down[2];

		if (!now)
			return pressed;
		program_after_vblank = NULL;    /* (nothing drawn while it looks ahead) */
		looking = true;
		run_ahead(AUTOBOMB_AHEAD, -1, 0, 0, shot_down, NULL);
		program_state_load(now);
		for (int player = 0; player < players; player++)
		{
			int with_bomb[2];
			bool laser, bomb_now = false;

			/* shot down in this very frame: too late for a bomb (the game has the button a frame later) */
			if (!watch[player] || shot_down[player] < 2)
				continue;
			/* the bomb now */
			run_ahead(TRY_FRAMES, player, 0, 1, with_bomb, &laser);
			program_state_load(now);
			bomb_now = !with_bomb[player];
			if (bomb_now && !laser)
			{
				*inputs(player) &= (uint16_t)~CAVE_BUTTON2;
				set_off++;
				pressed = true;
				continue;
			}
			/* it would be the laser's bomb (the shot held into it): the shot let go now and the bomb next frame
			   makes it a normal one, if there is time for that */
			if (shot_down[player] >= 3 && !(*inputs(player) & CAVE_BUTTON1))
			{
				run_ahead(TRY_FRAMES + 1, player, 1, 2, with_bomb, &laser);
				program_state_load(now);
				if (!with_bomb[player] && !laser)
				{
					*inputs(player) |= CAVE_BUTTON1;
					pending[player] = true;
					continue;
				}
			}
			/* only the laser's bomb saves the ship now: that rather than a miss */
			if (bomb_now)
			{
				*inputs(player) &= (uint16_t)~CAVE_BUTTON2;
				set_off++;
				set_off_laser++;
				pressed = true;
			}
		}
		program_after_vblank = after_vblank;
		looking = false;
		program_state_free(now);
	}
	return pressed;
}

void autobomb_reset(void)
{
	pending[0] = pending[1] = false;
}

bool autobomb_looking(void)
{
	return looking;
}

unsigned autobomb_count(void)
{
	return set_off;
}

unsigned autobomb_laser_count(void)
{
	return set_off_laser;
}
