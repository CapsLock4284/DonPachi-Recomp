/**
 *    Name:        achievements.c
 *    Description: goals met in the arcade game, kept in donpachi.achievements (achievements.h)
 *    Version:     0.1.0 (draft)
 *    Created:     10-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's memory they are read from (see TODO.md and widgets.c for how each was found):
 *      $1018EC the game's mode (0xB ship select, 7 playing, 8 the continue count)
 *      $101A40 the stage (0-4; 5 between the loops and the ending), $101A42 the loop
 *      $101A2E / $101A30 the lives; $102158 / $102198 the ships' records (+0 bit 15: flying; +$21 the power, 4 the
 *      most; +$22 the bombs); $101A8A / $101A8C the bomb slots (3-7); $106E14 bit 15 a bomb going off
 *      $10379A / $10379E the bees taken in the area; $10174E / $10176C the chain's hits; $10177A / $10177E the
 *      scores (BCD); $101A32 the rank (its low byte, 0-63); $101284 the difficulty (its low 2 bits)
 *      $100102 the objects (64 of $40 bytes: +0 bit 15 in use, +C's high byte the type; $7C the true last boss)
 */
#include "achievements.h"

#include "cave/machine.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
	A_AREA1, A_LOOP1, A_ALL, A_TRUE_BOSS, A_ONE_CREDIT, A_ONE_CREDIT_ALL, A_NO_MISS_AREA, A_NO_MISS_LOOP,
	A_NO_BOMB_AREA, A_BEES_AREA, A_BEES_LOOP, A_CHAIN30, A_CHAIN100, A_CHAIN200, A_SCORE1M, A_SCORE5M,
	A_SCORE10M, A_MAX_RANK, A_FULL_POWER, A_BOMB_SLOTS, A_EXTEND, A_TEAM, A_HARD, ACHIEVEMENTS
};

static const struct { const char *id, *name, *text; } list[ACHIEVEMENTS] = {
	{ "area1",          "AREA 1 CLEAR",     "CLEAR AREA 1" },
	{ "loop1",          "FIRST LOOP",       "CLEAR AREA 5" },
	{ "all_clear",      "ALL CLEAR",        "CLEAR AREA 10" },
	{ "true_boss",      "THE TRUE BOSS",    "REACH THE TRUE LAST BOSS" },
	{ "one_credit",     "ONE CREDIT",       "AREA 5 WITHOUT CONTINUES" },
	{ "one_credit_all", "ONE CREDIT CLEAR", "AREA 10 WITHOUT CONTINUES" },    /* (the id kept: saved files) */
	{ "no_miss_area",   "UNTOUCHABLE",      "CLEAR AN AREA WITHOUT DYING" },
	{ "no_miss_loop",   "PERFECT LOOP",     "AREAS 1 TO 5 WITHOUT DYING" },
	{ "no_bomb_area",   "NO BOMBS NEEDED",  "CLEAR AN AREA WITHOUT BOMBS" },
	{ "bees_area",      "BEE KEEPER",       "ALL 13 BEES IN AN AREA" },
	{ "bees_loop",      "QUEEN BEE",        "ALL BEES IN AREAS 1 TO 5" },
	{ "chain30",        "CHAIN 30",         "A CHAIN OF 30 HITS" },
	{ "chain100",       "CHAIN 100",        "A CHAIN OF 100 HITS" },
	{ "chain200",       "CHAIN 200",        "A CHAIN OF 200 HITS" },
	{ "score1m",        "ONE MILLION",      "SCORE 1 MILLION" },
	{ "score5m",        "FIVE MILLION",     "SCORE 5 MILLION" },
	{ "score10m",       "TEN MILLION",      "SCORE 10 MILLION" },
	{ "max_rank",       "DANGER ZONE",      "REACH RANK 63" },
	{ "full_power",     "FULL POWER",       "POWER UP ALL THE WAY" },
	{ "bomb_slots",     "BOMB HOARDER",     "REACH 7 BOMB SLOTS" },
	{ "extend",         "EXTRA LIFE",       "EARN AN EXTRA LIFE" },
	{ "team",           "TEAM WORK",        "CLEAR AN AREA WITH 2 PLAYERS" },
	{ "hard",           "HARD WAY",         "CLEAR AREA 5 ON HARD OR UP" },
};

static char dates[ACHIEVEMENTS][16];    /* when each was earned ("" not yet) */
static char file_path[1024];
static int shown_queue[ACHIEVEMENTS], queued;

/* ---- the game's memory ---- */

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

static unsigned byte(uint32_t address)
{
	return *cave_work_ram_byte(address);
}

static uint32_t bcd_long(uint32_t address)
{
	const uint32_t bcd = (uint32_t)word(address) << 16 | word(address + 2u);
	uint32_t value = 0;

	for (int digit = 7; digit >= 0; digit--)
		value = value * 10u + (bcd >> (digit * 4) & 15u);
	return value;
}

static const uint32_t records[2] = { 0x102158u, 0x102198u };

static bool flying(int player)
{
	return (word(records[player]) & 0x8000u) != 0;
}

static bool in_play(int player)
{
	return flying(player) || bcd_long(0x10177Au + 4u * (unsigned)player) != 0;
}

static int area_now(void)
{
	const unsigned stage = word(0x101A40u), loop = word(0x101A42u);

	return stage < 5u && loop < 2u ? (int)(loop * 5u + stage) : -1;
}

static bool object_up(unsigned type)
{
	for (uint32_t object = 0x100102u; object < 0x100102u + 64u * 0x40u; object += 0x40u)
		if ((word(object) & 0x8000u) && (word(object + 0xCu) >> 8) == type)
			return true;
	return false;
}

/* ---- the file ---- */

static void save(void)
{
	FILE *const file = file_path[0] ? fopen(file_path, "w") : NULL;

	if (!file)
		return;
	fputs("; DonPachi's achievements: when each was earned\n[achievements]\n", file);
	for (int index = 0; index < ACHIEVEMENTS; index++)
		if (dates[index][0])
			fprintf(file, "%s = %s\n", list[index].id, dates[index]);
	fclose(file);
}

void achievements_load(const char *path)
{
	char line[256];
	FILE *file;

	snprintf(file_path, sizeof(file_path), "%s", path);
	if (!(file = fopen(path, "r")))
		return;
	while (fgets(line, sizeof(line), file))
	{
		char id[64], date[16];

		if (sscanf(line, " %63[a-z0-9_] = %15s", id, date) != 2)
			continue;
		for (int index = 0; index < ACHIEVEMENTS; index++)
			if (!strcmp(id, list[index].id))
				snprintf(dates[index], sizeof(dates[index]), "%s", date);
	}
	fclose(file);
}

static void earn(int index)
{
	time_t now = time(NULL);
	const struct tm *const local = localtime(&now);

	if (dates[index][0])
		return;
	if (local)
		strftime(dates[index], sizeof(dates[index]), "%Y-%m-%d", local);
	else
		snprintf(dates[index], sizeof(dates[index]), "yes");
	if (queued < ACHIEVEMENTS)
		shown_queue[queued++] = index;
	save();
}

/* ---- the game, watched ---- */

static struct
{
	bool on;                    /* a player's game, from its ship select */
	bool tainted;               /* training, a cheat or a save state somewhere in it */
	bool continued;
	int area;                   /* the area being played (-1 none) */
	unsigned misses, bombs, bees;   /* in this area */
	unsigned loop_misses;       /* in areas 1-5 */
	unsigned all_bees;          /* areas of the first loop with all 13 */
	bool two_players;           /* both in play during this area */
	unsigned play_frames;       /* frames of play in a row (the ship select leaves its own values a moment) */
	unsigned last_mode, last_lives[2], last_bees[2], last_bombs[2];
	bool last_bomb_on;
} game;

static void game_begin(void)
{
	memset(&game, 0, sizeof(game));
	game.on = true;
	game.area = -1;
	game.last_mode = 0xBu;
}

static void area_begin(int area)
{
	game.area = area;
	game.misses = game.bombs = game.bees = 0;
	game.two_players = false;
}

/* the area 'area' cleared (the stage moved on from it as it was played) */
static void area_cleared(int area)
{
	earn(A_AREA1);    /* (area 1, or any after it: area 1 was cleared on the way) */
	if (game.misses == 0)
		earn(A_NO_MISS_AREA);
	if (game.bombs == 0)
		earn(A_NO_BOMB_AREA);
	if (game.bees >= 13u)
	{
		earn(A_BEES_AREA);
		if (area < 5)
			game.all_bees++;
	}
	if (game.two_players)
		earn(A_TEAM);
	if (area == 4)
	{
		earn(A_LOOP1);
		if (!game.continued)
			earn(A_ONE_CREDIT);
		if (game.loop_misses == 0 && !game.continued)
			earn(A_NO_MISS_LOOP);
		if (game.all_bees >= 5u)
			earn(A_BEES_LOOP);
		if ((word(0x101284u) & 3u) >= 2u)
			earn(A_HARD);
	}
	if (area == 9)
	{
		earn(A_ALL);
		if (!game.continued)
			earn(A_ONE_CREDIT_ALL);
	}
}

void achievements_frame(bool counts)
{
	const unsigned mode = word(0x1018ECu);
	const int area = area_now();
	const bool bomb_on = (word(0x106E14u) & 0x8000u) != 0;

	if (mode == 0xBu && game.last_mode != 0xBu)
		game_begin();    /* (a ship select: a new game) */
	if (!counts)
		game.tainted = true;
	if (mode == 0u || mode == 1u || mode == 5u)
		game.on = false;    /* (the attract mode, or its demonstration) */
	if (!game.on || game.tainted)
	{
		game.last_mode = mode;
		return;
	}
	if (game.last_mode == 8u && mode == 7u)
		game.continued = true;

	/* the area: cleared as the stage moves on from it during play */
	if (area != game.area)
	{
		if (game.area >= 0 && game.last_mode == 7u && (area == game.area + 1 || area == -1))
			area_cleared(game.area);
		if (area >= 0)
			area_begin(area);
		else
			game.area = -1;
	}

	game.play_frames = mode == 7u ? game.play_frames + 1u : 0u;
	if (mode == 7u)
	{
		for (int player = 0; player < 2; player++)
		{
			const unsigned lives = word(0x101A2Eu + 2u * (unsigned)player);
			const unsigned bees = word(0x10379Au + 4u * (unsigned)player);
			const unsigned bombs = byte(records[player] + 0x22u);

			if (game.last_mode == 7u)
			{
				if (lives < game.last_lives[player] && !flying(player))
				{
					game.misses++;
					if (area >= 0 && area < 5)
						game.loop_misses++;
				}
				/* more lives: one while flying, an extend; else a continue (or player 2 joining: a credit too) */
				if (lives > game.last_lives[player])
				{
					if (flying(player) && lives == game.last_lives[player] + 1u)
						earn(A_EXTEND);
					else
						game.continued = true;
				}
				if (bees > game.last_bees[player])
					game.bees += bees - game.last_bees[player];
				if (bomb_on && !game.last_bomb_on && bombs + 1u == game.last_bombs[player])
					game.bombs++;
			}
			game.last_lives[player] = lives;
			game.last_bees[player] = bees;
			game.last_bombs[player] = bombs;
			if (!in_play(player) || game.play_frames < 60u)
				continue;
			{
				const unsigned hits = word(0x10174Eu + 0x1Eu * (unsigned)player);
				const uint32_t score = bcd_long(0x10177Au + 4u * (unsigned)player);

				if (hits >= 30u)
					earn(A_CHAIN30);
				if (hits >= 100u)
					earn(A_CHAIN100);
				if (hits >= 200u)
					earn(A_CHAIN200);
				if (score >= 1000000u)
					earn(A_SCORE1M);
				if (score >= 5000000u)
					earn(A_SCORE5M);
				if (score >= 10000000u)
					earn(A_SCORE10M);
				if (byte(records[player] + 0x21u) >= 4u)
					earn(A_FULL_POWER);
				if (byte(0x101A8Au + 2u * (unsigned)player) >= 7u)
					earn(A_BOMB_SLOTS);
			}
		}
		if (in_play(0) && in_play(1))
			game.two_players = true;
		if ((word(0x101A32u) & 0xFFu) >= 63u)
			earn(A_MAX_RANK);
		if (object_up(0x7Cu))
			earn(A_TRUE_BOSS);
	}
	game.last_bomb_on = bomb_on;
	game.last_mode = mode;
}

/* ---- for the menu and the pop-up ---- */

const char *achievements_next_shown(void)
{
	static int showing;

	if (queued == 0)
		return NULL;
	showing = shown_queue[0];
	memmove(shown_queue, shown_queue + 1, sizeof(shown_queue[0]) * (size_t)(--queued));
	return list[showing].name;
}

int achievements_count(void)
{
	return ACHIEVEMENTS;
}

int achievements_done(void)
{
	int done = 0;

	for (int index = 0; index < ACHIEVEMENTS; index++)
		done += dates[index][0] != '\0';
	return done;
}

const char *achievements_name(int index)
{
	return index >= 0 && index < ACHIEVEMENTS ? list[index].name : "";
}

const char *achievements_text(int index)
{
	return index >= 0 && index < ACHIEVEMENTS ? list[index].text : "";
}

const char *achievements_date(int index)
{
	return index >= 0 && index < ACHIEVEMENTS && dates[index][0] ? dates[index] : NULL;
}
