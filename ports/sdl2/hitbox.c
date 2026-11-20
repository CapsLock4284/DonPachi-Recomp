/**
 *    Name:        hitbox.c
 *    Description: the ships' hitboxes shown over the picture (hitbox.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "hitbox.h"
#include "game_mode.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/video.h"

#include <stdbool.h>
#include <string.h>

#define PICTURE_W   VIDEO_HEIGHT    /* 240 */
#define PICTURE_H   VIDEO_WIDTH     /* 320 */
#define HISTORY     4
#define GLOW        2               /* pixels of glow around the box */
#define SEEN_IN_A_ROW 3             /* a ship's box shown once its sprite has been on the screen this many frames */
#define SEEN_FOR    8               /* a ship's box kept this many frames after its sprite was last seen (a flicker) */
#define SPRITE_BANK 0x2000u         /* the sprite RAM's words a bank: 8 a sprite */
#define BULLETS     0x104E94u       /* the enemies' bullets: 75 records of $40 bytes */
#define BULLET_SIZE 0x40u
#define BULLETS_MOST 75
#define BULLET_COLOUR 0x50FF70u

typedef struct
{
	bool active;
	int x0, x1, y0, y1;             /* the board's pixels, both ends in */
} box;

static const uint32_t records[2] = { 0x102158u, 0x102198u };
static box history[HISTORY][2];

/* the bullets that can hit a ship, each frame: their points, the board's pixels */
static struct
{
	int count;
	int16_t x[BULLETS_MOST], y[BULLETS_MOST];
} bullet_history[HISTORY];
static unsigned recorded;
static unsigned unseen[2] = { SEEN_FOR, SEEN_FOR };    /* the frames since each ship's sprite was last on the screen */
static unsigned seen[2];           /* the frames in a row it has been */
static bool showing[2];

static int word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2u);
}

/* 64ths of a pixel to pixels, rounding down */
static int pixels(int units)
{
	return units >= 0 ? units >> 6 : -((-units + 63) >> 6);
}

/* The ship's own sprite on the screen (the bank the board shows): its code, the record's +04, near its position.
   The record is in play before the ship is shown: from the ship select, at the place it starts from, while the game's
   first area begins, before it flies in from the bottom */
static bool ship_shown(uint32_t record, int x, int y)
{
	const uint16_t *sprite = &cave_board.sprite_ram[SPRITE_BANK * (cave_board.video_regs[4] & 3u)];
	const unsigned code = (unsigned)word(record + 0x04u);

	for (unsigned index = 0; index < SPRITE_BANK; index += 8u, sprite += 8)
	{
		int sx = sprite[2] & 0x3FF, sy = sprite[3] & 0x3FF;

		if (sprite[1] != code)
			continue;
		if (sx & 0x200)
			sx -= 0x400;
		if (sy & 0x200)
			sy -= 0x400;
		if (sx > x - 64 && sx < x + 16 && sy > y - 64 && sy < y + 16)    /* (the sprite's corner: up to 32 x 48 it) */
			return true;
	}
	return false;
}

void hitbox_record(void)
{
	box *const boxes = history[recorded++ % HISTORY];
	const unsigned at = (recorded - 1u) % HISTORY;

	if (program_frame_lagged())
	{
		/* (a frame of slowdown: the picture is the last one again, and so are its boxes; the ship's record is half
		   way through the frame) */
		if (recorded > 1u)
		{
			memcpy(boxes, history[(recorded - 2u) % HISTORY], sizeof(history[0]));
			bullet_history[at] = bullet_history[(recorded - 2u) % HISTORY];
		}
		return;
	}
	/* the bullets ship_hit_test tests: +00's top byte with bit 7 (in play) and bit 0, not bit 4 (one that has hit) */
	bullet_history[at].count = 0;
	if (game_mode_playing())
		for (int index = 0; index < BULLETS_MOST; index++)
		{
			const uint32_t record = BULLETS + BULLET_SIZE * (uint32_t)index;
			const int count = bullet_history[at].count;

			if ((word(record) & 0x9100) == 0x8100)
			{
				bullet_history[at].x[count] = (int16_t)pixels(word(record + 0x06u));
				bullet_history[at].y[count] = (int16_t)pixels(word(record + 0x08u));
				bullet_history[at].count = count + 1;
			}
		}

	for (int player = 0; player < 2; player++)
	{
		const uint32_t record = records[player];
		const int x = word(record + 0x06u), y = word(record + 0x08u);

		boxes[player].active = (word(record) & 0x8000) != 0;
		if (boxes[player].active && game_mode_playing() && ship_shown(record, pixels(x), pixels(y)))
		{
			unseen[player] = 0;
			if (seen[player] < SEEN_IN_A_ROW)
				seen[player]++;
		}
		else
		{
			seen[player] = 0;
			if (unseen[player] < SEEN_FOR)
				unseen[player]++;
		}
		/* (shown once the ship has been for a few frames in a row: not the one frame of the ship select's left on the
		   screen as the game starts; kept through a flicker) */
		if (seen[player] >= SEEN_IN_A_ROW)
			showing[player] = true;
		else if (unseen[player] >= SEEN_FOR || !boxes[player].active || !game_mode_playing())
			showing[player] = false;
		boxes[player].active = showing[player];
		boxes[player].x0 = pixels(x - word(record + 0x12u));
		boxes[player].x1 = pixels(x + word(record + 0x10u));
		boxes[player].y0 = pixels(y - word(record + 0x16u));
		boxes[player].y1 = pixels(y + word(record + 0x14u));
	}
}

/* 'percent' of 'colour' over the pixel at the picture's (column, row) */
static void tint(uint32_t *picture, int column, int row, uint32_t colour, int percent)
{
	uint32_t *pixel;
	uint32_t out = 0;

	if (column < 0 || column >= PICTURE_W || row < 0 || row >= PICTURE_H)
		return;
	pixel = &picture[row * PICTURE_W + column];
	for (int shift = 0; shift <= 16; shift += 8)
	{
		const uint32_t from = *pixel >> shift & 255u, to = colour >> shift & 255u;

		out |= (from + (to > from ? (to - from) * (uint32_t)percent / 100u : 0u)
			- (to < from ? (from - to) * (uint32_t)percent / 100u : 0u)) << shift;
	}
	*pixel = out;
}

/* 'percent' of white over the pixel at the picture's (column, row) */
static void whiten(uint32_t *picture, int column, int row, int percent)
{
	uint32_t *pixel;
	uint32_t r, g, b;

	if (column < 0 || column >= PICTURE_W || row < 0 || row >= PICTURE_H)
		return;
	pixel = &picture[row * PICTURE_W + column];
	r = *pixel >> 16 & 255u;
	g = *pixel >> 8 & 255u;
	b = *pixel & 255u;
	r += (255u - r) * (uint32_t)percent / 100u;
	g += (255u - g) * (uint32_t)percent / 100u;
	b += (255u - b) * (uint32_t)percent / 100u;
	*pixel = r << 16 | g << 8 | b;
}

void hitbox_draw(uint32_t *picture, int frames_ago, bool ships, bool bullets)
{
	const box *boxes;
	unsigned at;

	if (recorded <= (unsigned)frames_ago || frames_ago >= HISTORY)
		return;
	at = (recorded - 1u - (unsigned)frames_ago) % HISTORY;
	boxes = history[at];
	if (bullets)
		for (int index = 0; index < bullet_history[at].count; index++)
		{
			/* each bullet's point (a bullet hits as it comes inside a ship's box): a white pixel in a small green
			   cross, so it shows on any bullet (the cabinet's picture: column the board's y, row its x reversed) */
			const int column = bullet_history[at].y[index], row = (PICTURE_H - 1) - bullet_history[at].x[index];

			tint(picture, column - 1, row, BULLET_COLOUR, 85);
			tint(picture, column + 1, row, BULLET_COLOUR, 85);
			tint(picture, column, row - 1, BULLET_COLOUR, 85);
			tint(picture, column, row + 1, BULLET_COLOUR, 85);
			tint(picture, column, row, 0xFFFFFFu, 100);
		}
	for (int player = 0; ships && player < 2; player++)
	{
		const box *const b = &boxes[player];
		int left, right, top, bottom;

		if (!b->active)
			continue;
		/* (the cabinet's picture: its column is the board's y, its row the board's x from the far side) */
		left = b->y0;
		right = b->y1;
		top = (PICTURE_H - 1) - b->x1;
		bottom = (PICTURE_H - 1) - b->x0;
		for (int row = top - GLOW; row <= bottom + GLOW; row++)
			for (int column = left - GLOW; column <= right + GLOW; column++)
			{
				const int dx = column < left ? left - column : column > right ? column - right : 0;
				const int dy = row < top ? top - row : row > bottom ? row - bottom : 0;
				const int distance = dx > dy ? dx : dy;

				const bool edge = row == top || row == bottom || column == left || column == right;

				if (distance == 0)
					whiten(picture, column, row, edge ? 100 : 50);    /* (the ship shows through the middle) */
				else if (dx + dy <= GLOW + 1)    /* (rounded corners) */
					whiten(picture, column, row, distance == 1 ? 50 : 20);
			}
	}
}
