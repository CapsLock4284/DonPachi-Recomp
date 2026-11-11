/**
 *    Name:        stats.c
 *    Description: lifetime statistics, kept in donpachi.stats (stats.h)
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's memory they are read from (as achievements.c, and see TODO.md):
 *      $1018EC the game's mode (0xB ship select, 7 playing, 8 the continue count; 0, 1, 5 the attract mode)
 *      $101A40 / $101A42 the stage and loop; $101A2E / $101A30 the lives
 *      $102158 / $102198 the ships' records: +0 bit 15 flying, +$1C the ship (0-2: type A, B, C), +$22 the bombs
 *      $102178 / $1021B8 the power (0-4), $102194 / $1021D4 the power-ups toward the next level (each 2 a level)
 *      $106E14 bit 15 a bomb going off; $10379A / $10379E the bees taken in the area
 *      $101742 / $101760 the chain's timer (30 at each kill, then down a frame); $10174E / $10176C the chain's hits
 *      $10177A / $10177E the scores (BCD); $101A32 the rank (low byte); $101284 the difficulty (low 2 bits)
 *    An enemy destroyed: each kill sets the chain's timer to 30 (the game counts it down the same frame: it reads 29,
 *    so a kill is the timer not going down); the kills of one frame are the chain's hits gone up (a new chain: its
 *    hits, from 0, plus 1).
 */
#include "stats.h"

#include "cave/machine.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AREAS       10
#define SHIPS       3
#define SAVE_EVERY  (57 * 60)    /* frames of play between saves (a minute), besides the areas' and games' ends */

/* ---- what is kept ---- */

enum {
	T_GAMES, T_CREDITS, T_CONTINUES, T_PLAY_FRAMES, T_LOOP_1CC, T_ALL_1CC, T_AREAS_CLEARED, T_BOSSES, T_MIDBOSSES,
	T_TRUE_BOSS, T_KILLS, T_DEATHS, T_EXTENDS, T_BOMBS_USED, T_BEES, T_POWER_UPS, T_BOMB_ITEMS,
	R_BEST_CHAIN, R_LONGEST_LIFE, R_BEST_RANK, R_SCORE_NORMAL, R_SCORE_EASY, R_SCORE_HARD, R_SCORE_VERY_HARD,
	COUNTERS
};

static const char *const counter_ids[COUNTERS] = {
	"games", "credits", "continues", "play_frames", "one_credit_loops", "one_credit_clears", "areas_cleared",
	"bosses", "midbosses", "true_boss", "enemies", "deaths", "extra_lives", "bombs_used", "bees", "power_ups",
	"bomb_items",
	"best_chain", "longest_life_frames", "best_rank", "best_score_normal", "best_score_easy", "best_score_hard",
	"best_score_very_hard"
};

static struct
{
	unsigned long counter[COUNTERS];
	unsigned long area_played[AREAS], area_cleared[AREAS], area_deaths[AREAS], area_best_bees[AREAS];
	unsigned long ship_games[SHIPS], ship_best[SHIPS], ship_frames[SHIPS];
} kept;

static char file_path[1024];

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

static int area_now(void)
{
	const unsigned stage = word(0x101A40u), loop = word(0x101A42u);

	return stage < 5u && loop < 2u ? (int)(loop * 5u + stage) : -1;
}

static void at_least(unsigned long *kept_value, unsigned long value)
{
	if (value > *kept_value)
		*kept_value = value;
}

/* ---- the file ---- */

void stats_save(void)
{
	FILE *const file = file_path[0] ? fopen(file_path, "w") : NULL;

	if (!file)
		return;
	fputs("; DonPachi's lifetime statistics (arcade games only: not training, cheats or save states)\n[stats]\n", file);
	for (int index = 0; index < COUNTERS; index++)
		fprintf(file, "%s = %lu\n", counter_ids[index], kept.counter[index]);
	for (int area = 0; area < AREAS; area++)
		fprintf(file, "area%d = %lu %lu %lu %lu\n", area + 1, kept.area_played[area], kept.area_cleared[area],
			kept.area_deaths[area], kept.area_best_bees[area]);
	for (int ship = 0; ship < SHIPS; ship++)
		fprintf(file, "ship_%c = %lu %lu %lu\n", 'a' + ship, kept.ship_games[ship], kept.ship_best[ship],
			kept.ship_frames[ship]);
	fclose(file);
}

void stats_load(const char *path)
{
	char line[256];
	FILE *file;

	snprintf(file_path, sizeof(file_path), "%s", path);
	if (!(file = fopen(path, "r")))
		return;
	while (fgets(line, sizeof(line), file))
	{
		char id[64];
		unsigned long a, b, c, d;
		int area;
		char ship;

		if (sscanf(line, " area%d = %lu %lu %lu %lu", &area, &a, &b, &c, &d) == 5 && area >= 1 && area <= AREAS)
		{
			kept.area_played[area - 1] = a;
			kept.area_cleared[area - 1] = b;
			kept.area_deaths[area - 1] = c;
			kept.area_best_bees[area - 1] = d;
		}
		else if (sscanf(line, " ship_%c = %lu %lu %lu", &ship, &a, &b, &c) == 4 && ship >= 'a' && ship < 'a' + SHIPS)
		{
			kept.ship_games[ship - 'a'] = a;
			kept.ship_best[ship - 'a'] = b;
			kept.ship_frames[ship - 'a'] = c;
		}
		else if (sscanf(line, " %63[a-z0-9_] = %lu", id, &a) == 2)
			for (int index = 0; index < COUNTERS; index++)
				if (!strcmp(id, counter_ids[index]))
					kept.counter[index] = a;
	}
	fclose(file);
}

/* ---- the game, watched ---- */

static struct
{
	bool on;                    /* a player's game, from its ship select */
	bool tainted;               /* training, a cheat or a save state somewhere in it */
	bool started;               /* its play began (counted as a game) */
	bool continued;
	int ship;                   /* player 1's (0-2), as play began */
	int area;                   /* the area being played (-1 none) */
	unsigned long bees;         /* in this area */
	unsigned long life_frames;  /* player 1's frames flying since the last miss */
	unsigned frames_to_save;
	unsigned last_mode, last_lives[2], last_bees[2], last_bombs[2], last_power[2], last_timer[2], last_hits[2];
	bool last_flying[2];
	bool last_bomb_on;
} game;

static void game_end(void)
{
	if (game.on && game.started && !game.tainted)
		stats_save();
	game.on = false;
}

static void game_begin(void)
{
	game_end();
	memset(&game, 0, sizeof(game));
	game.on = true;
	game.area = -1;
	game.last_mode = 0xBu;
}

/* the power-ups taken toward the power: 2 a level */
static unsigned power_ups(int player)
{
	return word(0x102178u + 0x40u * (unsigned)player) * 2u + (word(0x102194u + 0x40u * (unsigned)player) & 1u);
}

static void area_cleared(int area)
{
	kept.counter[T_AREAS_CLEARED]++;
	kept.area_cleared[area]++;
	if (area == 4 && !game.continued)
		kept.counter[T_LOOP_1CC]++;
	if (area == 9 && !game.continued)
		kept.counter[T_ALL_1CC]++;
	stats_save();
}

void stats_frame(bool counts, unsigned defeats)
{
	const unsigned mode = word(0x1018ECu);
	const int area = area_now();
	const bool bomb_on = (word(0x106E14u) & 0x8000u) != 0;

	if (mode == 0xBu && game.last_mode != 0xBu)
		game_begin();    /* (a ship select: a new game) */
	if (!counts)
		game.tainted = true;
	if (mode == 0u || mode == 1u || mode == 5u)
		game_end();    /* (the attract mode, or its demonstration: the game over) */
	if (!game.on || game.tainted)
	{
		game.last_mode = mode;
		return;
	}
	if (mode == 7u && !game.started)
	{
		game.started = true;
		game.ship = (int)(word(records[0] + 0x1Cu) % SHIPS);
		kept.counter[T_GAMES]++;
		kept.counter[T_CREDITS]++;
		kept.ship_games[game.ship]++;
		game.last_mode = mode;
		for (int player = 0; player < 2; player++)
		{
			game.last_lives[player] = word(0x101A2Eu + 2u * (unsigned)player);
			game.last_bees[player] = word(0x10379Au + 4u * (unsigned)player);
			game.last_bombs[player] = byte(records[player] + 0x22u);
			game.last_power[player] = power_ups(player);
			game.last_flying[player] = flying(player);
		}
	}
	if (!game.started)
	{
		game.last_mode = mode;
		return;
	}
	if (game.last_mode == 8u && mode == 7u)
	{
		game.continued = true;
		kept.counter[T_CONTINUES]++;
		kept.counter[T_CREDITS]++;
	}

	/* the area: begun, and cleared as the stage moves on from it during play */
	if (area != game.area)
	{
		if (game.area >= 0 && game.last_mode == 7u && (area == game.area + 1 || area == -1))
			area_cleared(game.area);
		if (area >= 0)
		{
			kept.area_played[area]++;
			game.bees = 0;
		}
		game.area = area;
	}

	if (defeats & 1u)
		kept.counter[T_BOSSES]++;
	if (defeats & 2u)
		kept.counter[T_TRUE_BOSS]++;
	if (defeats & 4u)
		kept.counter[T_MIDBOSSES]++;

	if (mode == 7u)
	{
		const unsigned difficulty = word(0x101284u) & 3u;
		static const int best_score[4] = { R_SCORE_NORMAL, R_SCORE_EASY, R_SCORE_HARD, R_SCORE_VERY_HARD };

		kept.counter[T_PLAY_FRAMES]++;
		kept.ship_frames[game.ship]++;
		at_least(&kept.counter[R_BEST_RANK], word(0x101A32u) & 0xFFu);
		for (int player = 0; player < 2; player++)
		{
			const unsigned lives = word(0x101A2Eu + 2u * (unsigned)player);
			const unsigned bees = word(0x10379Au + 4u * (unsigned)player);
			const unsigned bombs = byte(records[player] + 0x22u);
			const unsigned power = power_ups(player);
			const unsigned timer = word(0x101742u + 0x1Eu * (unsigned)player);
			const unsigned hits = word(0x10174Eu + 0x1Eu * (unsigned)player);
			const uint32_t score = bcd_long(0x10177Au + 4u * (unsigned)player);
			const bool now_flying = flying(player);

			if (game.last_mode == 7u)
			{
				if (lives < game.last_lives[player] && !now_flying)
				{
					kept.counter[T_DEATHS]++;
					if (area >= 0)
						kept.area_deaths[area]++;
					if (player == 0)
						game.life_frames = 0;
				}
				if (lives > game.last_lives[player] && now_flying && lives == game.last_lives[player] + 1u)
					kept.counter[T_EXTENDS]++;    /* (more lives otherwise: a continue, counted from its mode) */
				if (bees > game.last_bees[player])
				{
					kept.counter[T_BEES] += bees - game.last_bees[player];
					game.bees += bees - game.last_bees[player];
				}
				if (bomb_on && !game.last_bomb_on && bombs + 1u == game.last_bombs[player])
					kept.counter[T_BOMBS_USED]++;
				/* items: while flying (a respawn sets the bombs and power again, not flying) */
				if (now_flying && game.last_flying[player])
				{
					if (bombs == game.last_bombs[player] + 1u)
						kept.counter[T_BOMB_ITEMS]++;
					if (power > game.last_power[player] && power <= game.last_power[player] + 2u)
						kept.counter[T_POWER_UPS] += power - game.last_power[player];
				}
				/* kills: the chain's timer set again (it would have gone down) */
				if (timer > 0u && timer >= game.last_timer[player])
					kept.counter[T_KILLS] += game.last_timer[player] == 0u || hits < game.last_hits[player]
						? hits + 1u : hits > game.last_hits[player] ? hits - game.last_hits[player] : 1u;
			}
			game.last_lives[player] = lives;
			game.last_bees[player] = bees;
			game.last_bombs[player] = bombs;
			game.last_power[player] = power;
			game.last_timer[player] = timer;
			game.last_hits[player] = hits;
			game.last_flying[player] = now_flying;
			at_least(&kept.counter[R_BEST_CHAIN], hits);
			at_least(&kept.counter[best_score[difficulty]], score);
			if (player == 0)
				at_least(&kept.ship_best[game.ship], score);
		}
		if (flying(0))
			at_least(&kept.counter[R_LONGEST_LIFE], ++game.life_frames);
		if (area >= 0)
			at_least(&kept.area_best_bees[area], game.bees);
		if (++game.frames_to_save >= SAVE_EVERY)
		{
			game.frames_to_save = 0;
			stats_save();
		}
	}
	game.last_bomb_on = bomb_on;
	game.last_mode = mode;
}

/* ---- the page ---- */

typedef enum { L_HEADING, L_COUNTER, L_TIME, L_SCORE, L_AREA_PLAYED, L_AREA_DEATHS, L_AREA_BEES, L_SHIP_GAMES,
	L_SHIP_BEST, L_SHIP_TIME } line_kind;

static const struct { line_kind kind; int which; const char *label; } page[] = {
	{ L_HEADING, 0, "TOTALS" },
	{ L_COUNTER, T_GAMES, "GAMES PLAYED" },
	{ L_COUNTER, T_CREDITS, "CREDITS USED" },
	{ L_COUNTER, T_CONTINUES, "CONTINUES" },
	{ L_TIME, T_PLAY_FRAMES, "PLAY TIME" },
	{ L_COUNTER, T_AREAS_CLEARED, "AREAS CLEARED" },
	{ L_COUNTER, T_LOOP_1CC, "1CC LOOP 1" },
	{ L_COUNTER, T_ALL_1CC, "1CC ALL" },
	{ L_COUNTER, T_MIDBOSSES, "MID-BOSSES DOWN" },
	{ L_COUNTER, T_BOSSES, "BOSSES DOWN" },
	{ L_COUNTER, T_TRUE_BOSS, "TRUE BOSS DOWN" },
	{ L_COUNTER, T_KILLS, "ENEMIES DOWN" },
	{ L_COUNTER, T_DEATHS, "DEATHS" },
	{ L_COUNTER, T_EXTENDS, "EXTRA LIVES" },
	{ L_COUNTER, T_BOMBS_USED, "BOMBS USED" },
	{ L_COUNTER, T_BEES, "BEES" },
	{ L_COUNTER, T_POWER_UPS, "POWER-UPS" },
	{ L_COUNTER, T_BOMB_ITEMS, "BOMB ITEMS" },
	{ L_HEADING, 0, "RECORDS" },
	{ L_COUNTER, R_BEST_CHAIN, "BEST CHAIN" },
	{ L_TIME, R_LONGEST_LIFE, "LONGEST LIFE" },
	{ L_COUNTER, R_BEST_RANK, "HIGHEST RANK" },
	{ L_SCORE, R_SCORE_EASY, "BEST EASY" },
	{ L_SCORE, R_SCORE_NORMAL, "BEST NORMAL" },
	{ L_SCORE, R_SCORE_HARD, "BEST HARD" },
	{ L_SCORE, R_SCORE_VERY_HARD, "BEST V.HARD" },
	{ L_HEADING, 0, "SHIPS" },
	{ L_SHIP_GAMES, 0, "TYPE A GAMES" }, { L_SHIP_BEST, 0, "  BEST SCORE" }, { L_SHIP_TIME, 0, "  PLAY TIME" },
	{ L_SHIP_GAMES, 1, "TYPE B GAMES" }, { L_SHIP_BEST, 1, "  BEST SCORE" }, { L_SHIP_TIME, 1, "  PLAY TIME" },
	{ L_SHIP_GAMES, 2, "TYPE C GAMES" }, { L_SHIP_BEST, 2, "  BEST SCORE" }, { L_SHIP_TIME, 2, "  PLAY TIME" },
	{ L_HEADING, 0, "AREAS" },
#define AREA_LINES(n) { L_AREA_PLAYED, n, "AREA " #n " CLEARED" }, { L_AREA_DEATHS, n, "  DEATHS" }, \
	{ L_AREA_BEES, n, "  BEST BEES" }
	AREA_LINES(1), AREA_LINES(2), AREA_LINES(3), AREA_LINES(4), AREA_LINES(5),
	AREA_LINES(6), AREA_LINES(7), AREA_LINES(8), AREA_LINES(9), AREA_LINES(10),
#undef AREA_LINES
};

#define PAGE_LINES ((int)(sizeof(page) / sizeof(page[0])))

int stats_lines(void)
{
	return PAGE_LINES;
}

const char *stats_label(int line)
{
	return line >= 0 && line < PAGE_LINES ? page[line].label : "";
}

/* frames (57.55 a second) as h:mm:ss, or m:ss */
static void time_text(char *out, size_t size, unsigned long frames)
{
	const unsigned long seconds = (unsigned long)((double)frames * CAVE_FRAME_USEC / 1000000.0);

	if (seconds >= 3600u)
		snprintf(out, size, "%lu:%02lu:%02lu", seconds / 3600u, seconds / 60u % 60u, seconds % 60u);
	else
		snprintf(out, size, "%lu:%02lu", seconds / 60u, seconds % 60u);
}

const char *stats_value(int line)
{
	static char text[24];
	int n;

	if (line < 0 || line >= PAGE_LINES || page[line].kind == L_HEADING)
		return NULL;
	n = page[line].which;
	switch (page[line].kind)
	{
	case L_COUNTER:
	case L_SCORE:
		snprintf(text, sizeof(text), "%lu", kept.counter[n]);
		break;
	case L_TIME:
		time_text(text, sizeof(text), kept.counter[n]);
		break;
	case L_AREA_PLAYED:
		snprintf(text, sizeof(text), "%lu / %lu", kept.area_cleared[n - 1], kept.area_played[n - 1]);
		break;
	case L_AREA_DEATHS:
		snprintf(text, sizeof(text), "%lu", kept.area_deaths[n - 1]);
		break;
	case L_AREA_BEES:
		snprintf(text, sizeof(text), "%lu / 13", kept.area_best_bees[n - 1]);
		break;
	case L_SHIP_GAMES:
		snprintf(text, sizeof(text), "%lu", kept.ship_games[n]);
		break;
	case L_SHIP_BEST:
		snprintf(text, sizeof(text), "%lu", kept.ship_best[n]);
		break;
	case L_SHIP_TIME:
		time_text(text, sizeof(text), kept.ship_frames[n]);
		break;
	default:
		return NULL;
	}
	return text;
}
