/**
 *    Name:        stagemap.c
 *    Description: the stage mini-map's pictures, made and kept (stagemap.h)
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    The file (little endian): "DPMAPS01", the stages (8: 5), then each stage: its picture's height (16), its first
 *    scroll value (16, signed), the scroll values (16), the screen's top row for each (16 each), the pixels (RGB 565,
 *    STAGEMAP_WIDTH a row).
 */
#include "stagemap.h"

#include "training.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAGES          5
#define CANVAS_ROWS     12000   /* the full-size picture's most rows (area 4's is about 7500) */
#define DRAW_EVERY      16      /* frames between the pictures drawn (the layers move 8 pixels a frame at most) */
#define SCROLL          0x101A56u
#define SHIELD          0x102196u
#define FIRST_SCROLL    (-64)

static const char magic[8] = { 'D', 'P', 'M', 'A', 'P', 'S', '0', '1' };

/* where each stage's boss appears: the scroll the picture ends at (widgets.c's) */
static const int boss_at[STAGES] = { 555, 576, 629, 888, 539 };

static struct
{
	int height;
	int first, count;           /* the scroll values the table has: first .. first + count - 1 */
	uint16_t *screen_top;
	uint16_t *pixels;
} maps[STAGES];
static bool ready;

static void free_maps(void)
{
	for (int stage = 0; stage < STAGES; stage++)
	{
		free(maps[stage].screen_top);
		free(maps[stage].pixels);
		memset(&maps[stage], 0, sizeof(maps[stage]));
	}
	ready = false;
}

bool stagemap_ready(void)
{
	return ready;
}

int stagemap_height(int stage)
{
	return ready && stage >= 0 && stage < STAGES ? maps[stage].height : 0;
}

const uint16_t *stagemap_pixels(int stage)
{
	return ready && stage >= 0 && stage < STAGES ? maps[stage].pixels : NULL;
}

int stagemap_screen_top(int stage, int scroll)
{
	if (!ready || stage < 0 || stage >= STAGES || !maps[stage].count)
		return 0;
	scroll -= maps[stage].first;
	if (scroll < 0)
		scroll = 0;
	if (scroll >= maps[stage].count)
		scroll = maps[stage].count - 1;
	return maps[stage].screen_top[scroll];
}

/* ---- the file ---- */

static void put16(FILE *file, unsigned value)
{
	fputc((int)(value & 0xFFu), file);
	fputc((int)(value >> 8 & 0xFFu), file);
}

static unsigned get16(FILE *file, bool *ok)
{
	const int a = fgetc(file), b = fgetc(file);

	if (a == EOF || b == EOF)
		*ok = false;
	return (unsigned)(a & 0xFF) | (unsigned)(b & 0xFF) << 8;
}

static bool save(const char *path)
{
	FILE *const file = fopen(path, "wb");

	if (!file)
		return false;
	fwrite(magic, 1, sizeof(magic), file);
	fputc(STAGES, file);
	for (int stage = 0; stage < STAGES; stage++)
	{
		put16(file, (unsigned)maps[stage].height);
		put16(file, (unsigned)(uint16_t)(int16_t)maps[stage].first);
		put16(file, (unsigned)maps[stage].count);
		for (int at = 0; at < maps[stage].count; at++)
			put16(file, maps[stage].screen_top[at]);
		for (int at = 0; at < STAGEMAP_WIDTH * maps[stage].height; at++)
			put16(file, maps[stage].pixels[at]);
	}
	return fclose(file) == 0;
}

bool stagemap_load(const char *path)
{
	FILE *const file = fopen(path, "rb");
	char head[8];
	bool ok = true;

	free_maps();
	if (!file)
		return false;
	if (fread(head, 1, 8, file) != 8 || memcmp(head, magic, 8) || fgetc(file) != STAGES)
		ok = false;
	for (int stage = 0; ok && stage < STAGES; stage++)
	{
		maps[stage].height = (int)get16(file, &ok);
		maps[stage].first = (int16_t)get16(file, &ok);
		maps[stage].count = (int)get16(file, &ok);
		if (!ok || maps[stage].height <= 0 || maps[stage].height > CANVAS_ROWS || maps[stage].count <= 0
				|| !(maps[stage].screen_top = malloc(sizeof(uint16_t) * (size_t)maps[stage].count))
				|| !(maps[stage].pixels = malloc(sizeof(uint16_t) * STAGEMAP_WIDTH * (size_t)maps[stage].height)))
		{
			ok = false;
			break;
		}
		for (int at = 0; at < maps[stage].count; at++)
			maps[stage].screen_top[at] = (uint16_t)get16(file, &ok);
		for (int at = 0; ok && at < STAGEMAP_WIDTH * maps[stage].height; at++)
			maps[stage].pixels[at] = (uint16_t)get16(file, &ok);
	}
	fclose(file);
	if (!ok)
	{
		free_maps();
		return false;
	}
	ready = true;
	return true;
}

/* ---- making them ---- */

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

/* the full-size picture: its rows counted from the bottom (the area's start), 240 pixels each */
static uint32_t (*canvas)[240];

/* the stage played through, its pictures joined into canvas: its rows, and the full-size screen's bottom row (from
   the bottom) at each scroll value in 'bottoms' (-1: not reached) */
static const char *play_through(int stage, long *rows, long *bottoms, int count)
{
	const training_setup setup = { stage, TRAINING_START, 0, 0, false, 0 };
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	const char *problem;
	long pos = 0, top = 0;
	int last_register;

	if ((problem = training_prepare(&setup)) != NULL)
		return problem;
	last_register = cave_board.layer_regs[0][0] & 0x1FF;
	for (long f = 0; f < 40000; f++)
	{
		const int scroll = (int16_t)word(SCROLL);
		const bool last = scroll >= boss_at[stage];

		if (scroll >= FIRST_SCROLL && scroll - FIRST_SCROLL < count && bottoms[scroll - FIRST_SCROLL] < 0)
			bottoms[scroll - FIRST_SCROLL] = pos;
		if (f % DRAW_EVERY == 0 || last)
		{
			/* (the picture's row r at the canvas's row pos + 319 - r, counted from the bottom) */
			video_draw_layers(frame, 3u);
			video_rotated(frame, picture);
			for (int r = 0; r < VIDEO_WIDTH; r++)
			{
				const long at = pos + VIDEO_WIDTH - 1 - r;

				if (at >= 0 && at < CANVAS_ROWS)
				{
					memcpy(canvas[at], picture + r * VIDEO_HEIGHT, sizeof(canvas[at]));
					if (at > top)
						top = at;
				}
			}
		}
		if (last)
			break;
		*cave_work_ram_byte(SHIELD) = 2;
		cave_board.in0 = cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		{
			/* how far the layers moved (the cabinet's up): their x scroll register, 512 around */
			const int now = cave_board.layer_regs[0][0] & 0x1FF;
			int moved = (now - last_register) & 0x1FF;

			if (moved >= 256)
				moved -= 512;
			if (moved > -32 && moved < 32)
				pos += moved;
			last_register = now;
		}
	}
	*rows = top + 1;
	return NULL;
}

/* canvas made small into the stage's picture, and the scroll table made from 'bottoms' */
static bool make_small(int stage, long rows, const long *bottoms, int count)
{
	const int height = (int)(rows * STAGEMAP_WIDTH / VIDEO_HEIGHT);
	uint16_t *const pixels = malloc(sizeof(uint16_t) * STAGEMAP_WIDTH * (size_t)height);
	uint16_t *const tops = malloc(sizeof(uint16_t) * (size_t)count);
	long last = 0;

	if (!pixels || !tops || height <= 0)
	{
		free(pixels);
		free(tops);
		return false;
	}
	for (int y = 0; y < height; y++)
		for (int x = 0; x < STAGEMAP_WIDTH; x++)
		{
			/* (the full-size pixels under this one, averaged: rows from the picture's top) */
			const long y0 = (long)y * rows / height, y1 = (long)(y + 1) * rows / height;
			const int x0 = x * VIDEO_HEIGHT / STAGEMAP_WIDTH, x1 = (x + 1) * VIDEO_HEIGHT / STAGEMAP_WIDTH;
			unsigned long r = 0, g = 0, b = 0, n = 0;

			for (long yy = y0; yy < (y1 > y0 ? y1 : y0 + 1); yy++)
				for (int xx = x0; xx < (x1 > x0 ? x1 : x0 + 1); xx++)
				{
					const uint32_t pixel = canvas[rows - 1 - yy][xx];

					r += pixel >> 16 & 255u;
					g += pixel >> 8 & 255u;
					b += pixel & 255u;
					n++;
				}
			r /= n;
			g /= n;
			b /= n;
			pixels[y * STAGEMAP_WIDTH + x] = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
		}
	for (int at = 0; at < count; at++)
	{
		/* (the screen's top, full size, from the picture's top: rows - 1 - (bottom + 319)) */
		long full;

		if (bottoms[at] >= 0)
			last = bottoms[at];
		full = rows - 1 - (last + VIDEO_WIDTH - 1);
		if (full < 0)
			full = 0;
		tops[at] = (uint16_t)(full * height / rows);
	}
	maps[stage].height = height;
	maps[stage].first = FIRST_SCROLL;
	maps[stage].count = count;
	maps[stage].pixels = pixels;
	maps[stage].screen_top = tops;
	return true;
}

const char *stagemap_build(const char *path, void (*each)(int stage))
{
	const char *problem = NULL;

	free_maps();
	if (!(canvas = calloc(CANVAS_ROWS, sizeof(*canvas))))
		return "no memory for the maps";
	for (int stage = 0; stage < STAGES && !problem; stage++)
	{
		const int count = boss_at[stage] - FIRST_SCROLL + 1;
		long *const bottoms = malloc(sizeof(long) * (size_t)count);
		long rows = 0;

		if (!bottoms)
		{
			problem = "no memory for the maps";
			break;
		}
		for (int at = 0; at < count; at++)
			bottoms[at] = -1;
		if (each)
			each(stage);
		memset(canvas, 0, sizeof(*canvas) * CANVAS_ROWS);
		if ((problem = play_through(stage, &rows, bottoms, count)) == NULL && !make_small(stage, rows, bottoms, count))
			problem = "no memory for the maps";
		free(bottoms);
	}
	free(canvas);
	canvas = NULL;
	if (problem)
	{
		free_maps();
		return problem;
	}
	ready = true;
	if (!save(path))
		return "the maps were made, but could not be saved";
	return NULL;
}
