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

#include "cave/machine.h"
#include "cave/rom_set.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
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

static const uint32_t records[2] = { 0x102158u, 0x102198u };
static const uint32_t timers[2] = { 0x101742u, 0x101760u }, hits[2] = { 0x10174Eu, 0x10176Cu };
static const uint32_t bests[2] = { 0x101754u, 0x101772u }, chain_points[2] = { 0x10174Au, 0x101768u };
static const uint32_t scores[2] = { 0x10177Au, 0x10177Eu };
#define TOP_SCORE 0x101846u

/* The game's mode ($1018EC): 0xB the ship select, which only a start with a credit leads to; 7 the play; 0, 1 and 5
   the attract mode and its demonstration (whose ships play as the players' would). A player's own game is from the
   ship select on, until the attract mode */
#define GAME_MODE 0x1018ECu

static bool own_game;

static void follow_game_mode(void)
{
	const unsigned mode = word(GAME_MODE);

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
#define GRAPH_H     104
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

static const uint32_t lives_at[2] = { LIVES, LIVES + 2u }, bombs_at[2] = { BOMBS, BOMBS + 0x40u };

static struct
{
	unsigned frames;                /* played (the game's mode 7) */
	unsigned bombs[2];
	unsigned misses[2][AREAS];
	bool reached[AREAS];
	unsigned last_lives[2], last_bombs[2];
	bool last_bomb_on;
} run;

static uint32_t session_best;       /* the best score since the program started: a reset keeps it */

static void run_reset(void)
{
	memset(&run, 0, sizeof(run));
	for (int player = 0; player < 2; player++)
	{
		run.last_lives[player] = word(lives_at[player]);
		run.last_bombs[player] = word(bombs_at[player]);
	}
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

	if (word(GAME_MODE) == 7u)
		run.frames++;
	if (area >= 0)
		run.reached[area] = true;
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
		run.last_lives[player] = lives;
		run.last_bombs[player] = bombs;
		if (bcd_long(scores[player]) > session_best)
			session_best = bcd_long(scores[player]);
	}
	run.last_bomb_on = bomb_on;
}

void widgets_reset(void)
{
	memset(&graph, 0, sizeof(graph));
	memset(&run, 0, sizeof(run));
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

void widgets_frame(void)
{
	const bool was_own = own_game;

	follow_game_mode();
	if (own_game && !was_own)
	{
		/* a player's game begins (the ship select): a new graph, against the #1 as it is now */
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
	if (!own_game)
		return;    /* (the last game's graph and run stay, through the attract mode) */
	run_frame();
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

/* the scroll position where each stage's boss is reached, measured: the loops' are the same but area 10's. Area 1's
   boss comes as the scroll slows to half speed (630), and it goes on slowly to 792 through the fight; the others stop
   at their boss (area 2's speeds up for its chase) */
static const int boss_at[2][5] = { { 630, 566, 647, 920, 548 }, { 630, 566, 647, 920, 1011 } };
/* where the stage's own scrolling starts: area 1 opens with the ship's fast fly-in, to 256 */
static const int start_at[5] = { 256, 0, 0, 0, 0 };

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
	/* how far through it: the bar fills to the boss; then BOSS */
	outline(p, 8, top + 40, WIDGET_W - 8, top + 52, known ? GREY : DIM);
	if (known)
	{
		const int scroll = (int16_t)word(SCROLL) - start_at[stage], end = boss_at[loop][stage] - start_at[stage];
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
	}

	p.pixels = right;
	frame_panel(&p, "SCORE GRAPH");
	put_centre(&p, 22, "VS #1 SCORE", GREY, 1);
	draw_graph(&p);
	draw_stage(&p, 236);
}
