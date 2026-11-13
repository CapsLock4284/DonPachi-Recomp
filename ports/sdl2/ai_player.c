/**
 *    Name:        ai_player.c
 *    Description: player 2 flown by the computer (ai_player.h)
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's memory it reads (the positions in 64ths of a pixel: +8 across the picture, 0-240; +6 up it, 0-320,
 *    0 at the bottom):
 *      $1018EC the game's mode (7 playing); $101A31 player 2's lives; $10139A the credits, $101287 bit 0: 2 a game
 *      $102198 player 2's ship's record: +0 bit 15 flying, +6 / +8 its place, +$22 its bombs
 *      $102158 player 1's (+6 / +8)
 *      $104E94 the enemies' bullets, 75 of $40 bytes: +0 bit 7 in use, bit 0 able to hit, bit 4 not (the game's own
 *        test, func_0055DA); +6 / +8 the place (their speed: from the frame before)
 *      $103814 (100) and $104494 (50) the enemies' hit records, $20 bytes: +0 bits 15 and 13 able to be shot, +6 /
 *        +8 the place, +$10 / +$12 / +$14 / +$16 the box around it, +$18 the hit points
 *    Each frame: the 9 ways the stick can go (still and the 8), each tried 12 frames ahead, the bullets moved on at
 *    their speed and the enemies where they are (a touch is a life): the safest, then the nearest to where it wants
 *    to be (under the strongest enemy, else beside player 1, low in the picture). The shot tapped all the time;
 *    a bomb when every way is hit within a few frames.
 */
#include "ai_player.h"
#include "game_mode.h"

#include "cave/machine.h"

#include <stdlib.h>
#include <string.h>

#define P2_LIVES    0x101A31u
#define CREDITS     0x10139Au
#define COINAGE     0x101287u
#define P1_SHIP     0x102158u
#define P2_SHIP     0x102198u
#define BULLETS     0x104E94u
#define BULLET_MOST 75

#define UNIT        64          /* a pixel */
#define WIDTH       (240 * UNIT)
#define HEIGHT      (320 * UNIT)
#define SPEED       176         /* the ship's way a frame (2.75 pixels) */
#define AHEAD       12          /* frames looked ahead */
#define HIT         (7 * UNIT)  /* a bullet this near the ship's middle: a hit, as far as the bot cares */
#define NEAR        (20 * UNIT) /* nearer than this: some danger */
#define HOME_Y      (48 * UNIT) /* where it likes to be, up from the bottom */

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

static int position(uint32_t address)
{
	return (int16_t)word(address);
}

static unsigned byte(uint32_t address)
{
	return *cave_work_ram_byte(address);
}

/* ---- joining: a coin in player 2's slot, then its Start ---- */

static struct
{
	int phase, frames;
} join;

enum { WAIT, COIN, GAP, START, SETTLE };

static unsigned joining(void)
{
	const unsigned need = (byte(COINAGE) & 1u) ? 2u : 1u;

	switch (join.phase)
	{
	case WAIT:
		join.phase = word(CREDITS) < need ? COIN : START;
		join.frames = 6;
		return 0;
	case COIN:
		if (--join.frames == 0)
		{
			join.phase = GAP;
			join.frames = 8;
		}
		return CAVE_COIN;
	case GAP:
		if (--join.frames == 0)
		{
			join.phase = word(CREDITS) < need ? COIN : START;
			join.frames = 6;
		}
		return 0;
	case START:
		if (--join.frames == 0)
		{
			join.phase = SETTLE;
			join.frames = 90;
		}
		return CAVE_START;
	default:
		if (--join.frames == 0)
			join.phase = WAIT;    /* (not in yet: again) */
		return 0;
	}
}

/* ---- flying ---- */

typedef struct
{
	int x, y, dx, dy;
} thing;

static thing bullets[BULLET_MOST], last[BULLET_MOST];
static bool was[BULLET_MOST];
static int last_way = 0;
static long frame;

static const int ways[9][2] = { { 0, 0 }, { 0, 1 }, { 0, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 }, { -1, -1 },
	{ 1, -1 } };
static const unsigned way_inputs[9] = { 0, CAVE_UP, CAVE_DOWN, CAVE_LEFT, CAVE_RIGHT, CAVE_LEFT | CAVE_UP,
	CAVE_RIGHT | CAVE_UP, CAVE_LEFT | CAVE_DOWN, CAVE_RIGHT | CAVE_DOWN };

static int clamp(int value, int low, int high)
{
	return value < low ? low : value > high ? high : value;
}

/* the bullets that can hit, with their way from the frame before */
static int read_bullets(void)
{
	int count = 0;

	for (int b = 0; b < BULLET_MOST; b++)
	{
		const uint32_t at = BULLETS + 0x40u * (uint32_t)b;
		const unsigned flags = byte(at);
		const bool live = (flags & 0x80u) && (flags & 1u) && !(flags & 0x10u);
		thing t = { position(at + 8u), position(at + 6u), 0, 0 };

		if (live && was[b] && abs(t.x - last[b].x) < 12 * UNIT && abs(t.y - last[b].y) < 12 * UNIT)
		{
			t.dx = t.x - last[b].x;
			t.dy = t.y - last[b].y;
		}
		last[b] = t;
		was[b] = live;
		if (live)
			bullets[count++] = t;
	}
	return count;
}

/* the enemies on the screen that can be shot (their middles and half sizes), and the strongest's across */
static int read_enemies(thing *out, int most, int *target_x)
{
	int count = 0, strongest = -1;

	*target_x = -1;
	for (int r = 0; r < 150 && count < most; r++)
	{
		const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
		const int x = position(at + 8u), y = position(at + 6u), hp = (int16_t)word(at + 0x18u);

		if ((word(at) & 0xA000u) != 0xA000u || x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
			continue;
		out[count].x = x;
		out[count].y = y;
		out[count].dx = (int)(word(at + 0x14u) > word(at + 0x16u) ? word(at + 0x14u) : word(at + 0x16u));
		out[count].dy = (int)(word(at + 0x10u) > word(at + 0x12u) ? word(at + 0x10u) : word(at + 0x12u));
		count++;
		if (hp > strongest && y > HOME_Y + 40 * UNIT)
		{
			strongest = hp;
			*target_x = x;
		}
	}
	return count;
}

/* how dangerous the way is: the bullets near the ship as it goes, nearer frames counting more; 'first_hit' the
   first frame a bullet hits (AHEAD + 1: none) */
static long danger(int way, const thing *shots, int shots_count, const thing *enemies, int enemies_count, int x,
	int y, int *first_hit)
{
	long total = 0;

	*first_hit = AHEAD + 1;
	for (int k = 1; k <= AHEAD; k++)
	{
		const int sx = clamp(x + ways[way][0] * SPEED * k, 8 * UNIT, WIDTH - 8 * UNIT);
		const int sy = clamp(y + ways[way][1] * SPEED * k, 8 * UNIT, HEIGHT - 40 * UNIT);
		const long weight = AHEAD + 1 - k;

		for (int b = 0; b < shots_count; b++)
		{
			const long dx = shots[b].x + (long)shots[b].dx * k - sx, dy = shots[b].y + (long)shots[b].dy * k - sy;
			const long d2 = dx * dx + dy * dy;

			if (d2 < (long)HIT * HIT)
			{
				total += 100000 * weight;
				if (k < *first_hit)
					*first_hit = k;
			}
			else if (d2 < (long)NEAR * NEAR)
				total += (long)((NEAR * NEAR - d2) / (UNIT * UNIT)) * weight;
		}
		for (int e = 0; e < enemies_count; e++)
		{
			/* (a touch: the enemy's box and a little more) */
			if (labs((long)enemies[e].x - sx) < enemies[e].dx + 6 * UNIT
					&& labs((long)enemies[e].y - sy) < enemies[e].dy + 6 * UNIT)
			{
				total += 50000 * weight;
				if (k < *first_hit)
					*first_hit = k;
			}
		}
	}
	return total;
}

unsigned ai_player_inputs(void)
{
	static thing enemies[150];
	unsigned held = 0;
	int shots_count, enemies_count, target_x, x, y, best = 0, best_hit = 0;
	long best_cost = -1;
	const bool flying = (word(P2_SHIP) & 0x8000u) != 0;

	frame++;
	if (!game_mode_playing())
	{
		join.phase = WAIT;
		return 0;
	}
	if (!flying && byte(P2_LIVES) == 0u)
		return joining();    /* (not in the game: in) */
	join.phase = WAIT;
	shots_count = read_bullets();
	if (!flying)
		return 0;    /* (down: the next ship comes) */
	enemies_count = read_enemies(enemies, 150, &target_x);
	x = position(P2_SHIP + 8u);
	y = position(P2_SHIP + 6u);
	if (target_x < 0)
	{
		/* no enemy to go for: beside player 1, on the side with more room */
		const int p1 = position(P1_SHIP + 8u);

		target_x = p1 < WIDTH / 2 ? p1 + 48 * UNIT : p1 - 48 * UNIT;
	}
	for (int way = 0; way < 9; way++)
	{
		int first_hit;
		const int to_x = clamp(x + ways[way][0] * SPEED * AHEAD, 0, WIDTH), to_y = y + ways[way][1] * SPEED * AHEAD;
		long cost = danger(way, bullets, shots_count, enemies, enemies_count, x, y, &first_hit);

		cost += labs((long)to_x - target_x) / UNIT * 4;     /* (where it wants to be) */
		cost += labs((long)to_y - HOME_Y) / UNIT * 3;
		if (way != last_way)
			cost += 20;                                        /* (a little steadiness) */
		if (best_cost < 0 || cost < best_cost)
		{
			best_cost = cost;
			best = way;
			best_hit = first_hit;
		}
	}
	last_way = best;
	held |= way_inputs[best];
	held |= frame % 2 ? CAVE_BUTTON1 : 0u;                     /* (the shot tapped) */
	if (best_hit <= 3 && word(P2_SHIP + 0x22u) > 0u)
		held |= CAVE_BUTTON2;                                  /* (no way out: a bomb) */
	return held;
}

void ai_player_reset(void)
{
	memset(was, 0, sizeof(was));
	join.phase = WAIT;
	last_way = 0;
	frame = 0;
}
