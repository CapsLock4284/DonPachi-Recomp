/**
 *    Name:        training.c
 *    Description: the main menu's TRAINING (training.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "training.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/sound.h"

#include <stdint.h>
#include <stdio.h>

#define GAME_MODE     0x1018ECu   /* 0xB the ship select, 7 playing */
#define STAGE         0x101A40u
#define LOOP          0x101A42u
#define SHIP          0x102158u   /* player 1's ship's record */
#define SHIELD        0x102196u   /* (byte) the ship's respawn shield: frames it cannot be hit */
#define POWER         0x102179u   /* (byte) */
#define BOMBS         0x10217Bu   /* (byte) */
#define LIVES         0x101A2Fu   /* (byte) */
#define SCORE         0x10177Au   /* (BCD long) */
#define BOMB_SLOTS    0x101A8Au   /* (byte) player 1's bomb slots, 3-7; the next byte the bombs used toward one more */

/* the game's own: the HUD's lives and bombs drawn from the ship's record (its slots +$24, its bombs +$22) */
void func_005020(uint32_t a5, uint32_t a6);
#define OBJECTS       0x100102u   /* 64 of $40 bytes: +0 bit 15 in use, +C (high byte) the type */
#define MAX_FRAMES    80000L      /* (the longest way there, area 10's true boss, is about 30000) */

static const unsigned stage_boss[5] = { 0x31u, 0x58u, 0x4Bu, 0x3Bu, 0x6Bu };    /* (area 1's: the way on) */
#define TRUE_BOSS     0x7Cu
#define WARNING       0x3Fu       /* the "WARNING" before a stage's boss (none before a mid-boss; also the legal screen's
                                     at boot, before the game) */

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

static void set_word(uint32_t address, unsigned value)
{
	*(uint16_t *)cave_work_ram_at(address, 2) = (uint16_t)value;
}

static void set_byte(uint32_t address, unsigned value)
{
	*cave_work_ram_byte(address) = (uint8_t)value;
}

/* an object of the type in play */
static bool object_up(unsigned type)
{
	for (unsigned slot = 0; slot < 64u; slot++)
	{
		const uint32_t object = OBJECTS + 0x40u * slot;

		if ((word(object) & 0x8000u) && word(object + 0xCu) >> 8 == type)
			return true;
	}
	return false;
}

/* (the true boss: the stick towards area 10's first boss, to shoot it down; the collision records as the game's
   hit test reads them, $104494: 50 of $20 bytes, +0 bit 13 can be shot, +8 its x, +$18 its hit points) */
static unsigned towards_target(void)
{
	int best = -1, most = 0;

	for (int record = 0; record < 50; record++)
	{
		const uint32_t at = 0x104494u + 0x20u * (uint32_t)record;
		const int hit_points = (int16_t)word(at + 0x18u);

		if ((word(at) & 0xA000u) == 0xA000u && hit_points > most)
		{
			most = hit_points;
			best = record;
		}
	}
	if (best >= 0)
	{
		const int target = (int16_t)word(0x104494u + 0x20u * (uint32_t)best + 8u), ship = (int16_t)word(SHIP + 8u);

		if (target > ship + 256)
			return CAVE_RIGHT;
		if (target < ship - 256)
			return CAVE_LEFT;
	}
	return 0;
}

const char *training_name(const training_setup *setup)
{
	static char name[40];
	static const char *const points[TRAINING_POINTS] = { "", " BOSS", " TRUE BOSS" };

	snprintf(name, sizeof(name), "AREA %d%s", setup->area + 1, points[setup->point]);
	return name;
}

/* A number of the HUD's (BCD) into its sprites' list, as the game makes it when the number changes (func_006C24):
   8 digits of 8 bytes, the leading zeros none, the last one always */
static void hud_number(uint32_t list, uint32_t value)
{
	bool shown = false;

	for (int digit = 7; digit >= 0; digit--, list += 8u)
	{
		const unsigned nibble = value >> 28;

		value <<= 4;
		if (nibble || shown || digit == 0)
		{
			set_word(list, 0x1330u);
			set_word(list + 2u, 0x31C0u + nibble);
			shown = true;
		}
		else
		{
			set_word(list, 0);
			set_word(list + 2u, 0);
		}
	}
}

void training_hud(void)
{
	/* (player 1's score and the HIGH as the HUD shows them, made again) */
	hud_number(0x101786u, (uint32_t)word(SCORE) << 16 | word(SCORE + 2u));
	hud_number(0x1017C6u, (uint32_t)word(0x101782u) << 16 | word(0x101784u));
}

/* The place's area: on the way there, the stage the game is in (0-4, 5 between) and its loop */
static int area_now(void)
{
	const unsigned stage = word(STAGE), loop = word(LOOP);

	return stage < 5u && loop < 2u ? (int)(loop * 5u + stage) : -1;
}

const char *training_prepare(const training_setup *setup)
{
	const unsigned stage = (unsigned)(setup->area % 5), loop = (unsigned)(setup->area / 5);
	const int point = setup->point == TRAINING_TRUE_BOSS && setup->area != 9 ? TRAINING_BOSS : setup->point;
	long frame = 0, select_frames = 0;
	bool playing = false, area1_boss_seen = false, sent = setup->area == 0, done = false;

	program_reset();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	program_run_frame();    /* (the reset takes place: the board starts again) */
	cave_sound_drop();
	for (; frame < MAX_FRAMES && !done; frame++)
	{
		const unsigned mode = word(GAME_MODE);
		unsigned held = 0;

		if (mode == 0xBu)
		{
			/* the ship select: the ship (down presses), then shot */
			select_frames++;
			if (select_frames > 60 && select_frames <= 60 + 30L * setup->ship && (select_frames - 61) % 30 < 8)
				held = CAVE_DOWN;
			else if (select_frames > 80 + 30L * setup->ship && (select_frames / 8) % 2)
				held = CAVE_BUTTON1;
		}
		else if (mode == 7u && select_frames > 0)
		{
			playing = true;
			if (sent && area_now() == setup->area && point == TRAINING_START
					&& (setup->area == 0 || (int16_t)word(0x101A56u) < 0))
				done = true;    /* (the area's start: its scroll from below 0, as it begins) */
			else
			{
				/* on the way: shielded, not firing (no points: the HUD shows the score only as it changes),
				   without bombs (none for the area's clear to count into the score); area 10's first boss shot
				   down at full power for the true boss */
				set_byte(SHIELD, 2u);
				set_byte(BOMBS, 0u);
				set_word(SCORE, 0);    /* (and no score: the HIGH would go up with it) */
				set_word(SCORE + 2u, 0);
				if (point == TRAINING_TRUE_BOSS && sent)
				{
					set_byte(POWER, 4u);
					held = ((frame / 2) % 2 ? CAVE_BUTTON1 : 0u) | towards_target();
				}
				if (!sent)
				{
					/* area 1 played to its boss's end: then the game's next area is the place's (it adds 1
					   to the stage as area 1 is cleared: the stage before it, and the loop) */
					if (object_up(stage_boss[0]))
						area1_boss_seen = true;
					else if (area1_boss_seen)
					{
						set_word(STAGE, (stage - 1u) & 0xFFFFu);
						set_word(LOOP, loop);
						sent = true;
					}
				}
				else if (area_now() == setup->area)
					done = point == TRAINING_TRUE_BOSS ? object_up(TRUE_BOSS) : object_up(WARNING);
			}
		}
		else if (frame > 400 && !playing)
		{
			/* the attract mode: a coin, then Start, until the ship select comes */
			const long beat = frame % 40;

			if (beat < 5)
				held = CAVE_COIN;
			else if (beat >= 20 && beat < 25)
				held = CAVE_START;
		}
		if (done)
			break;
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		if (playing && word(GAME_MODE) != 7u)
			return "the game left the stage on the way (game over?)";
	}
	if (!done)
		return "the place was not reached";
	/* a fresh run from here: score 0, no chain, 3 lives and 3 bombs, the power chosen, no shield */
	set_word(SCORE, 0);
	set_word(SCORE + 2u, 0);
	set_word(0x10190Eu, 0);    /* (the score the HUD shows when it is more than the score: the run's best) */
	set_word(0x101910u, 0);
	for (uint32_t at = 0; at < 4u; at++)    /* (the HUD's HIGH: the table's #1 again, not the way here's score) */
		*cave_work_ram_byte(0x101782u + at) = *cave_work_ram_byte(0x101846u + at);
	training_hud();
	for (uint32_t at = 0x10173Au; at < 0x101750u; at += 2u)
		set_word(at, 0);    /* (the chain: the last kill's values, its timer, display, sum, points, hits) */
	set_word(0x101754u, 0);    /* (the best chain) */
	set_byte(LIVES, 3u);
	{
		const unsigned bombs = setup->max_bombs ? 7u : 3u;

		set_byte(BOMB_SLOTS, bombs);
		set_byte(BOMB_SLOTS + 1u, 0u);
		set_word(SHIP + 0x24u, bombs);
		set_word(SHIP + 0x22u, bombs);
		func_005020(0x100082u, SHIP);    /* (as the game draws them when they change) */
	}
	set_byte(POWER, setup->max_power ? 4u : 0u);
	set_byte(SHIELD, 0u);    /* (the shield the way here was held at: the game counts it down only from a respawn) */
	set_byte(0x10139Bu, 0u);    /* (the credits the way here put in beyond the game's) */
	cave_board.in0 = 0xFFFFu;
	return NULL;
}
