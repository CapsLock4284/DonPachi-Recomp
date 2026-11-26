/**
 *    Name:        widgets.c
 *    Description: the side panels (widgets.h)
 *    Version:     0.2.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The font is the game's text layer, as the options menu's (menu.c: ASCII less 0x20, pens 1-6 a shade, 15 the
 *    outline), here also twice as large for the hits.
 *    The areas' file (donpachi.stages): "DPSTGB01", then 10 scores (little-endian longs, as the game shows them),
 *    areas 1 ... 10; 0 none yet.
 */
#include "widgets.h"
#include "file_save.h"
#include "game_mode.h"
#include "paths.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKGROUND  0x0A0A14u
#define FRAME       0x2A2A44u
#define GOLD        0xFFC83Cu
#define WHITE       0xF0F0F0u
#define GREY        0x8C8C9Cu
#define DIM         0x40404Cu
#define ORANGE      0xFF7A1Eu
#define CYAN        0x6CD8FFu
#define GREEN       0x5CE05Cu
#define YELLOW      0xFFE040u
#define RED         0xFF4040u

#define CHAIN_FRAMES 30     /* the chain's window: the timer's value after a kill */

/* ---- drawing on a panel ---- */

typedef struct
{
	uint32_t *pixels;
	int w;                      /* its width: WIDGET_W to WIDGET_MAX_W (a row is this many pixels) */
} panel;

static void dot(panel *p, int x, int y, uint32_t colour)
{
	if (x >= 0 && x < p->w && y >= 0 && y < WIDGET_H)
		p->pixels[y * p->w + x] = colour;
}

static void box(panel *p, int x0, int y0, int x1, int y1, uint32_t colour)
{
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++)
			dot(p, x, y, colour);
}

static void outline(panel *p, int x0, int y0, int x1, int y1, uint32_t colour)
{
	box(p, x0, y0, x1, y0 + 1, colour);
	box(p, x0, y1 - 1, x1, y1, colour);
	box(p, x0, y0, x0 + 1, y1, colour);
	box(p, x1 - 1, y0, x1, y1, colour);
}

static uint32_t shade(uint32_t colour, int percent)
{
	return ((colour >> 16 & 255u) * (uint32_t)percent / 100u) << 16 | ((colour >> 8 & 255u) * (uint32_t)percent / 100u) << 8
		| (colour & 255u) * (uint32_t)percent / 100u;
}

/* a character of the game's font at (x, y), 'size' times as large (its tiles lie turned: a row is a column); left of
   'split' in 'left_colour' (the chain bar's numbers: dark on the bar, light past it) */
static void put_char_split(panel *p, int x, int y, char ch, uint32_t left_colour, uint32_t colour, int split, int size);

static void put_char(panel *p, int x, int y, char ch, uint32_t colour, int size)
{
	put_char_split(p, x, y, ch, colour, colour, -1, size);
}

static void put_char_split(panel *p, int x, int y, char ch, uint32_t left_colour, uint32_t colour, int split, int size)
{
	static const int pens[16] = { 0, 100, 92, 84, 76, 68, 60, 60, 60, 60, 60, 60, 60, 60, 60, -1 };
	const int code = toupper((unsigned char)ch) - 0x20;
	const uint8_t *tile;

	if (code <= 0 || code >= 0x60 || !rom_set.layer[2])
		return;
	tile = rom_set.layer[2] + code * 32;
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
		{
			const int tx = 7 - j, ty = i;
			const uint8_t byte = tile[ty * 4 + tx / 2];
			const int pen = (tx & 1) ? (byte & 15) : (byte >> 4);

			if (pen && x + i * size < split)
				box(p, x + i * size, y + j * size, x + (i + 1) * size, y + (j + 1) * size,
					pens[pen] < 0 ? left_colour : shade(left_colour, 100));    /* (one solid colour on the bar) */
			else if (pen)
				box(p, x + i * size, y + j * size, x + (i + 1) * size, y + (j + 1) * size,
					pens[pen] < 0 ? 0x101018u : shade(colour, pens[pen]));
		}
}

static int text_width(const char *text, int size)
{
	return (int)strlen(text) * 8 * size;
}

static void put_text(panel *p, int x, int y, const char *text, uint32_t colour, int size)
{
	for (; *text; text++, x += 8 * size)
		put_char(p, x, y, *text, colour, size);
}

static void put_centre(panel *p, int y, const char *text, uint32_t colour, int size)
{
	put_text(p, (p->w - text_width(text, size)) / 2, y, text, colour, size);
}

static void put_right(panel *p, int right, int y, const char *text, uint32_t colour)
{
	put_text(p, right - text_width(text, 1), y, text, colour, 1);
}

/* a straight line (the graph's) */
static void line(panel *p, int x0, int y0, int x1, int y1, uint32_t colour)
{
	const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
	const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
	int error = dx - dy;

	for (;;)
	{
		dot(p, x0, y0, colour);
		if (x0 == x1 && y0 == y1)
			break;
		if (2 * error > -dy)
		{
			error -= dy;
			x0 += sx;
		}
		if (2 * error < dx)
		{
			error += dx;
			y0 += sy;
		}
	}
}

/* ---- the game's memory ---- */

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2u);
}

static uint32_t bcd_long(uint32_t address)
{
	uint32_t value = 0;

	for (uint32_t at = 0; at < 4u; at++)
	{
		const uint8_t byte = *cave_work_ram_byte(address + at);

		value = value * 100u + (byte >> 4) * 10u + (byte & 15u);
	}
	return value;
}

/* the boss bar's (with the stage, below) */
static void boss_forget(void);
static void boss_frame(void);
static void midboss_frame(void);
static void midboss_forget(void);
static unsigned defeat_events;    /* the bosses shot down since widgets_take_defeats (1 a boss, 2 the true boss, 4 a
                                     mid-boss) */

static const uint32_t records[2] = { 0x102158u, 0x102198u };
static const uint32_t timers[2] = { 0x101742u, 0x101760u }, hits[2] = { 0x10174Eu, 0x10176Cu };
static const uint32_t bests[2] = { 0x101754u, 0x101772u }, chain_points[2] = { 0x10174Au, 0x101768u };
static const uint32_t scores[2] = { 0x10177Au, 0x10177Eu };
#define TOP_SCORE 0x101846u

/* The game's mode ($1018EC): 0xB the ship select, which only a start with a credit leads to; 7 the play; 0, 1 and 5
   the attract mode and its demonstration (whose ships play as the players' would). A player's own game is from the
   ship select on, until the attract mode */

static bool own_game;

static void follow_game_mode(void)
{
	const unsigned mode = game_mode_now();    /* (7: a game played, by one player or both) */

	if (mode == 0xBu)
		own_game = true;
	else if (mode == 0u || mode == 1u || mode == 5u)
		own_game = false;
}

static bool in_play(int player)
{
	return own_game && ((word(records[player]) & 0x8000u) != 0 || bcd_long(scores[player]) != 0);
}

/* ---- the graph: each player's score against the #1 high score, through the run ---- */

#define GRAPH_X     8
#define GRAPH_Y     44
#define GRAPH_W     WIDGET_GRAPH_POINTS     /* points (104): one a column at the narrowest panel (a wider one spreads them: graph_x) */
#define GRAPH_H     88
#define MARKS       WIDGET_GRAPH_MARKS

static struct
{
	bool running;
	uint32_t target;                /* the #1 high score as the run began */
	float points[2][GRAPH_W];       /* the score over the target, each GRAPH_W-th of the run so far */
	bool played[2][GRAPH_W];
	int count;                      /* points kept */
	unsigned interval, frames;      /* the game's frames a point, and those since the last */
	int marks[MARKS], mark_count;   /* the points where a stage began */
	unsigned stage;
	uint32_t last_score[2];         /* the scores, as the game last had them (kept through the attract mode) */
	uint32_t table[5];              /* the high score table as the run began (#1 first): player 1 chases #5 first */
	int passed_at[5];               /* the point where player 1's score went past each, or -1 */
} graph;

/* the places' colours, #1 to #5 (not the graph's own orange and cyan) */
static const uint32_t place_colours[5] = { GOLD, 0xFF5CC8u, 0xB07CFFu, GREEN, 0x8C9CFFu };

/* the place player 1 chases: the lowest not yet passed, or -1 past #1 */
static int chasing(void)
{
	for (int place = 4; place >= 0; place--)
		if (graph.passed_at[place] < 0)
			return place;
	return -1;
}

/* ---- the run: the time played, the bombs, the misses in each area; and the session's best score ---- */

#define LIVES       0x101A2Eu   /* player 1's lives, the one in play counted ($101A30 player 2's) */
#define BOMBS       0x10217Au   /* player 1's bombs (the ship's record +$22; $1021BA player 2's) */
#define BOMB_ON     0x106E14u   /* bit 15: a bomb going off (set as one is let go) */
#define AREAS       10
/* the bees: 13 in each area, a count of those taken in the area so far for each player (`func_008162` adds 1 as
   one is taken; set back to 0 by a miss and at the area's end: the panel counts the rises itself) */
#define BEES_TAKEN  0x10379Au   /* ($10379E player 2's) */
#define BEES        13

static const uint32_t lives_at[2] = { LIVES, LIVES + 2u }, bombs_at[2] = { BOMBS, BOMBS + 0x40u };

static struct
{
	unsigned frames;                /* played (the game's mode 7) */
	unsigned slowed;                /* of them, frames of slowdown (cave/program.h's program_frame_lagged) */
	bool playing;                   /* in play at the last whole frame (the game's mode 7) */
	unsigned bombs[2];
	unsigned misses[2][AREAS];
	bool reached[AREAS];
	unsigned last_lives[2], last_bombs[2];
	bool last_bomb_on;
	unsigned bees[AREAS];           /* the bees taken in each area (by either player: the panel shows them with one) */
	unsigned last_bees[2];
	int bee_area;                   /* the area the bees panel shows (the last played: between the loops too) */
	/* each area's score (player 1's) against its best: the area being scored, its start and whether it counts */
	int scored_area;                /* -1: none */
	uint32_t area_start;
	bool area_counts;
	bool done[AREAS], counted[AREAS], new_best[AREAS];
	uint32_t made[AREAS], best_before[AREAS];
	unsigned last_mode;
	bool play_seen;                 /* the game's play begun (mode 7): the ship select keeps the last game's score */
	/* the last points scored (the player followed), and the chain's hits then */
	uint32_t last_score;
	uint32_t last_gain;
	unsigned last_gain_hits;
	bool gained;
} run;

/* ---- the areas' best scores (the file) ---- */

static const char stages_magic[8] = { 'D', 'P', 'S', 'T', 'G', 'B', '0', '1' };
static char stages_path[1024];
static uint32_t best[AREAS];
static bool clean_now;

const char *widgets_open(const char *path)
{
	FILE *file;
	uint8_t data[sizeof(stages_magic) + 4 * AREAS];

	memset(best, 0, sizeof(best));
	if (path)
		snprintf(stages_path, sizeof(stages_path), "%s", path);
	else
		snprintf(stages_path, sizeof(stages_path), "%sdonpachi.stages", paths_folder());
	if (!(file = fopen(stages_path, "rb")))
		return NULL;
	if (fread(data, 1, sizeof(data), file) != sizeof(data) || memcmp(data, stages_magic, sizeof(stages_magic)))
	{
		fclose(file);
		return "not a DonPachi area scores file: started again (and saved over it)";
	}
	fclose(file);
	for (int area = 0; area < AREAS; area++)
	{
		const uint8_t *const at = data + sizeof(stages_magic) + 4 * area;

		best[area] = (uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24;
	}
	return NULL;
}

static void stages_save(void)
{
	uint8_t data[sizeof(stages_magic) + 4 * AREAS];

	if (!stages_path[0])
		return;    /* (no file opened: a tool's run) */
	memcpy(data, stages_magic, sizeof(stages_magic));
	for (int area = 0; area < AREAS; area++)
	{
		uint8_t *const at = data + sizeof(stages_magic) + 4 * area;

		at[0] = (uint8_t)best[area];
		at[1] = (uint8_t)(best[area] >> 8);
		at[2] = (uint8_t)(best[area] >> 16);
		at[3] = (uint8_t)(best[area] >> 24);
	}
	file_save(stages_path, data, sizeof(data));    /* (by file_save's helper: the frame doesn't wait for the disk) */
}

void widgets_set_clean(bool clean)
{
	clean_now = clean;
}

static uint32_t session_best;       /* the best score since the program started: a reset keeps it */
/* and the rest of the session (the gadgets' SESSION): the frames played, the games begun, the credits (a game's
   start and each continue) */
static uint32_t session_frames;
static unsigned session_games, session_credits;
static bool session_two;            /* player 2 in the game (joining is a credit too) */

static int area_now(void);

static void run_reset(void)
{
	memset(&run, 0, sizeof(run));
	for (int player = 0; player < 2; player++)
	{
		run.last_lives[player] = word(lives_at[player]);
		run.last_bombs[player] = word(bombs_at[player]);
		run.last_bees[player] = word(BEES_TAKEN + 4u * (unsigned)player);
	}
	run.bee_area = area_now();
	run.last_bomb_on = (word(BOMB_ON) & 0x8000u) != 0;
	run.scored_area = -1;
	run.last_mode = game_mode_now();
	run.last_score = bcd_long(scores[0]);
}

/* the player the panels follow: player 1, or player 2 playing alone */
static int followed(void)
{
	return !in_play(0) && in_play(1) ? 1 : 0;
}

/* each area's score against its best: begun as it is entered, finished as the play moves on from it (to the next
   area, between the loops, the ending); it counts with player 1 alone in it, clean all through, no continue */
static void areas_frame(int area, unsigned mode)
{
	const uint32_t score = bcd_long(scores[0]);

	/* (no area until the play begins: at the ship select the score is still the last game's, set to 0 after) */
	if (mode == 7u)
		run.play_seen = true;
	if (!run.play_seen)
		area = -1;
	if (!clean_now || in_play(1) || mode == 8u)
		run.area_counts = false;    /* (8: the continue's count down) */
	if (run.last_mode == 8u && mode == 7u)
		session_credits++;    /* (a continue taken) */
	if (in_play(1) && !session_two)
		session_credits++;    /* (player 2 joined, or began with player 1) */
	session_two = in_play(1);
	if (run.scored_area >= 0 && score < run.area_start)
	{
		/* the score gone down (a continue starts it again): the area measured from here, and doesn't count */
		run.area_start = score;
		run.area_counts = false;
	}
	if (area != run.scored_area)
	{
		const int was = run.scored_area;

		if (was >= 0 && run.last_mode == 7u && (area == was + 1 || area == -1))
		{
			const uint32_t made = score >= run.area_start ? score - run.area_start : 0u;

			run.done[was] = true;
			run.made[was] = made;
			run.best_before[was] = best[was];
			run.counted[was] = run.area_counts;
			run.new_best[was] = run.area_counts && made > best[was];
			if (run.new_best[was])
			{
				best[was] = made;
				stages_save();
			}
		}
		run.scored_area = area;
		run.area_start = score;
		run.area_counts = clean_now && !in_play(1) && mode != 8u;
	}
	{
		/* the last points scored, and the chain's hits then */
		const int player = followed();
		const uint32_t now = bcd_long(scores[player]);

		if (now > run.last_score && run.last_score)
		{
			run.last_gain = now - run.last_score;
			run.last_gain_hits = word(hits[player]);
			run.gained = true;
		}
		run.last_score = now;
	}
	run.last_mode = mode;
}

/* the area being played, 0-9, or -1 (between the loops, the ending) */
static int area_now(void)
{
	const unsigned stage = word(0x101A40u), loop = word(0x101A42u);

	return stage < 5u && loop < 2u ? (int)(loop * 5u + stage) : -1;
}

static void run_frame(void)
{
	const bool bomb_on = (word(BOMB_ON) & 0x8000u) != 0;
	const int area = area_now();

	run.playing = game_mode_playing();
	if (run.playing)
	{
		run.frames++;
		session_frames++;
	}
	if (area >= 0)
	{
		run.reached[area] = true;
		run.bee_area = area;
	}
	for (int player = 0; player < 2; player++)
	{
		const unsigned lives = word(lives_at[player]), bombs = word(bombs_at[player]);

		/* a bomb: one let go (the bomb starts as the player's stock goes down; the stage's end, which counts the
		   bombs left down into the score, starts none) */
		if (bomb_on && !run.last_bomb_on && bombs + 1u == run.last_bombs[player])
			run.bombs[player]++;
		/* a miss: a life gone while the ship is down (the final tally takes the lives left with the ship flying) */
		if (lives < run.last_lives[player] && (word(records[player]) & 0x8000u) == 0 && area >= 0)
			run.misses[player][area]++;
		{
			const unsigned bees = word(BEES_TAKEN + 4u * (unsigned)player);

			if (bees > run.last_bees[player] && area >= 0 && game_mode_playing())
				run.bees[area] += bees - run.last_bees[player];
			run.last_bees[player] = bees;
		}
		run.last_lives[player] = lives;
		run.last_bombs[player] = bombs;
		if (bcd_long(scores[player]) > session_best)
			session_best = bcd_long(scores[player]);
	}
	run.last_bomb_on = bomb_on;
	areas_frame(area, game_mode_now());
}

void widgets_begin_game(void)
{
	/* a player's game begins (the ship select, or training's place): a new graph, against the #1 as it is now */
	widgets_reset();
	own_game = true;
	session_games++;
	session_credits++;
	session_two = false;
	graph.running = true;
	graph.target = bcd_long(TOP_SCORE);
	if (graph.target == 0)
		graph.target = 1;
	for (int place = 0; place < 5; place++)
	{
		graph.table[place] = bcd_long(TOP_SCORE + 4u * (uint32_t)place);    /* (five BCD longs: hiscore.h) */
		graph.passed_at[place] = -1;
	}
	graph.interval = 8;
	graph.stage = word(0x101A40u);
	run_reset();
}

void widgets_reset(void)
{
	memset(&graph, 0, sizeof(graph));
	memset(&run, 0, sizeof(run));
	boss_forget();
	midboss_forget();
	own_game = false;
}

static void graph_point(void)
{
	if (graph.count == GRAPH_W)
	{
		/* (full: every two points become one, and a point takes twice as long) */
		for (int player = 0; player < 2; player++)
			for (int at = 0; at < GRAPH_W / 2; at++)
			{
				graph.points[player][at] = graph.points[player][2 * at + 1];
				graph.played[player][at] = graph.played[player][2 * at] || graph.played[player][2 * at + 1];
			}
		graph.count = GRAPH_W / 2;
		graph.interval *= 2;
		for (int mark = 0; mark < graph.mark_count; mark++)
			graph.marks[mark] /= 2;
		for (int place = 0; place < 5; place++)
			if (graph.passed_at[place] >= 0)
				graph.passed_at[place] /= 2;
	}
	for (int player = 0; player < 2; player++)
	{
		graph.points[player][graph.count] = (float)bcd_long(scores[player]) / (float)graph.target;
		graph.played[player][graph.count] = in_play(player);
	}
	graph.count++;
}

/* ---- an achievement earned: a box over the right panel's top ---- */

#define TOAST_FRAMES 230    /* 4 seconds */

static char toast_name[32];
static int toast_frames, toast_more;

void widgets_toast(const char *name, int more)
{
	snprintf(toast_name, sizeof(toast_name), "%s", name);
	toast_frames = TOAST_FRAMES;
	toast_more = more;
}

bool widgets_toast_busy(void)
{
	return toast_frames > 0;
}

static void draw_toast(panel *p)
{
	const int top = 18, bottom = 102;
	/* the frame flashes for its first half second */
	const uint32_t rim = TOAST_FRAMES - toast_frames < 30 && (TOAST_FRAMES - toast_frames) / 5 % 2 ? WHITE : GOLD;
	char first[32], second[32] = "";

	box(p, 4, top, p->w - 4, bottom, 0x14142Au);
	outline(p, 4, top, p->w - 4, bottom, rim);
	outline(p, 6, top + 2, p->w - 6, bottom - 2, FRAME);
	/* a little gold trophy: the cup, its handles, the stem and the base */
	{
		const int c = p->w / 2, y = top + 6;

		box(p, c - 5, y, c + 6, y + 6, GOLD);           /* the cup */
		box(p, c - 4, y + 6, c + 5, y + 7, GOLD);
		box(p, c - 3, y + 7, c + 4, y + 8, GOLD);
		box(p, c - 8, y + 1, c - 5, y + 2, GOLD);       /* the handles */
		box(p, c - 8, y + 1, c - 7, y + 4, GOLD);
		box(p, c - 8, y + 3, c - 5, y + 4, GOLD);
		box(p, c + 6, y + 1, c + 9, y + 2, GOLD);
		box(p, c + 8, y + 1, c + 9, y + 4, GOLD);
		box(p, c + 6, y + 3, c + 9, y + 4, GOLD);
		box(p, c - 3, y + 1, c - 1, y + 5, 0xFFF0A0u);  /* its shine */
		box(p, c, y + 8, c + 1, y + 11, GOLD);          /* the stem */
		box(p, c - 3, y + 11, c + 4, y + 13, GOLD);     /* the base */
	}
	put_centre(p, top + 25, "ACHIEVEMENT", GOLD, 1);
	put_centre(p, top + 35, "UNLOCKED", GOLD, 1);
	/* the name: on two lines when it is wider than the panel (at its last space that fits) */
	snprintf(first, sizeof(first), "%s", toast_name);
	if (text_width(first, 1) > p->w - 16)
	{
		char *space = NULL;

		for (char *at = first; *at && text_width(first, 1) - text_width(at, 1) <= p->w - 16; at++)
			if (*at == ' ')
				space = at;
		if (space)
		{
			snprintf(second, sizeof(second), "%s", space + 1);
			*space = '\0';
		}
	}
	put_centre(p, top + (second[0] ? 49 : 54), first, WHITE, 1);
	if (second[0])
		put_centre(p, top + 59, second, WHITE, 1);
	if (toast_more > 0)
	{
		char more[16];

		snprintf(more, sizeof(more), "+%d MORE", toast_more);
		put_centre(p, top + 72, more, GREY, 1);
	}
}

/* the enemies' bullets on the screen, as the game tests them against the ship (hitbox.c's): their records at $104E94,
   75 of $40 bytes, one in play where +00 has bit 15 and bit 8 and not bit 12 (one that has hit) */
#define BULLETS       0x104E94u
#define BULLET_SIZE   0x40u
#define BULLETS_MOST  75

static unsigned bullets_now;    /* (counted on the frames that aren't slowed) */

/* the board's frames over the last second, and which were slowed: the game's own frames a second (FPS on the right
   panel: the board's 57.55 less its frames of slowdown, as DoDonPachi Recomp's BOARD) */
#define SECOND_FRAMES 58
static struct
{
	uint8_t lagged[SECOND_FRAMES];
	int at, filled, lagged_now;
} speed;

static void speed_frame(bool lagged)
{
	if (speed.filled == SECOND_FRAMES)
		speed.lagged_now -= speed.lagged[speed.at];
	else
		speed.filled++;
	speed.lagged[speed.at] = lagged;
	speed.lagged_now += lagged;
	speed.at = (speed.at + 1) % SECOND_FRAMES;
}

static unsigned count_bullets(void)
{
	unsigned count = 0;

	if (word(0x1018ECu) != 7u)
		return 0;    /* (only in play: the records keep their last values otherwise) */
	for (uint32_t index = 0; index < BULLETS_MOST; index++)
		count += (word(BULLETS + BULLET_SIZE * index) & 0x9100u) == 0x8100u;
	return count;
}

void widgets_frame(void)
{
	const bool was_own = own_game;

	if (toast_frames > 0)
		toast_frames--;
	speed_frame(program_frame_lagged());
	if (program_frame_lagged())
	{
		/* a frame of slowdown: the game's memory is half way through its frame, so it is looked at again next; the
		   run's time goes on, and the frame is counted as slowed */
		if (own_game && run.playing)
		{
			run.frames++;
			run.slowed++;
			session_frames++;
		}
		return;
	}

	bullets_now = count_bullets();
	follow_game_mode();
	if (own_game && !was_own)
		widgets_begin_game();
	if (!own_game)
		return;    /* (the last game's graph and run stay, through the attract mode) */
	run_frame();
	boss_frame();
	midboss_frame();
	for (int player = 0; player < 2; player++)
		graph.last_score[player] = bcd_long(scores[player]);
	/* the high score chase: each place player 1's score goes past, and where on the graph */
	for (int place = 4; place >= 0; place--)
		if (graph.running && graph.passed_at[place] < 0 && graph.table[place] && graph.last_score[0] > graph.table[place])
			graph.passed_at[place] = graph.count;
	if (word(0x101A40u) != graph.stage && graph.mark_count < MARKS)
	{
		graph.stage = word(0x101A40u);
		graph.marks[graph.mark_count++] = graph.count;
	}
	if (++graph.frames >= graph.interval)
	{
		graph.frames = 0;
		graph_point();
	}
}

/* ---- drawing ---- */

static void number(char *out, size_t size, uint32_t value)
{
	snprintf(out, size, "%lu", (unsigned long)value);
}

static void frame_panel(panel *p, const char *title)
{
	box(p, 0, 0, p->w, WIDGET_H, BACKGROUND);
	outline(p, 0, 0, p->w, WIDGET_H, FRAME);
	box(p, 1, 1, p->w - 1, 15, 0x14142Au);
	put_centre(p, 4, title, GOLD, 1);
}

/* a player's chain: the hits, the time left to go on with it, its points, the best */
static void draw_chain(panel *p, int player, int top)
{
	const bool playing = in_play(player);
	const unsigned timer = word(timers[player]), count = word(hits[player]);
	const uint32_t colour = playing ? WHITE : DIM;
	char text[32];
	int filled;

	snprintf(text, sizeof(text), "%dP", player + 1);
	put_text(p, 8, top, text, playing ? (player ? CYAN : ORANGE) : DIM, 1);
	put_right(p, p->w - 8, top, "CHAIN", playing ? GREY : DIM);
	/* the hits, large, while the chain or its result is on */
	snprintf(text, sizeof(text), "%u", playing && count ? count : 0u);
	put_text(p, 8, top + 14, text, playing && count ? (timer ? GOLD : GREY) : DIM, 3);
	put_text(p, 8 + text_width(text, 3) + 4, top + 30, "HIT", playing && count ? colour : DIM, 1);
	/* the time left: 30 frames after a kill, draining, the frames left counted down inside it (30 frames: 0.52 s) */
	outline(p, 8, top + 44, p->w - 8, top + 56, playing ? GREY : DIM);
	filled = (int)((p->w - 18) * (timer > CHAIN_FRAMES ? CHAIN_FRAMES : timer) / CHAIN_FRAMES);
	if (playing && timer)
	{
		box(p, 9, top + 45, 9 + filled, top + 55, timer > 15 ? GREEN : timer > 7 ? YELLOW : RED);
		snprintf(text, sizeof(text), "%u", timer);
		for (int at = 0, x = (p->w - text_width(text, 1)) / 2; text[at]; at++, x += 8)
			put_char_split(p, x, top + 46, text[at], 0x10101Cu, WHITE, 9 + filled, 1);    /* (dark on the bar) */
	}
	/* its points so far, and the best chain */
	put_text(p, 8, top + 62, "PTS", playing ? GREY : DIM, 1);
	number(text, sizeof(text), playing ? bcd_long(chain_points[player]) : 0u);
	put_right(p, p->w - 8, top + 62, text, colour);
	put_text(p, 8, top + 74, "MAX", playing ? GREY : DIM, 1);
	number(text, sizeof(text), playing ? word(bests[player]) : 0u);
	put_right(p, p->w - 8, top + 74, text, colour);
}

#define CHAIN_HEIGHT 96     /* a player's chain, with the space before the next */

/* the graph's box is the panel's width less 8 each side; point 'at' (0 to GRAPH_W) of it at this x */
static int graph_x(const panel *p, int at)
{
	return GRAPH_X + at * (p->w - 2 * GRAPH_X) / GRAPH_W;
}

static void draw_graph(panel *p)
{
	const int bottom = GRAPH_Y + GRAPH_H, right = p->w - GRAPH_X;
	float most = 1.0f;
	char text[32];

	/* (the height: 100% at the top, or the best score if a player is past the #1) */
	for (int player = 0; player < 2; player++)
		for (int at = 0; at < graph.count; at++)
			if (graph.played[player][at] && graph.points[player][at] > most)
				most = graph.points[player][at];
	box(p, GRAPH_X, GRAPH_Y, right, bottom, 0x000000u);
	for (int at = 0; at < graph.count; at++)
	{
		/* player 1: the area under the line, as the arcade ports' rank graphs */
		const int height = graph.played[0][at] ? (int)(graph.points[0][at] / most * (float)GRAPH_H) : 0;

		if (height > 0)
			box(p, graph_x(p, at), bottom - (height > GRAPH_H ? GRAPH_H : height), graph_x(p, at + 1), bottom, ORANGE);
	}
	for (int at = 1; at < graph.count; at++)
		if (graph.played[1][at] && graph.played[1][at - 1])
			line(p, graph_x(p, at - 1), bottom - 1 - (int)(graph.points[1][at - 1] / most * (float)(GRAPH_H - 1)),
				graph_x(p, at), bottom - 1 - (int)(graph.points[1][at] / most * (float)(GRAPH_H - 1)), CYAN);
	/* the 100% and 50% lines, the stages' marks */
	for (int x = GRAPH_X; x < right; x += 2)
	{
		dot(p, x, bottom - (int)((float)GRAPH_H / most), WHITE);
		dot(p, x, bottom - (int)((float)GRAPH_H / most / 2.0f), GREY);
	}
	for (int mark = 0; mark < graph.mark_count; mark++)
		for (int y = GRAPH_Y; y < bottom; y += 3)
			dot(p, graph_x(p, graph.marks[mark]), y, DIM);
	/* the high score chase, as DoDonPachi Recomp's: the place being chased dashed at its score, those passed dotted at
	   theirs and a line in their colour where they were passed, with their number (each on a row of its own) */
	if (graph.running)
	{
		const int chased = chasing();

		for (int place = 4; place >= 0; place--)
		{
			const uint32_t colour = place_colours[place];
			const int y = bottom - (int)((float)graph.table[place] / (float)graph.target / most * (float)GRAPH_H);

			if (y < GRAPH_Y || graph.table[place] == 0)
				continue;
			if (graph.passed_at[place] >= 0)
			{
				const int x = graph_x(p, graph.passed_at[place]);

				for (int dx = GRAPH_X; dx < right; dx += 4)
					dot(p, dx, y, colour);
				box(p, x, GRAPH_Y, x + 1, bottom, colour);
				put_char(p, x + 2 > right - 8 ? x - 9 : x + 2, GRAPH_Y + 1 + 9 * (4 - place), (char)('1' + place),
					colour, 1);
			}
			else if (place == chased)
				for (int dx = GRAPH_X; dx < right; dx += 6)
					box(p, dx, y, dx + 3, y + 1, colour);
		}
	}
	outline(p, GRAPH_X - 1, GRAPH_Y - 1, right + 1, bottom + 1, GREY);
	/* (the top of the box: 100%, or the highest score's share when a player is past the #1) */
	snprintf(text, sizeof(text), "%d%%", (int)(most * 100.0f + 0.5f));
	put_right(p, p->w - 2, GRAPH_Y - 10, text, GREY);
	put_text(p, GRAPH_X, bottom + 3, "ST", GREY, 1);
	put_right(p, right, bottom + 3, "NOW", GREY);

	/* the percentage, as large as it goes, and the scores */
	if (graph.running)
	{
		const float now = (float)graph.last_score[0] / (float)graph.target * 100.0f;

		snprintf(text, sizeof(text), "%.1f%%", (double)now);
		put_centre(p, bottom + 14, text, ORANGE, 2);
		if (graph.last_score[1])
		{
			snprintf(text, sizeof(text), "2P %.1f%%", (double)((float)graph.last_score[1] / (float)graph.target * 100.0f));
			put_centre(p, bottom + 32, text, CYAN, 1);
		}
	}
	/* the place chased next (in its colour), or the #1 once it is passed */
	if (graph.running && chasing() >= 0)
	{
		char label[16];

		number(text, sizeof(text), graph.table[chasing()]);
		snprintf(label, sizeof(label), "NEXT #%d", chasing() + 1);
		if (text_width(label, 1) + 8 + text_width(text, 1) > p->w - 16)
			snprintf(label, sizeof(label), "#%d", chasing() + 1);    /* (a narrow panel: the shorter name) */
		put_text(p, 8, bottom + 44, label, place_colours[chasing()], 1);
		put_right(p, p->w - 8, bottom + 44, text, place_colours[chasing()]);
	}
	else
	{
		number(text, sizeof(text), graph.running ? graph.target : bcd_long(TOP_SCORE));
		put_text(p, 8, bottom + 44, !graph.running ? "TOP" : text_width("PAST #1", 1) + 8 + text_width(text, 1) > p->w - 16
			? "PAST" : "PAST #1", graph.running ? GOLD : GREY, 1);
		put_right(p, p->w - 8, bottom + 44, text, WHITE);
	}
	put_text(p, 8, bottom + 56, "NOW", GREY, 1);
	number(text, sizeof(text), graph.last_score[0]);
	put_right(p, p->w - 8, bottom + 56, text, WHITE);
}

/* ---- the stage: the area (1-10), the loop, and how far through the stage, to its boss ---- */

#define STAGE       0x101A40u   /* 0-4 the stages of a loop (5: between the loops, and the ending) */
#define LOOP        0x101A42u   /* 0 the first loop (areas 1-5), 1 the second (6-10) */
#define SCROLL      0x101A56u   /* the stage's scroll position: from -1 up to its boss, where it stops (stage 2's
                                   boss is a chase: its scroll goes on, faster, from 566) */

/* the scroll position where each stage's boss appears (its object is made), measured: the same in both loops. The
   bar then shows the boss's health. Area 1's scroll goes on slowly through the fight, area 2's boss is a chase */
static const int boss_at[5] = { 555, 576, 629, 888, 539 };
/* where the stage's own scrolling starts: area 1 opens with the ship's fast fly-in, to 256 */
static const int start_at[5] = { 256, 0, 0, 0, 0 };

/* ---- the boss's health ----

   The game's objects: 64 of $40 bytes from $100102; +0 bit 15 in use, +C (high byte) the type (object_types at
   $12756), +6 a pointer to its hit records: $20 bytes each, +0 the flags (bit 13: it can be shot), +$18 the hit
   points (ship_hit_test takes the shots' damage off them). Each boss keeps its health its own way (read from its
   routine): parts with their own hit points, or "sensor" records set back to $7FFF each frame, the damage taken off a
   pool; and pools in reserve that refill the first as it runs out. The health is all of it together, against the
   most it has been since the boss appeared */

#define BOSS_TRUE   0x7Cu       /* area 10's last boss, after area 5's (the harder version) */

/* the boss of each stage (both loops): area 10 has two */
static const unsigned stage_boss[5] = { 0x31u, 0x58u, 0x4Bu, 0x3Bu, 0x6Bu };

static uint32_t long_at(uint32_t address)
{
	return (uint32_t)word(address) << 16 | word(address + 2u);
}

static long positive16(uint32_t address)
{
	const int16_t value = (int16_t)word(address);

	return value > 0 ? value : 0;
}

/* the health of the boss object at 'object': -1 if its type is not a boss */
static long boss_health(unsigned type, uint32_t object)
{
	const uint32_t hits = long_at(object + 6u);
	int32_t pool;

	switch (type)
	{
	case 0x31u:
		/* area 1 (boss_area_1): the core (record 1, $5FFF until the four parts are gone) with its reserve in record
		   2, and the parts, records 3-6 */
		return positive16(hits + 0x38u) + positive16(hits + 0x58u) + positive16(hits + 0x78u)
			+ positive16(hits + 0x98u) + positive16(hits + 0xB8u) + positive16(hits + 0xD8u);
	case 0x58u:
		/* area 2 (boss_area_2): record 0, refilled from record 2's and then from +$2D6 */
		return positive16(hits + 0x18u) + positive16(hits + 0x58u) + positive16(hits + 0x2D6u);
	case 0x4Bu:
		/* area 3 (boss_area_3): a long at +$30 of the records */
		pool = (int32_t)long_at(hits + 0x30u);
		return pool > 0 ? pool : 0;
	case 0x3Bu:
		/* area 4 (boss_area_4): record 0, refilled from the object's +$32, then +$36, then the records' +$3A */
		return positive16(hits + 0x18u) + word(object + 0x32u) + word(object + 0x36u) + word(hits + 0x3Au);
	case 0x6Bu:
	case BOSS_TRUE:
		/* area 5 and 10 (boss_area_5), the true last boss (true_boss): a long at +$1C of the records */
		pool = (int32_t)long_at(hits + 0x1Cu);
		return pool > 0 ? pool : 0;
	}
	return -1;
}

enum { BOSS_NONE, BOSS_FIGHT, BOSS_DEFEATED, BOSS_TIME_UP };

static struct
{
	int area;                   /* the area the state is for, 0-9 (-1: none) */
	int state;
	unsigned type;
	long health, most;
	unsigned done;              /* the bosses already met in the area: 1 its boss, 2 the true boss */
	bool timed_out;             /* its health went to nothing at once: its time ran out (shots take it a little at a
	                               time), and it flies off or blows itself up */
} boss = { -1, BOSS_NONE, 0, 0, 0, 0, false };

static void boss_forget(void)
{
	boss.area = -1;
	boss.state = BOSS_NONE;
	boss.done = 0;
}

/* the points for each stage's boss core (the shmups wiki's enemy list), and the true boss's: a score going up by
   this much in a frame is the boss shot down (a part may be left, and the object stays through its explosion) */
static const uint32_t boss_points[5] = { 100000u, 210000u, 300000u, 440000u, 500000u };
#define TRUE_BOSS_POINTS 1000000u

static uint32_t score_of(int player)
{
	const uint32_t bcd = long_at(0x10177Au + 4u * (unsigned)player);
	uint32_t value = 0;

	for (int digit = 7; digit >= 0; digit--)
		value = value * 10u + (bcd >> (digit * 4) & 15u);
	return value;
}

static void boss_frame(void)
{
	const unsigned stage = word(STAGE), loop = word(LOOP);
	const int area = stage < 5u && loop < 2u ? (int)(loop * 5u + stage) : -1;
	static uint32_t last_score[2];
	uint32_t gained = 0;
	bool seen = false;

	for (int player = 0; player < 2; player++)
	{
		const uint32_t score = score_of(player);

		if (score > last_score[player] && score - last_score[player] > gained)
			gained = score - last_score[player];
		last_score[player] = score;
	}

	if (area != boss.area)
	{
		boss.area = area;
		boss.state = BOSS_NONE;
		boss.done = 0;
	}
	if (area < 0)
		return;
	for (unsigned slot = 0; slot < 64u && !seen; slot++)
	{
		const uint32_t object = 0x100102u + 0x40u * slot;
		const unsigned type = word(object + 0xCu) >> 8;
		const unsigned bit = type == BOSS_TRUE ? 2u : 1u;
		long health;

		if (!(word(object) & 0x8000u))
			continue;
		/* (only this stage's boss, once: its slot is soon used again, with the type left in it) */
		if (!(type == stage_boss[stage] || (type == BOSS_TRUE && area == 9)) || (boss.done & bit))
			continue;
		if ((health = boss_health(type, object)) < 0)
			continue;
		seen = true;
		if (boss.state != BOSS_FIGHT || boss.type != type)
		{
			boss.state = BOSS_FIGHT;
			boss.type = type;
			boss.most = 0;
			boss.health = 0;
			boss.timed_out = false;
		}
		if (boss.health - health > boss.most / 8)
			boss.timed_out = true;
		boss.health = health;
		if (health > boss.most)
			boss.most = health;
		if ((health == 0 && boss.most > 0 && !boss.timed_out)
			|| gained >= (type == BOSS_TRUE ? TRUE_BOSS_POINTS : boss_points[stage]))
		{
			/* shot down: its object may stay through the explosion, to the area's end */
			boss.done |= bit;
			boss.state = BOSS_DEFEATED;
			defeat_events |= bit;
			return;
		}
	}
	if (!seen && boss.state == BOSS_FIGHT)
	{
		/* gone: shot down, or flown off when its time ran out */
		boss.done |= boss.type == BOSS_TRUE ? 2u : 1u;
		boss.state = (boss.health > 0 && boss.most > 0) || boss.timed_out ? BOSS_TIME_UP : BOSS_DEFEATED;
		if (boss.state == BOSS_DEFEATED)
			defeat_events |= boss.type == BOSS_TRUE ? 2u : 1u;
	}
}

/* ---- the mid-boss's health ----

   Each area's mid-boss (both loops), found by its hit points (tests/harness/midboss_find, midboss_fight): area 1 a
   heavy tank, 2 three heavy planes (three objects), 3 a heavy tank, 4 the big ship (its record 0 refilled once from
   the record's +$1C as it runs out), 5 the big ship of two parts (records 0 and 5). The health is all of it together,
   against the most it has been; the bar shows it from the mid-boss's appearing until a few seconds after it is gone
   (shot down, or flown off) */

static const unsigned stage_midboss[5] = { 0x42u, 0x13u, 0x21u, 0x1Eu, 0x8Bu };
#define MIDBOSS_SHOWN_AFTER 180    /* frames the result stays (DEFEATED / ESCAPED) */

static long midboss_health(unsigned type, uint32_t object)
{
	const uint32_t hits = long_at(object + 6u);
	long health = positive16(hits + 0x18u);

	if (type == 0x1Eu)
		health += positive16(hits + 0x1Cu);    /* (the reserve) */
	else if (type == 0x8Bu)
		health += positive16(hits + 0xB8u);    /* (the second part, record 5) */
	return health;
}

static struct
{
	int area;                   /* the area the state is for, 0-9 (-1: none) */
	int state;                  /* BOSS_NONE, BOSS_FIGHT, BOSS_DEFEATED, BOSS_TIME_UP (escaped) */
	long health, most;
	bool done;                  /* met in the area already */
	int after;                  /* frames left showing the result */
} midboss = { -1, BOSS_NONE, 0, 0, false, 0 };

static void midboss_forget(void)
{
	midboss.area = -1;
	midboss.state = BOSS_NONE;
}

static void midboss_frame(void)
{
	const unsigned stage = word(STAGE), loop = word(LOOP);
	const int area = stage < 5u && loop < 2u ? (int)(loop * 5u + stage) : -1;
	long health = 0;
	bool seen = false;

	if (area != midboss.area)
	{
		midboss.area = area;
		midboss.state = BOSS_NONE;
		midboss.done = false;
		midboss.after = 0;
	}
	if (area < 0)
		return;
	if (midboss.after > 0 && --midboss.after == 0)
		midboss.state = BOSS_NONE;
	/* (before the area's boss only: its type is used again later in some areas) */
	if (midboss.done || (int16_t)word(SCROLL) >= boss_at[stage])
		return;
	for (unsigned slot = 0; slot < 64u; slot++)
	{
		const uint32_t object = 0x100102u + 0x40u * slot;

		if ((word(object) & 0x8000u) && word(object + 0xCu) >> 8 == stage_midboss[stage])
		{
			seen = true;
			health += midboss_health(stage_midboss[stage], object);
		}
	}
	if (seen)
	{
		if (midboss.state != BOSS_FIGHT)
		{
			midboss.state = BOSS_FIGHT;
			midboss.most = 0;
		}
		midboss.health = health;
		if (health > midboss.most)
			midboss.most = health;
		if (health == 0 && midboss.most > 0)
		{
			/* shot down (its object stays through the explosion) */
			midboss.done = true;
			midboss.state = BOSS_DEFEATED;
			midboss.after = MIDBOSS_SHOWN_AFTER;
			defeat_events |= 4u;
		}
	}
	else if (midboss.state == BOSS_FIGHT)
	{
		midboss.done = true;
		midboss.state = midboss.health > 0 ? BOSS_TIME_UP : BOSS_DEFEATED;
		if (midboss.state == BOSS_DEFEATED)
			defeat_events |= 4u;
		midboss.after = MIDBOSS_SHOWN_AFTER;
	}
}

static void draw_stage(panel *p, int top)
{
	const unsigned stage = word(STAGE), loop = word(LOOP);
	const bool known = own_game && stage < 5u && loop < 2u;
	char text[32];

	box(p, 8, top, p->w - 8, top + 1, FRAME);
	put_text(p, 8, top + 6, "AREA", known ? GREY : DIM, 1);
	put_right(p, p->w - 8, top + 6, "LOOP", known ? GREY : DIM);
	snprintf(text, sizeof(text), "%u", loop * 5u + stage + 1u);
	put_text(p, 8, top + 16, known ? text : "-", known ? GOLD : DIM, 2);
	snprintf(text, sizeof(text), "%u", loop + 1u);
	put_text(p, p->w - 8 - text_width(known ? text : "-", 2), top + 16, known ? text : "-", known ? WHITE : DIM, 2);
	/* how far through it: the bar fills to the boss; then the boss's health, emptying as it is shot */
	outline(p, 8, top + 40, p->w - 8, top + 52, known ? GREY : DIM);
	if (known && boss.area == (int)(loop * 5u + stage) && boss.state != BOSS_NONE)
	{
		const bool fight = boss.state == BOSS_FIGHT;
		/* (rounded up: 0% only when it is gone) */
		const int left = !fight || boss.timed_out || boss.most <= 0 ? 0
			: (int)((boss.health * 100 + boss.most - 1) / boss.most);
		const int filled = (p->w - 18) * left / 100;

		box(p, 9, top + 41, 9 + filled, top + 51, boss.type == BOSS_TRUE ? GOLD : RED);
		if (fight && !boss.timed_out)
			snprintf(text, sizeof(text), "%d%%", left);
		else
			snprintf(text, sizeof(text), "%s", boss.state == BOSS_DEFEATED ? "DEFEATED" : "TIME UP");    /* (or going) */
		for (int at = 0, x = (p->w - text_width(text, 1)) / 2; text[at]; at++, x += 8)
			put_char_split(p, x, top + 42, text[at], 0x10101Cu, WHITE, 9 + filled, 1);
		put_centre(p, top + 58, boss.type == BOSS_TRUE ? "TRUE BOSS" : "BOSS HEALTH", GREY, 1);
		return;
	}
	if (known && midboss.area == (int)(loop * 5u + stage) && midboss.state != BOSS_NONE)
	{
		const bool fight = midboss.state == BOSS_FIGHT;
		const int left = !fight || midboss.most <= 0 ? 0 : (int)((midboss.health * 100 + midboss.most - 1) / midboss.most);
		const int filled = (p->w - 18) * left / 100;

		box(p, 9, top + 41, 9 + filled, top + 51, ORANGE);
		if (fight)
			snprintf(text, sizeof(text), "%d%%", left);
		else
			snprintf(text, sizeof(text), "%s", midboss.state == BOSS_DEFEATED ? "DEFEATED" : "ESCAPED");
		for (int at = 0, x = (p->w - text_width(text, 1)) / 2; text[at]; at++, x += 8)
			put_char_split(p, x, top + 42, text[at], 0x10101Cu, WHITE, 9 + filled, 1);
		put_centre(p, top + 58, "MID-BOSS", GREY, 1);
		return;
	}
	if (known)
	{
		const int scroll = (int16_t)word(SCROLL) - start_at[stage], end = boss_at[stage] - start_at[stage];
		const int done = scroll <= 0 ? 0 : scroll >= end ? 100 : scroll * 100 / end;
		const int filled = (p->w - 18) * done / 100;

		box(p, 9, top + 41, 9 + filled, top + 51, done >= 100 ? RED : CYAN);
		if (done >= 100)
			snprintf(text, sizeof(text), "BOSS");
		else
			snprintf(text, sizeof(text), "%d%%", done);
		for (int at = 0, x = (p->w - text_width(text, 1)) / 2; text[at]; at++, x += 8)
			put_char_split(p, x, top + 42, text[at], 0x10101Cu, WHITE, 9 + filled, 1);
	}
	put_centre(p, top + 58, "STAGE PROGRESS", known ? GREY : DIM, 1);
}

/* ---- the rank: the game's difficulty, 0-63 ---- */

/* $101A33 (the low byte of $101A32), worked out again as the stage scrolls (func at $02D6xx): the area's base, the
   stage and loop (16 a loop), the time since the last miss (1 each 2048 frames), the bees taken in the stage, the
   lives, and each player's power (2 a level), bombs and best chain (1 each 8 hits over 20); 63 at most. The bullets'
   speed is taken from it ($101A34, $101A36) */
#define RANK        0x101A32u
#define RANK_MAX    63

static void draw_rank(panel *p, int top)
{
	const bool known = own_game && area_now() >= 0;
	const unsigned rank = word(RANK) & 0xFFu;
	char text[16];

	box(p, 8, top, p->w - 8, top + 1, FRAME);
	put_text(p, 8, top + 6, "RANK", known ? GREY : DIM, 1);
	snprintf(text, sizeof(text), "%u/%u", known ? rank : 0u, RANK_MAX);
	put_right(p, p->w - 8, top + 6, known ? text : "-", known ? WHITE : DIM);
	outline(p, 8, top + 18, p->w - 8, top + 26, known ? GREY : DIM);
	if (known)
	{
		const int filled = (p->w - 18) * (int)(rank > RANK_MAX ? RANK_MAX : rank) / RANK_MAX;
		/* green, to yellow at half, to red at the top */
		const uint32_t colour = rank < 22u ? GREEN : rank < 43u ? YELLOW : RED;

		box(p, 9, top + 19, 9 + filled, top + 25, colour);
	}
}

#define RANK_HEIGHT 34

/* ---- the run ---- */

/* ---- the areas: the loop's five, each one's score against its best (as the gadgets' AREAS) ---- */

/* the loop's areas (the one being played, or the last) into v's: the gadgets' and the panel's */
static void areas_view(widgets_view *v)
{
	const int shown = run.scored_area >= 0 ? run.scored_area : run.bee_area;

	v->loop_shown = shown >= 5 ? 1 : 0;
	for (int at = 0; at < 5; at++)
	{
		const int which = v->loop_shown * 5 + at;

		if (own_game && which == run.scored_area)
		{
			v->areas[at].state = WIDGET_AREA_LIVE;
			v->areas[at].made = graph.last_score[0] >= run.area_start ? graph.last_score[0] - run.area_start : 0u;
			v->areas[at].best = best[which];
			v->areas[at].counts = run.area_counts;
		}
		else if (run.done[which])
		{
			v->areas[at].state = WIDGET_AREA_DONE;
			v->areas[at].made = run.made[which];
			v->areas[at].best = run.best_before[which];
			v->areas[at].counts = run.counted[which];
			v->areas[at].new_best = run.new_best[which];
		}
		else
		{
			v->areas[at].state = WIDGET_AREA_TODO;
			v->areas[at].best = best[which];
		}
	}
}

static void draw_areas(panel *p, int top, bool two)
{
	static widgets_view v;    /* (only its areas filled in here) */
	const uint32_t label = own_game ? GREY : DIM;
	char text[32], title[16];
	int y = top + 6, against = -1;

	areas_view(&v);
	box(p, 8, top, p->w - 8, top + 1, FRAME);
	if (!two)
	{
		put_text(p, 8, y, "AREAS", label, 1);
		snprintf(title, sizeof(title), "LOOP %d", v.loop_shown + 1);
		put_right(p, p->w - 8, y, title, label);
		y += 11;
	}
	for (int at = 0; at < 5; at++, y += 11)
	{
		const int state = v.areas[at].state;
		const uint32_t made = v.areas[at].made, area_best = v.areas[at].best;
		uint32_t number_colour = DIM, score_colour = DIM;

		snprintf(title, sizeof(title), "%d", v.loop_shown * 5 + at + 1);
		if (state == WIDGET_AREA_LIVE)
		{
			number_colour = YELLOW;
			score_colour = WHITE;
			number(text, sizeof(text), made);
			against = at;
		}
		else if (state == WIDGET_AREA_DONE)
		{
			number_colour = label;
			score_colour = v.areas[at].new_best ? GOLD : v.areas[at].counts && made < area_best ? RED : WHITE;
			number(text, sizeof(text), made);
			if (against < 0 || v.areas[against].state != WIDGET_AREA_LIVE)
				against = at;
		}
		else if (area_best)
			number(text, sizeof(text), area_best);    /* (dim: the best to beat) */
		else
			snprintf(text, sizeof(text), "-");
		put_text(p, 8, y, title, number_colour, 1);
		put_right(p, p->w - 8, y, text, score_colour);
	}
	if (two)
		return;    /* (no room under two chains; two players' areas don't count) */
	/* the area being played (or the last) against its best */
	y += 1;
	put_text(p, 8, y, "VS BEST", label, 1);
	{
		uint32_t colour = DIM;

		snprintf(text, sizeof(text), "-");
		if (against >= 0)
		{
			const uint32_t made = v.areas[against].made, area_best = v.areas[against].best;
			const bool live = v.areas[against].state == WIDGET_AREA_LIVE;

			if (!v.areas[against].counts)
				snprintf(text, sizeof(text), "OFF");    /* (not a clean one-player area: it can't be a best) */
			else if (!area_best)
			{
				snprintf(text, sizeof(text), "NEW");
				colour = GOLD;
			}
			else
			{
				snprintf(text, sizeof(text), "%s%lu", made >= area_best ? "+" : "-",
					(unsigned long)(made >= area_best ? made - area_best : area_best - made));
				colour = made > area_best ? GOLD : live ? WHITE : RED;
			}
		}
		put_right(p, p->w - 8, y, text, colour);
	}
	y += 12;
	number(text, sizeof(text), session_best);
	put_text(p, 8, y, "BEST", label, 1);
	put_right(p, p->w - 8, y, text, GOLD);
}

/* ---- the bees: the area's 13, each greyed until it is taken ---- */

#define BEE_CODE    0x0C2Eu     /* the bee's sprite (its first frame: 32 x 32, at the code's 256 pixels on) */
#define BEE_COLOUR  0x3Bu       /* its palette (the sprite's attribute as shown, $3B30) */
#define BEE_SIZE    32
#define BEE_ICON    13          /* drawn at half size, from (3, 3) of the sprite: the bee lies in 26 x 26 of it */

static unsigned bee_pen(int x, int y)
{
	const uint32_t pixel = (BEE_CODE << 8) + (uint32_t)(y * BEE_SIZE + x);
	const uint8_t byte = rom_set.sprites[(pixel >> 1) & (DONPACHI_SPRITES_SIZE - 1u)];

	return (pixel & 1u) ? (byte & 15u) : (byte >> 4);
}

/* xGRB 555, as the board's palette has it, to 0x00RRGGBB */
static uint32_t palette_colour(unsigned pen)
{
	const uint16_t value = cave_board.palette[pen & (CAVE_PALETTE_WORDS - 1u)];
	const uint32_t g = value >> 10 & 31u, r = value >> 5 & 31u, b = value & 31u;

	return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

/* a bee at (x, y): 2 x 2 of the sprite's pixels to each of the icon's, their colours averaged; one with none of the
   bee's pixels left clear. Taken: its own colours; not yet: grey, dim */
static void draw_bee(panel *p, int x, int y, bool taken)
{
	for (int row = 0; row < BEE_ICON; row++)
		for (int column = 0; column < BEE_ICON; column++)
		{
			unsigned r = 0, g = 0, b = 0, count = 0;

			for (int sy = 0; sy < 2; sy++)
				for (int sx = 0; sx < 2; sx++)
				{
					const unsigned pen = bee_pen(3 + (BEE_ICON - 1 - row) * 2 + sx, 3 + column * 2 + sy);    /* (turned, as the picture is) */

					if (pen)
					{
						const uint32_t colour = palette_colour(BEE_COLOUR * 16u + pen);

						r += colour >> 16 & 255u;
						g += colour >> 8 & 255u;
						b += colour & 255u;
						count++;
					}
				}
			if (count < 2)
				continue;    /* (the outline's lone pixels: the shape stays clean at half size) */
			r /= count;
			g /= count;
			b /= count;
			if (!taken)
			{
				const unsigned grey = (r * 30u + g * 59u + b * 11u) / 100u * 45u / 100u + 18u;

				r = g = b = grey;
			}
			if (x + column >= 0 && x + column < p->w && y + row >= 0 && y + row < WIDGET_H)
				p->pixels[(y + row) * p->w + x + column] = r << 16 | g << 8 | b;
		}
}

#define BEES_HEIGHT 33

static void draw_bees(panel *p, int top)
{
	const unsigned taken = own_game && run.bee_area >= 0 ? run.bees[run.bee_area] : 0u;
	const int step = (p->w - 16 - BEE_ICON) / 6;    /* (seven across the panel: 15 apart at its narrowest) */

	box(p, 8, top, p->w - 8, top + 1, FRAME);
	for (int bee = 0; bee < BEES; bee++)
	{
		const int row = bee / 7, column = bee % 7;

		draw_bee(p, 8 + column * step + row * (step / 2), top + 5 + row * 14, (unsigned)bee < taken);    /* (the 6 of the second row centred) */
	}
}

/* ---- the controls: what the board is given (the stick and the three buttons) ---- */

static void draw_input(panel *p, int top, int player)
{
	const uint16_t held = (uint16_t)~(player ? cave_board.in1 : cave_board.in0);
	const int left = (p->w - WIDGET_W) / 2;    /* (the stick and buttons, 104 wide, in the middle) */
	const uint32_t frame = own_game ? GREY : DIM;
	/* the stick: a 3 x 3 of cells, the one it points to lit */
	const int dx = held & CAVE_LEFT ? -1 : held & CAVE_RIGHT ? 1 : 0, dy = held & CAVE_UP ? -1 : held & CAVE_DOWN ? 1 : 0;
	static const struct { uint16_t bit; char name; } buttons[3] = {
		{ CAVE_BUTTON1, 'S' }, { CAVE_BUTTON2, 'B' }, { CAVE_BUTTON3, 'C' }
	};

	box(p, 8, top, p->w - 8, top + 1, FRAME);
	for (int row = -1; row <= 1; row++)
		for (int column = -1; column <= 1; column++)
		{
			const int x = left + 8 + (column + 1) * 9, y = top + 6 + (row + 1) * 9;

			if (own_game && row == dy && column == dx)
				box(p, x, y, x + 8, y + 8, row == 0 && column == 0 ? GREY : GOLD);
			else
				outline(p, x, y, x + 8, y + 8, row == 0 && column == 0 ? DIM : frame);
		}
	for (int button = 0; button < 3; button++)
	{
		const int x = left + 44 + button * 24, y = top + 9;
		const bool on = own_game && (held & buttons[button].bit);

		if (on)
			box(p, x, y, x + 20, y + 14, GOLD);
		else
			outline(p, x, y, x + 20, y + 14, frame);
		put_char(p, x + 6, y + 3, buttons[button].name, on ? 0x10101Cu : frame, 1);
	}
}

/* ---- the settings in use (right panel, at the bottom) ---- */

static widgets_status status;

void widgets_set_status(const widgets_status *now)
{
	status = *now;
}

static void draw_setting(panel *p, int y, const char *name, const char *value, uint32_t colour)
{
	put_text(p, 8, y, name, GREY, 1);
	put_right(p, p->w - 8, y, value, colour);
}

#define ROW(n)  (5 + 8 * (n))    /* the settings' lines, from the top of theirs */

static void draw_status(panel *p, int top)
{
	char text[16];

	box(p, 8, top, p->w - 8, top + 1, FRAME);
	if (status.fast_forward > 0)
	{
		snprintf(text, sizeof(text), ">>%dX", status.fast_forward);
		draw_setting(p, top + ROW(0), "SPEED", text, YELLOW);
	}
	else
		draw_setting(p, top + ROW(0), "TIMING", status.arcade_timing ? "ARCADE" : "FAST", WHITE);
	draw_setting(p, top + ROW(1), "FREE PLAY", status.free_play ? "ON" : "OFF", WHITE);
	draw_setting(p, top + ROW(2), "CHEATS", status.cheats ? "ON" : "OFF", status.cheats ? RED : WHITE);
	/* the game's frames a second over the last second (owner, 21-11-2026: in place of this game's share of frames
	   slowed): the board's 57.55 less its frames of the arcade's slowdown; SLOWDOWN OFF when that is turned off */
	if (!status.slowdown)
		draw_setting(p, top + ROW(3), "SLOWDOWN", "OFF", WHITE);
	else
	{
		const double rate = 1000000.0 / (double)CAVE_FRAME_USEC;
		const int frames = speed.filled ? speed.filled : 1;

		snprintf(text, sizeof(text), "%.1f", rate * (double)(frames - speed.lagged_now) / (double)frames);
		draw_setting(p, top + ROW(3), "FPS", text, speed.lagged_now ? YELLOW : WHITE);
	}
	/* the enemies' bullets on the screen, under the slowdown they cause (in place of C AUTO, the EEPROM's C button
	   auto fire: as every game after DonPachi has it, it needn't be shown) */
	snprintf(text, sizeof(text), "%u", bullets_now);
	draw_setting(p, top + ROW(4), "BULLETS", text, bullets_now >= 50u ? YELLOW : WHITE);
	draw_setting(p, top + ROW(5), "AUTOBOMB", status.autobomb ? "ON" : "OFF", status.autobomb ? YELLOW : WHITE);
}

/* ---- the FPS meter, on the right panel over the score graph ---- */

static widgets_meter meter;

void widgets_set_meter(const widgets_meter *now)
{
	meter = *now;
}

static void draw_meter(panel *p)
{
	const int top = GRAPH_Y - 3, bottom = GRAPH_Y + GRAPH_H + 3, w = p->w;
	char text[24];
	const struct { const char *name; double value; const char *format; uint32_t colour; } rows[6] = {
		{ "GAME", meter.game, "%.1f", meter.game < meter.board - 0.5 ? YELLOW : GREEN },
		{ "BOARD", meter.board, "%.1f", meter.board < 57.0 ? RED : WHITE },
		{ "WINDOW", meter.windows, "%.1f", WHITE },
		{ "LATE", (double)meter.late, "%.0f", meter.late ? RED : WHITE },
		{ "WORK MS", meter.work, "%.1f", WHITE },
		{ "MOST MS", meter.work_most, "%.1f", meter.work_most > 17.4 ? YELLOW : WHITE },
	};

	box(p, 4, top, w - 4, bottom, 0x14142Au);
	outline(p, 4, top, w - 4, bottom, FRAME);
	put_centre(p, top + 8, "FPS METER", GOLD, 1);
	for (int row = 0; row < 6; row++)
	{
		const int y = top + 24 + row * 11;

		put_text(p, 10, y, rows[row].name, GREY, 1);
		snprintf(text, sizeof(text), rows[row].format, rows[row].value);
		put_right(p, w - 8, y, text, rows[row].colour);
	}
}

void widgets_draw(uint32_t *left, uint32_t *right, int width)
{
	panel p;

	p.w = width < WIDGET_W ? WIDGET_W : width > WIDGET_MAX_W ? WIDGET_MAX_W : width;
	p.pixels = left;
	frame_panel(&p, "GET POINT");
	{
		/* the players in the game (player 1's, dimmed, when there is none); player 2's only when there is one */
		int top = 26;

		if (in_play(0) || !in_play(1))
		{
			draw_chain(&p, 0, top);
			top += CHAIN_HEIGHT;
		}
		if (in_play(1))
		{
			draw_chain(&p, 1, top);
			top += CHAIN_HEIGHT;
		}
		/* the rank and the run under the chains (closer together with two players) */
		top -= 2;
		draw_rank(&p, top);
		draw_areas(&p, top + RANK_HEIGHT, in_play(0) && in_play(1));
		/* the area's bees, then the controls, with one player (two leave no room) */
		if (!(in_play(0) && in_play(1)))
		{
			draw_bees(&p, 248);
			draw_input(&p, 248 + BEES_HEIGHT, in_play(1) && !in_play(0) ? 1 : 0);
		}
	}

	p.pixels = right;
	frame_panel(&p, "SCORE GRAPH");
	put_centre(&p, 22, "VS #1 SCORE", GREY, 1);
	draw_graph(&p);
	draw_stage(&p, 200);
	draw_status(&p, 266);
	if (meter.on)
		draw_meter(&p);
	if (toast_frames > 0)
		draw_toast(&p);
}

/* ---- the menu's tooltips: a panel of their own ---- */

/* 'text' from 'y', broken at its spaces into lines that fit the panel (8 pixels a letter, 6 at each side): the y
   after it */
static int put_wrapped(panel *p, int y, const char *text, uint32_t colour)
{
	const int most = (p->w - 12) / 8;
	char line[40];

	while (*text && y < WIDGET_H - 12)
	{
		int length = (int)strlen(text), take = length;

		if (length > most)
		{
			/* (the last space that fits; a word too long for a line is cut) */
			take = most;
			while (take > 0 && text[take] != ' ')
				take--;
			if (take == 0)
				take = most;
		}
		snprintf(line, sizeof(line), "%.*s", take, text);
		put_text(p, 6, y, line, colour, 1);
		y += 11;
		text += take;
		while (*text == ' ')
			text++;
	}
	return y;
}

void widgets_draw_help(uint32_t *pixels, int width, const char *title, const char *text)
{
	panel p;
	int y;

	p.w = width < WIDGET_W ? WIDGET_W : width > WIDGET_MAX_W ? WIDGET_MAX_W : width;
	p.pixels = pixels;
	frame_panel(&p, "HELP");
	y = put_wrapped(&p, 26, title, GOLD);
	box(&p, 6, y + 2, p.w - 6, y + 3, FRAME);
	put_wrapped(&p, y + 10, text, WHITE);
}

unsigned widgets_take_defeats(void)
{
	const unsigned events = defeat_events;

	defeat_events = 0;
	return events;
}

/* ---- save states (main.c): the run as the panels had it (the session's best score stays) ---- */

void *widgets_state_save(void)
{
	unsigned char *const copy = malloc(sizeof(own_game) + sizeof(graph) + sizeof(run) + sizeof(boss) + sizeof(midboss));
	unsigned char *at = copy;

	if (!copy)
		return NULL;
	memcpy(at, &own_game, sizeof(own_game));
	memcpy(at += sizeof(own_game), &graph, sizeof(graph));
	memcpy(at += sizeof(graph), &run, sizeof(run));
	memcpy(at += sizeof(run), &boss, sizeof(boss));
	memcpy(at += sizeof(boss), &midboss, sizeof(midboss));
	return copy;
}

void widgets_state_load(const void *state)
{
	const unsigned char *at = state;

	if (!state)
		return;
	memcpy(&own_game, at, sizeof(own_game));
	memcpy(&graph, at += sizeof(own_game), sizeof(graph));
	memcpy(&run, at += sizeof(graph), sizeof(run));
	memcpy(&boss, at += sizeof(run), sizeof(boss));
	memcpy(&midboss, at += sizeof(boss), sizeof(midboss));
}

/* ---- what the gadget panels show (gadgets.h) ---- */

void widgets_view_now(widgets_view *v)
{
	const int area = area_now();

	memset(v, 0, sizeof(*v));
	v->own = own_game;
	for (int player = 0; player < 2; player++)
	{
		v->in_play[player] = in_play(player);
		if (v->in_play[player])
		{
			v->chain[player].hits = word(hits[player]);
			v->chain[player].timer = word(timers[player]);
			v->chain[player].best = word(bests[player]);
			v->chain[player].points = bcd_long(chain_points[player]);
		}
		v->score[player] = graph.last_score[player];
	}
	v->rank_known = own_game && area >= 0;
	v->rank = word(RANK) & 0xFFu;
	if (v->rank > RANK_MAX)
		v->rank = RANK_MAX;
	areas_view(v);
	v->bee_area = own_game ? run.bee_area : -1;
	v->bees_taken = v->bee_area >= 0 ? run.bees[v->bee_area] : 0u;
	v->bee_area_over = v->bee_area >= 0 && area != v->bee_area;
	{
		const int player = in_play(1) && !in_play(0) ? 1 : 0;

		v->held = own_game ? (uint16_t)~(player ? cave_board.in1 : cave_board.in0) : 0u;
	}
	/* the high score chase and the graph */
	v->graph_running = graph.running;
	v->target = graph.running ? graph.target : bcd_long(TOP_SCORE);
	v->chasing = graph.running ? chasing() : 0;
	for (int place = 0; place < 5; place++)
	{
		v->table[place] = graph.running ? graph.table[place] : bcd_long(TOP_SCORE + 4u * (uint32_t)place);
		v->passed_at[place] = graph.running ? graph.passed_at[place] : -1;
	}
	v->graph_count = graph.count;
	memcpy(v->points, graph.points, sizeof(v->points));
	memcpy(v->played, graph.played, sizeof(v->played));
	v->mark_count = graph.mark_count;
	memcpy(v->marks, graph.marks, sizeof(v->marks));
	/* the area's bar: as draw_stage's */
	{
		const unsigned stage = word(STAGE), loop = word(LOOP);

		v->area_known = own_game && stage < 5u && loop < 2u;
		v->area = (int)(loop * 5u + stage);
		v->loop = (int)loop;
		if (!v->area_known)
			v->bar = WIDGET_BAR_NONE;
		else if (boss.area == v->area && boss.state != BOSS_NONE)
		{
			const bool fight = boss.state == BOSS_FIGHT;

			v->bar = boss.type == BOSS_TRUE ? WIDGET_BAR_TRUE_BOSS : WIDGET_BAR_BOSS;
			v->percent = !fight || boss.timed_out || boss.most <= 0 ? 0
				: (int)((boss.health * 100 + boss.most - 1) / boss.most);
			if (fight && !boss.timed_out)
				snprintf(v->bar_text, sizeof(v->bar_text), "%d%%", v->percent);
			else
				snprintf(v->bar_text, sizeof(v->bar_text), "%s", boss.state == BOSS_DEFEATED ? "DEFEATED" : "TIME UP");
		}
		else if (midboss.area == v->area && midboss.state != BOSS_NONE)
		{
			const bool fight = midboss.state == BOSS_FIGHT;

			v->bar = WIDGET_BAR_MIDBOSS;
			v->percent = !fight || midboss.most <= 0 ? 0 : (int)((midboss.health * 100 + midboss.most - 1) / midboss.most);
			if (fight)
				snprintf(v->bar_text, sizeof(v->bar_text), "%d%%", v->percent);
			else
				snprintf(v->bar_text, sizeof(v->bar_text), "%s", midboss.state == BOSS_DEFEATED ? "DEFEATED" : "ESCAPED");
		}
		else
		{
			const int scroll = (int16_t)word(SCROLL) - start_at[stage], end = boss_at[stage] - start_at[stage];

			v->bar = WIDGET_BAR_PROGRESS;
			v->percent = scroll <= 0 ? 0 : scroll >= end ? 100 : scroll * 100 / end;
			snprintf(v->bar_text, sizeof(v->bar_text), v->percent >= 100 ? "BOSS" : "%d%%", v->percent);
		}
	}
	v->gained = own_game && run.gained;
	v->last_gain = run.last_gain;
	v->last_gain_hits = run.last_gain_hits;
	{
		const int frames = speed.filled ? speed.filled : 1;

		v->board_rate = 1000000.0 / (double)CAVE_FRAME_USEC;
		v->game_rate = v->board_rate * (double)(frames - speed.lagged_now) / (double)frames;
		v->lagged_now = speed.lagged_now > 0;
	}
	v->bullets = bullets_now;
	v->status = status;
	snprintf(v->toast, sizeof(v->toast), "%s", toast_frames > 0 ? toast_name : "");
	v->toast_frames = toast_frames;
	v->toast_more = toast_more;
	v->meter = meter;
	v->session_frames = session_frames;
	v->session_games = session_games;
	v->session_credits = session_credits;
	v->session_best = session_best;
}
