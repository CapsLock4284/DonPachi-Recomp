/**
 *    Name:        training.c
 *    Description: the main menu's TRAINING (training.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "training.h"
#include "game_mode.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/sound.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GAME_MODE     0x1018ECu   /* 0xB the ship select, 7 playing */
#define STAGE         0x101A40u
#define LOOP          0x101A42u
#define SHIP          0x102158u   /* player 1's ship's record */
#define SHIELD        0x102196u   /* (byte) the ship's respawn shield: frames it cannot be hit */
#define POWER         0x102179u   /* (byte) 0-4: shot levels 1-4, MAX */
#define POWER_HALF    0x102194u   /* the power-ups toward the next level: each 2 one more */
#define MAX_POWER     0x101A86u   /* 1 at MAX (with the ship's record's bit 10): a power-up then the bonus */
#define SCROLL        0x101A56u   /* the area's way on (below 0 as it starts) */
#define BEES_TAKEN    0x10379Au   /* the bees taken in the area */
#define BULLETS       0x104E94u   /* the enemies' bullets: 75 of $40 bytes, +0 not 0 in use */
#define BOMBS         0x10217Bu   /* (byte) */
#define LIVES         0x101A2Fu   /* (byte) */
#define SCORE         0x10177Au   /* (BCD long) */
#define BOMB_SLOTS    0x101A8Au   /* (byte) player 1's bomb slots, 3-7; the next byte the bombs used toward one more */

/* the game's own: the HUD's lives and bombs drawn from the ship's record (its slots +$24, its bombs +$22) */
void draw_lives_and_bombs(uint32_t a5, uint32_t a6);
#ifdef DONPACHI_JAPAN_PROGRAM
void jpn_draw_lives_and_bombs(uint32_t a5, uint32_t a6);    /* (the Japanese program's: src_jpn/jpn_names.h) */
#endif
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

/* The stick towards the strongest enemy on the screen, to shoot it down (area 10's first boss for the true boss;
   the way to a mid-boss): the collision records as the game's hit test reads them, $103814 (100) and $104494 (50,
   the bosses'), $20 bytes each: +0 bits 15 and 13 in play and can be shot, +6 y, +8 x, +$18 hit points */
static unsigned towards_target(void)
{
	int most = 0, target = -1;

	for (int record = 0; record < 150; record++)
	{
		const uint32_t at = record < 100 ? 0x103814u + 0x20u * (uint32_t)record
			: 0x104494u + 0x20u * (uint32_t)(record - 100);
		const int hit_points = (int16_t)word(at + 0x18u), x = (int16_t)word(at + 8u), y = (int16_t)word(at + 6u);

		if ((word(at) & 0xA000u) == 0xA000u && hit_points > most && y >= 0 && y < 0x5000 && x >= 0 && x < 0x3C00)
		{
			most = hit_points;
			target = x;
		}
	}
	if (target >= 0)
	{
		const int ship = (int16_t)word(SHIP + 8u);

		if (target > ship + 256)
			return CAVE_RIGHT;
		if (target < ship - 256)
			return CAVE_LEFT;
	}
	return 0;
}

/* The stick towards the clearest place on the ship's row: the x (in steps of 4 pixels) farthest from any enemy on
   the screen (by its hit record's x and y, $40 to the pixel), so the hand-over does not leave the ship in a swarm
   (area 4's: the shield taken off, the ship was hit at once) */
static unsigned towards_clear(void)
{
	const int ship = (int16_t)word(SHIP + 8u), row = (int16_t)word(SHIP + 6u);
	int best = ship, clearest = -1;

	for (int x = 0x600; x <= 0x3600; x += 0x100)
	{
		int nearest = 1 << 30;

		for (int record = 0; record < 150; record++)
		{
			const uint32_t at = record < 100 ? 0x103814u + 0x20u * (uint32_t)record
				: 0x104494u + 0x20u * (uint32_t)(record - 100);
			const int ex = (int16_t)word(at + 8u), ey = (int16_t)word(at + 6u);

			if ((word(at) & 0x8000u) && ey >= 0 && ey < 0x5000 && ex >= 0 && ex < 0x3C00)
			{
				const int distance = abs(ex - x) + abs(ey - row);

				if (distance < nearest)
					nearest = distance;
			}
		}
		/* (the farthest; of those as far, the nearest to the ship) */
		if (nearest > clearest || (nearest == clearest && abs(x - ship) < abs(best - ship)))
		{
			clearest = nearest;
			best = x;
		}
	}
	if (best > ship + 128)
		return CAVE_RIGHT;
	if (best < ship - 128)
		return CAVE_LEFT;
	return 0;
}

/* The calm spot before each area's mid-boss (the same in both loops): the scroll the way there stops at */
static const int calm_spot[5] = { 490, 317, 258, 376, 335 };    /* (area 4: before its swarm, 378-391) */
#define CEASE_FIRE    4           /* the fire stopped this much of the scroll before it (about 1.5 seconds): the shots
                                     and the explosions, their points, over by the place */

/* The STAGE PROGRESS bar's (widgets.c): the scroll where each area's boss appears, and where its own scrolling
   starts (area 1's after the ship's fly-in); the same in both loops */
static const int boss_at[5] = { 555, 576, 629, 888, 539 };
static const int start_at[5] = { 256, 0, 0, 0, 0 };

int training_part_way_scroll(int stage, int percent)
{
	const int end = boss_at[stage] - start_at[stage];

	/* (the first scroll the bar shows as 'percent': it counts scroll * 100 / end down) */
	return start_at[stage] + (percent * end + 99) / 100;
}

/* the percentage kept to the steps the menu offers */
static int part_way_percent(const training_setup *setup)
{
	const int percent = setup->percent / TRAINING_PERCENT_STEP * TRAINING_PERCENT_STEP;

	return percent < TRAINING_PERCENT_STEP ? TRAINING_PERCENT_STEP
		: percent > TRAINING_PERCENT_MOST ? TRAINING_PERCENT_MOST : percent;
}

/* ON PACE: the power-ups taken on the way to the place, areas 1 and 2 (from area 3 on, MAX: 8 or more): at the
   start, the calm spot before the mid-boss, the boss (the power-ups there are, with the ship shooting all it can) */
static const int on_pace[2][3] = { { 0, 2, 5 }, { 5, 7, 9 } };

/* the power-ups for the setup's power: levels 2 each */
static int power_ups(const training_setup *setup)
{
	if (setup->power == TRAINING_POWER_ON_PACE)
	{
		/* (part way: the start's before the mid-boss's calm spot, the mid-boss's after it) */
		const int point = setup->point == TRAINING_PART_WAY
			? (training_part_way_scroll(setup->area % 5, part_way_percent(setup)) < calm_spot[setup->area % 5]
				? TRAINING_START : TRAINING_MIDBOSS)
			: setup->point > TRAINING_BOSS ? TRAINING_BOSS : setup->point;

		return setup->area < 2 ? on_pace[setup->area][point] : 8;
	}
	return setup->power >= TRAINING_POWER_MAX ? 8 : 2 * setup->power;
}

const char *training_power_name(const training_setup *setup)
{
	static char name[16];
	static const char *const levels[4] = { "1", "2", "3", "4" };
	const int ups = power_ups(setup);
	const char *const level = ups >= 8 ? "MAX" : levels[ups / 2];

	if (setup->power == TRAINING_POWER_ON_PACE)
		snprintf(name, sizeof(name), "ON PACE (%s)", level);
	else
		snprintf(name, sizeof(name), "%s", level);
	return name;
}

const char *training_name(const training_setup *setup)
{
	static char name[40];
	static const char *const points[TRAINING_POINTS] = { "", " MID-BOSS", " BOSS", " TRUE BOSS", "" };

	if (setup->point == TRAINING_PART_WAY)
		snprintf(name, sizeof(name), "AREA %d %d%%", setup->area + 1, part_way_percent(setup));
	else
		snprintf(name, sizeof(name), "AREA %d%s", setup->area + 1, points[setup->point]);
	return name;
}

/* A number of the HUD's (BCD) into its sprites' list, as the game makes it when the number changes (draw_hud_number):
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

static const char *run_to_place(const training_setup *setup);

const char *training_difficulty_name(int difficulty)
{
	static const char *const names[4] = { "NORMAL", "EASY", "HARD", "VERY HARD" };

	return names[difficulty & 3];
}

/* The difficulty is the test mode's, in the EEPROM (word 4, bits 8-9): the game reads it as it boots, into
   $101288 (the rank's base and the bullets from it). The training game boots with the training's, then the
   EEPROM has the player's own back at once: the file never gets the training's, nor does the save state made at
   the place (the game keeps what it read) */
const char *training_prepare(const training_setup *setup)
{
	const uint16_t own = cave_board.eeprom.words[4];
	const char *problem;

	cave_board.eeprom.words[4] = (uint16_t)((own & ~0x0300u) | (unsigned)(setup->difficulty & 3) << 8);
	problem = run_to_place(setup);
	cave_board.eeprom.words[4] = own;
	return problem;
}

static const char *run_to_place(const training_setup *setup)
{
	const unsigned stage = (unsigned)(setup->area % 5), loop = (unsigned)(setup->area / 5);
	const int point = setup->point == TRAINING_TRUE_BOSS && setup->area != 9 ? TRAINING_BOSS : setup->point;
	/* the scroll a mid-boss's or a point part way's run hands over at (-1: none, the other places) */
	const int handover = point == TRAINING_MIDBOSS ? calm_spot[stage]
		: point == TRAINING_PART_WAY ? training_part_way_scroll((int)stage, part_way_percent(setup)) : -1;
	long frame = 0, select_frames = 0;
	bool playing = false, area1_boss_seen = false, sent = setup->area == 0, done = false, begun = false;
	unsigned bees = 0;

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
			if (sent && !begun && area_now() == setup->area && (setup->area == 0 || (int16_t)word(SCROLL) < 0))
			{
				begun = true;    /* (the place's area begun: its bees taken so far, the run's own) */
				bees = word(BEES_TAKEN);
			}
			if (sent && area_now() == setup->area && point == TRAINING_START
					&& (setup->area == 0 || (int16_t)word(SCROLL) < 0))
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
				if ((point == TRAINING_TRUE_BOSS && sent)
					|| (handover >= 0 && begun && (int16_t)word(SCROLL) < handover - CEASE_FIRE))
				{
					set_byte(POWER, 4u);
					held = ((frame / 2) % 2 ? CAVE_BUTTON1 : 0u) | towards_target();
				}
				else if (handover >= 0 && begun)
					held = towards_clear();    /* (the fire stopped: to the hand-over away from the enemies) */
				if (!sent)
				{
					/* area 1 played to its boss's end: then the game's next area is the place's (it adds 1
					   to the stage as area 1 is cleared: the stage before it, and the loop) */
					if (object_up(stage_boss[0]))
						area1_boss_seen = true;
					else if (area1_boss_seen)
					{
						if (setup->area == 5)
						{
							/* area 6: the first loop's end (area 5 cleared), as the game goes to the second:
							   its story between the loops, then area 6 from the dropship, as area 1 */
							set_word(STAGE, 4u);
							set_word(LOOP, 0u);
						}
						else
						{
							set_word(STAGE, (stage - 1u) & 0xFFFFu);
							set_word(LOOP, loop);
						}
						sent = true;
					}
				}
				else if (handover >= 0)
					done = begun && (int16_t)word(SCROLL) >= handover;
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
		if (playing && !game_mode_playing())
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
	for (uint32_t at = 0x10173Au; at < 0x101758u; at += 2u)
		set_word(at, 0);    /* (the chain: the last kill's values, its timer, display, sum, points, hits; the big
		                       "N HIT"'s hits and timer; the best chain) */
	set_byte(LIVES, 3u);
	{
		const unsigned bombs = setup->max_bombs ? 7u : 3u;

		set_byte(BOMB_SLOTS, bombs);
		set_byte(BOMB_SLOTS + 1u, 0u);
		set_word(SHIP + 0x24u, bombs);
		set_word(SHIP + 0x22u, bombs);
#ifdef DONPACHI_JAPAN_PROGRAM
		if (rom_set.version == DONPACHI_JAPAN)
			jpn_draw_lives_and_bombs(0x100082u, SHIP);
		else
#endif
			draw_lives_and_bombs(0x100082u, SHIP);    /* (as the game draws them when they change) */
	}
	{
		/* the power: as the game's power-ups make it (MAX: the flags a power-up then reads for the bonus) */
		const int ups = power_ups(setup) > 8 ? 8 : power_ups(setup);

		set_byte(POWER, (unsigned)ups / 2u);
		set_word(POWER_HALF, ups < 8 ? (unsigned)ups % 2u : 0u);
		set_word(MAX_POWER, ups >= 8 ? 1u : 0u);
		set_word(SHIP, ups >= 8 ? word(SHIP) | 0x400u : word(SHIP) & ~0x400u);
	}
	if (handover >= 0)
	{
		/* the way's fight: the bees it took not the player's, the enemies' bullets taken off (as a bomb's start
		   does, clear_enemy_bullets) */
		set_word(BEES_TAKEN, bees);
		for (uint32_t bullet = 0; bullet < 75u; bullet++)
			set_word(BULLETS + 0x40u * bullet, 0);
	}
	set_byte(SHIELD, 0u);    /* (the shield the way here was held at: the game counts it down only from a respawn) */
	set_byte(0x10139Bu, 0u);    /* (the credits the way here put in beyond the game's) */
	cave_board.in0 = 0xFFFFu;
	return NULL;
}
