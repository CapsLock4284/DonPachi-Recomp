/**
 *    Name:        widgets.c
 *    Description: the side panels (widgets.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The font is the game's text layer, as the options menu's (menu.c: ASCII less 0x20, pens 1-6 a shade, 15 the
 *    outline), here also twice as large for the hits.
 */
#include "widgets.h"
#include "game_mode.h"

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
} panel;

static void dot(panel *p, int x, int y, uint32_t colour)
{
	if (x >= 0 && x < WIDGET_W && y >= 0 && y < WIDGET_H)
		p->pixels[y * WIDGET_W + x] = colour;
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
	put_text(p, (WIDGET_W - text_width(text, size)) / 2, y, text, colour, size);
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
#define GRAPH_W     104     /* points: one a column */
#define GRAPH_H     88
#define MARKS       24

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
} graph;

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
} run;

static uint32_t session_best;       /* the best score since the program started: a reset keeps it */

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
		run.frames++;
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
}

void widgets_begin_game(void)
{
	/* a player's game begins (the ship select, or training's place): a new graph, against the #1 as it is now */
	widgets_reset();
	own_game = true;
	graph.running = true;
	graph.target = bcd_long(TOP_SCORE);
	if (graph.target == 0)
		graph.target = 1;
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

	box(p, 4, top, WIDGET_W - 4, bottom, 0x14142Au);
	outline(p, 4, top, WIDGET_W - 4, bottom, rim);
	outline(p, 6, top + 2, WIDGET_W - 6, bottom - 2, FRAME);
	/* a little gold trophy: the cup, its handles, the stem and the base */
	{
		const int c = WIDGET_W / 2, y = top + 6;

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
	if (text_width(first, 1) > WIDGET_W - 16)
	{
		char *space = NULL;

		for (char *at = first; *at && text_width(first, 1) - text_width(at, 1) <= WIDGET_W - 16; at++)
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

void widgets_frame(void)
{
	const bool was_own = own_game;

	if (toast_frames > 0)
		toast_frames--;
	if (program_frame_lagged())
	{
		/* a frame of slowdown: the game's memory is half way through its frame, so it is looked at again next; the
		   run's time goes on, and the frame is counted as slowed */
		if (own_game && run.playing)
		{
			run.frames++;
			run.slowed++;
		}
		return;
	}

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
	box(p, 0, 0, WIDGET_W, WIDGET_H, BACKGROUND);
	outline(p, 0, 0, WIDGET_W, WIDGET_H, FRAME);
	box(p, 1, 1, WIDGET_W - 1, 15, 0x14142Au);
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
	put_right(p, WIDGET_W - 8, top, "CHAIN", playing ? GREY : DIM);
	/* the hits, large, while the chain or its result is on */
	snprintf(text, sizeof(text), "%u", playing && count ? count : 0u);
	put_text(p, 8, top + 14, text, playing && count ? (timer ? GOLD : GREY) : DIM, 3);
	put_text(p, 8 + text_width(text, 3) + 4, top + 30, "HIT", playing && count ? colour : DIM, 1);
	/* the time left: 30 frames after a kill, draining, the frames left counted down inside it (30 frames: 0.52 s) */
	outline(p, 8, top + 44, WIDGET_W - 8, top + 56, playing ? GREY : DIM);
	filled = (int)((WIDGET_W - 18) * (timer > CHAIN_FRAMES ? CHAIN_FRAMES : timer) / CHAIN_FRAMES);
	if (playing && timer)
	{
		box(p, 9, top + 45, 9 + filled, top + 55, timer > 15 ? GREEN : timer > 7 ? YELLOW : RED);
		snprintf(text, sizeof(text), "%u", timer);
		for (int at = 0, x = (WIDGET_W - text_width(text, 1)) / 2; text[at]; at++, x += 8)
			put_char_split(p, x, top + 46, text[at], 0x10101Cu, WHITE, 9 + filled, 1);    /* (dark on the bar) */
	}
	/* its points so far, and the best chain */
	put_text(p, 8, top + 62, "PTS", playing ? GREY : DIM, 1);
	number(text, sizeof(text), playing ? bcd_long(chain_points[player]) : 0u);
	put_right(p, WIDGET_W - 8, top + 62, text, colour);
	put_text(p, 8, top + 74, "MAX", playing ? GREY : DIM, 1);
	number(text, sizeof(text), playing ? word(bests[player]) : 0u);
	put_right(p, WIDGET_W - 8, top + 74, text, colour);
}

#define CHAIN_HEIGHT 96     /* a player's chain, with the space before the next */

static void draw_graph(panel *p)
{
	const int bottom = GRAPH_Y + GRAPH_H;
	float most = 1.0f;
	char text[32];

	/* (the height: 100% at the top, or the best score if a player is past the #1) */
	for (int player = 0; player < 2; player++)
		for (int at = 0; at < graph.count; at++)
			if (graph.played[player][at] && graph.points[player][at] > most)
				most = graph.points[player][at];
	box(p, GRAPH_X, GRAPH_Y, GRAPH_X + GRAPH_W, bottom, 0x000000u);
	for (int at = 0; at < graph.count; at++)
	{
		/* player 1: the area under the line, as the arcade ports' rank graphs */
		const int height = graph.played[0][at] ? (int)(graph.points[0][at] / most * (float)GRAPH_H) : 0;

		if (height > 0)
			box(p, GRAPH_X + at, bottom - (height > GRAPH_H ? GRAPH_H : height), GRAPH_X + at + 1, bottom, ORANGE);
	}
	for (int at = 1; at < graph.count; at++)
		if (graph.played[1][at] && graph.played[1][at - 1])
			line(p, GRAPH_X + at - 1, bottom - 1 - (int)(graph.points[1][at - 1] / most * (float)(GRAPH_H - 1)),
				GRAPH_X + at, bottom - 1 - (int)(graph.points[1][at] / most * (float)(GRAPH_H - 1)), CYAN);
	/* the 100% and 50% lines, the stages' marks */
	for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 2)
	{
		dot(p, x, bottom - (int)((float)GRAPH_H / most), WHITE);
		dot(p, x, bottom - (int)((float)GRAPH_H / most / 2.0f), GREY);
	}
	for (int mark = 0; mark < graph.mark_count; mark++)
		for (int y = GRAPH_Y; y < bottom; y += 3)
			dot(p, GRAPH_X + graph.marks[mark], y, DIM);
	outline(p, GRAPH_X - 1, GRAPH_Y - 1, GRAPH_X + GRAPH_W + 1, bottom + 1, GREY);
	/* (the top of the box: 100%, or the highest score's share when a player is past the #1) */
	snprintf(text, sizeof(text), "%d%%", (int)(most * 100.0f + 0.5f));
	put_right(p, WIDGET_W - 2, GRAPH_Y - 10, text, GREY);
	put_text(p, GRAPH_X, bottom + 3, "ST", GREY, 1);
	put_right(p, GRAPH_X + GRAPH_W, bottom + 3, "NOW", GREY);

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
	put_text(p, 8, bottom + 44, "TOP", GREY, 1);
	number(text, sizeof(text), graph.running ? graph.target : bcd_long(TOP_SCORE));
	put_right(p, WIDGET_W - 8, bottom + 44, text, WHITE);
	put_text(p, 8, bottom + 56, "NOW", GREY, 1);
	number(text, sizeof(text), graph.last_score[0]);
	put_right(p, WIDGET_W - 8, bottom + 56, text, WHITE);
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
   points (func_0055DA takes the shots' damage off them). Each boss keeps its health its own way (read from its
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
		/* area 1 (func_0213B8): the core (record 1, $5FFF until the four parts are gone) with its reserve in record
		   2, and the parts, records 3-6 */
		return positive16(hits + 0x38u) + positive16(hits + 0x58u) + positive16(hits + 0x78u)
			+ positive16(hits + 0x98u) + positive16(hits + 0xB8u) + positive16(hits + 0xD8u);
	case 0x58u:
		/* area 2 (func_026FA2): record 0, refilled from record 2's and then from +$2D6 */
		return positive16(hits + 0x18u) + positive16(hits + 0x58u) + positive16(hits + 0x2D6u);
	case 0x4Bu:
		/* area 3 (func_03985C): a long at +$30 of the records */
		pool = (int32_t)long_at(hits + 0x30u);
		return pool > 0 ? pool : 0;
	case 0x3Bu:
		/* area 4 (func_024582): record 0, refilled from the object's +$32, then +$36, then the records' +$3A */
		return positive16(hits + 0x18u) + word(object + 0x32u) + word(object + 0x36u) + word(hits + 0x3Au);
	case 0x6Bu:
	case BOSS_TRUE:
		/* area 5 and 10 (func_03B0F4), the true last boss (func_03E5C8): a long at +$1C of the records */
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

	box(p, 8, top, WIDGET_W - 8, top + 1, FRAME);
	put_text(p, 8, top + 6, "AREA", known ? GREY : DIM, 1);
	put_right(p, WIDGET_W - 8, top + 6, "LOOP", known ? GREY : DIM);
	snprintf(text, sizeof(text), "%u", loop * 5u + stage + 1u);
	put_text(p, 8, top + 16, known ? text : "-", known ? GOLD : DIM, 2);
	snprintf(text, sizeof(text), "%u", loop + 1u);
	put_text(p, WIDGET_W - 8 - text_width(known ? text : "-", 2), top + 16, known ? text : "-", known ? WHITE : DIM, 2);
	/* how far through it: the bar fills to the boss; then the boss's health, emptying as it is shot */
	outline(p, 8, top + 40, WIDGET_W - 8, top + 52, known ? GREY : DIM);
	if (known && boss.area == (int)(loop * 5u + stage) && boss.state != BOSS_NONE)
	{
		const bool fight = boss.state == BOSS_FIGHT;
		/* (rounded up: 0% only when it is gone) */
		const int left = !fight || boss.timed_out || boss.most <= 0 ? 0
			: (int)((boss.health * 100 + boss.most - 1) / boss.most);
		const int filled = (WIDGET_W - 18) * left / 100;

		box(p, 9, top + 41, 9 + filled, top + 51, boss.type == BOSS_TRUE ? GOLD : RED);
		if (fight && !boss.timed_out)
			snprintf(text, sizeof(text), "%d%%", left);
		else
			snprintf(text, sizeof(text), "%s", boss.state == BOSS_DEFEATED ? "DEFEATED" : "TIME UP");    /* (or going) */
		for (int at = 0, x = (WIDGET_W - text_width(text, 1)) / 2; text[at]; at++, x += 8)
			put_char_split(p, x, top + 42, text[at], 0x10101Cu, WHITE, 9 + filled, 1);
		put_centre(p, top + 58, boss.type == BOSS_TRUE ? "TRUE BOSS" : "BOSS HEALTH", GREY, 1);
		return;
	}
	if (known && midboss.area == (int)(loop * 5u + stage) && midboss.state != BOSS_NONE)
	{
		const bool fight = midboss.state == BOSS_FIGHT;
		const int left = !fight || midboss.most <= 0 ? 0 : (int)((midboss.health * 100 + midboss.most - 1) / midboss.most);
		const int filled = (WIDGET_W - 18) * left / 100;

		box(p, 9, top + 41, 9 + filled, top + 51, ORANGE);
		if (fight)
			snprintf(text, sizeof(text), "%d%%", left);
		else
			snprintf(text, sizeof(text), "%s", midboss.state == BOSS_DEFEATED ? "DEFEATED" : "ESCAPED");
		for (int at = 0, x = (WIDGET_W - text_width(text, 1)) / 2; text[at]; at++, x += 8)
			put_char_split(p, x, top + 42, text[at], 0x10101Cu, WHITE, 9 + filled, 1);
		put_centre(p, top + 58, "MID-BOSS", GREY, 1);
		return;
	}
	if (known)
	{
		const int scroll = (int16_t)word(SCROLL) - start_at[stage], end = boss_at[stage] - start_at[stage];
		const int done = scroll <= 0 ? 0 : scroll >= end ? 100 : scroll * 100 / end;
		const int filled = (WIDGET_W - 18) * done / 100;

		box(p, 9, top + 41, 9 + filled, top + 51, done >= 100 ? RED : CYAN);
		if (done >= 100)
			snprintf(text, sizeof(text), "BOSS");
		else
			snprintf(text, sizeof(text), "%d%%", done);
		for (int at = 0, x = (WIDGET_W - text_width(text, 1)) / 2; text[at]; at++, x += 8)
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

	box(p, 8, top, WIDGET_W - 8, top + 1, FRAME);
	put_text(p, 8, top + 6, "RANK", known ? GREY : DIM, 1);
	snprintf(text, sizeof(text), "%u/%u", known ? rank : 0u, RANK_MAX);
	put_right(p, WIDGET_W - 8, top + 6, known ? text : "-", known ? WHITE : DIM);
	outline(p, 8, top + 18, WIDGET_W - 8, top + 26, known ? GREY : DIM);
	if (known)
	{
		const int filled = (WIDGET_W - 18) * (int)(rank > RANK_MAX ? RANK_MAX : rank) / RANK_MAX;
		/* green, to yellow at half, to red at the top */
		const uint32_t colour = rank < 22u ? GREEN : rank < 43u ? YELLOW : RED;

		box(p, 9, top + 19, 9 + filled, top + 25, colour);
	}
}

#define RANK_HEIGHT 34

/* ---- the run ---- */

static void put_time(char *out, size_t size, unsigned frames)
{
	/* the game's frames are 17376 us (57.55 a second) */
	const unsigned long seconds = (unsigned long)((unsigned long long)frames * 17376u / 1000000u);

	if (seconds >= 3600u)
		snprintf(out, size, "%lu:%02lu:%02lu", seconds / 3600u, seconds / 60u % 60u, seconds % 60u);
	else
		snprintf(out, size, "%lu:%02lu", seconds / 60u, seconds % 60u);
}

static void draw_run(panel *p, int top, bool two)
{
	const uint32_t label = own_game ? GREY : DIM;
	char text[32];
	int y = top + 6;

	box(p, 8, top, WIDGET_W - 8, top + 1, FRAME);
	put_time(text, sizeof(text), run.frames);
	put_text(p, 8, y, "TIME", label, 1);
	put_right(p, WIDGET_W - 8, y, text, WHITE);
	y += 12;
	put_text(p, 8, y, "BOMBS", label, 1);
	if (two)
		snprintf(text, sizeof(text), "%u / %u", run.bombs[0], run.bombs[1]);
	else
		snprintf(text, sizeof(text), "%u", run.bombs[0]);
	put_right(p, WIDGET_W - 8, y, text, WHITE);
	y += 12;
	{
		unsigned total[2] = { 0, 0 };

		for (int player = 0; player < 2; player++)
			for (int area = 0; area < AREAS; area++)
				total[player] += run.misses[player][area];
		/* (two players' "1 / 0" leaves no room for the longer name) */
		put_text(p, 8, y, two ? "DEATHS" : "LIVES LOST", label, 1);
		if (two)
			snprintf(text, sizeof(text), "%u / %u", total[0], total[1]);
		else
			snprintf(text, sizeof(text), "%u", total[0]);
		put_right(p, WIDGET_W - 8, y, text, WHITE);
		y += 12;
	}
	if (!two)
	{
		/* player 1's misses in each area: the loops one above the other */
		const int now = own_game ? area_now() : -1;

		for (int column = 0; column < 5; column++)
		{
			char digit[2] = { (char)('1' + column), 0 };

			put_char(p, 8 + column * 21 + 6, y, digit[0], DIM, 1);
		}
		y += 10;
		for (int loop = 0; loop < 2; loop++, y += 13)
			for (int column = 0; column < 5; column++)
			{
				const int area = loop * 5 + column, x = 8 + column * 21;
				const unsigned misses = run.misses[0][area];

				outline(p, x, y, x + 19, y + 11, area == now ? GOLD : FRAME);
				if (!run.reached[area])
					put_char(p, x + 6, y + 2, '-', DIM, 1);
				else
				{
					const char ch = misses > 9u ? '+' : (char)('0' + misses);

					put_char(p, x + 6, y + 2, ch, misses == 0u ? GREEN : misses == 1u ? YELLOW : RED, 1);
				}
			}
		y += 2;
	}
	number(text, sizeof(text), session_best);
	put_text(p, 8, y, "BEST", label, 1);
	put_right(p, WIDGET_W - 8, y, text, GOLD);
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
			if (x + column >= 0 && x + column < WIDGET_W && y + row >= 0 && y + row < WIDGET_H)
				p->pixels[(y + row) * WIDGET_W + x + column] = r << 16 | g << 8 | b;
		}
}

#define BEES_HEIGHT 33

static void draw_bees(panel *p, int top)
{
	const unsigned taken = own_game && run.bee_area >= 0 ? run.bees[run.bee_area] : 0u;

	box(p, 8, top, WIDGET_W - 8, top + 1, FRAME);
	for (int bee = 0; bee < BEES; bee++)
	{
		const int row = bee / 7, column = bee % 7;

		draw_bee(p, 8 + column * 15 + row * 7, top + 5 + row * 14, (unsigned)bee < taken);    /* (the 6 of the second row centred) */
	}
}

/* ---- the controls: what the board is given (the stick and the three buttons) ---- */

static void draw_input(panel *p, int top, int player)
{
	const uint16_t held = (uint16_t)~(player ? cave_board.in1 : cave_board.in0);
	const uint32_t frame = own_game ? GREY : DIM;
	/* the stick: a 3 x 3 of cells, the one it points to lit */
	const int dx = held & CAVE_LEFT ? -1 : held & CAVE_RIGHT ? 1 : 0, dy = held & CAVE_UP ? -1 : held & CAVE_DOWN ? 1 : 0;
	static const struct { uint16_t bit; char name; } buttons[3] = {
		{ CAVE_BUTTON1, 'S' }, { CAVE_BUTTON2, 'B' }, { CAVE_BUTTON3, 'C' }
	};

	box(p, 8, top, WIDGET_W - 8, top + 1, FRAME);
	for (int row = -1; row <= 1; row++)
		for (int column = -1; column <= 1; column++)
		{
			const int x = 8 + (column + 1) * 9, y = top + 6 + (row + 1) * 9;

			if (own_game && row == dy && column == dx)
				box(p, x, y, x + 8, y + 8, row == 0 && column == 0 ? GREY : GOLD);
			else
				outline(p, x, y, x + 8, y + 8, row == 0 && column == 0 ? DIM : frame);
		}
	for (int button = 0; button < 3; button++)
	{
		const int x = 44 + button * 24, y = top + 9;
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
	put_right(p, WIDGET_W - 8, y, value, colour);
}

static void draw_status(panel *p, int top)
{
	char text[16];

	box(p, 8, top, WIDGET_W - 8, top + 1, FRAME);
	if (status.fast_forward > 0)
	{
		snprintf(text, sizeof(text), ">>%dX", status.fast_forward);
		draw_setting(p, top + 5, "SPEED", text, YELLOW);
	}
	else
		draw_setting(p, top + 5, "TIMING", status.arcade_timing ? "ARCADE" : "FAST", WHITE);
	/* the board's own rapid fire: the EEPROM's "C BUTTON FULL-AUTO" (word 4, bit 2) */
	draw_setting(p, top + 14, "C AUTO", cave_board.eeprom.words[4] & 0x0004u ? "ON" : "OFF", WHITE);
	draw_setting(p, top + 23, "FREE PLAY", status.free_play ? "ON" : "OFF", WHITE);
	draw_setting(p, top + 32, "CHEATS", status.cheats ? "ON" : "OFF", status.cheats ? RED : WHITE);
	/* the arcade's slowdown: off, or this game's share of frames slowed (on, before there is a game) */
	if (!status.slowdown)
		draw_setting(p, top + 41, "SLOWDOWN", "OFF", WHITE);
	else if (own_game && run.frames > 0u)
	{
		const unsigned tenths = (unsigned)(((unsigned long long)run.slowed * 1000u + run.frames / 2u) / run.frames);

		if (tenths < 100u)
			snprintf(text, sizeof(text), "%u.%u%%", tenths / 10u, tenths % 10u);    /* (0.4%) */
		else
			snprintf(text, sizeof(text), "%u%%", (tenths + 5u) / 10u);              /* (12%) */
		draw_setting(p, top + 41, "SLOWDOWN", text, run.slowed ? YELLOW : WHITE);
	}
	else
		draw_setting(p, top + 41, "SLOWDOWN", "ON", WHITE);
}

void widgets_draw(uint32_t *left, uint32_t *right)
{
	panel p;

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
		draw_run(&p, top + RANK_HEIGHT, in_play(0) && in_play(1));
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
	draw_status(&p, 268);
	if (toast_frames > 0)
		draw_toast(&p);
}

/* ---- the menu's tooltips: a panel of their own ---- */

/* 'text' from 'y', broken at its spaces into lines that fit the panel (8 pixels a letter, 6 at each side): the y
   after it */
static int put_wrapped(panel *p, int y, const char *text, uint32_t colour)
{
	const int most = (WIDGET_W - 12) / 8;
	char line[32];

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

void widgets_draw_help(uint32_t *pixels, const char *title, const char *text)
{
	panel p;
	int y;

	p.pixels = pixels;
	frame_panel(&p, "HELP");
	y = put_wrapped(&p, 26, title, GOLD);
	box(&p, 6, y + 2, WIDGET_W - 6, y + 3, FRAME);
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
