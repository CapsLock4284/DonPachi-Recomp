/**
 *    Name:        hitbox.c
 *    Description: the ships' hitboxes shown over the picture (hitbox.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "hitbox.h"

#include "cave/machine.h"
#include "cave/video.h"

#include <stdbool.h>
#include <string.h>

#define PICTURE_W   VIDEO_HEIGHT    /* 240 */
#define PICTURE_H   VIDEO_WIDTH     /* 320 */
#define HISTORY     4
#define GLOW        2               /* pixels of glow around the box */

typedef struct
{
	bool active;
	int x0, x1, y0, y1;             /* the board's pixels, both ends in */
} box;

static const uint32_t records[2] = { 0x102158u, 0x102198u };
static box history[HISTORY][2];
static unsigned recorded;

static int word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2u);
}

/* 64ths of a pixel to pixels, rounding down */
static int pixels(int units)
{
	return units >= 0 ? units >> 6 : -((-units + 63) >> 6);
}

void hitbox_record(void)
{
	box *const boxes = history[recorded++ % HISTORY];

	for (int player = 0; player < 2; player++)
	{
		const uint32_t record = records[player];
		const int x = word(record + 0x06u), y = word(record + 0x08u);

		boxes[player].active = (word(record) & 0x8000) != 0;
		boxes[player].x0 = pixels(x - word(record + 0x12u));
		boxes[player].x1 = pixels(x + word(record + 0x10u));
		boxes[player].y0 = pixels(y - word(record + 0x16u));
		boxes[player].y1 = pixels(y + word(record + 0x14u));
	}
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

void hitbox_draw(uint32_t *picture, int frames_ago)
{
	const box *boxes;

	if (recorded <= (unsigned)frames_ago)
		return;
	boxes = history[(recorded - 1u - (unsigned)frames_ago) % HISTORY];
	for (int player = 0; player < 2; player++)
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
