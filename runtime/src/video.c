/**
 *    Name:        video.c
 *    Description: DonPachi's picture from the board's memories (cave/video.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After MAME (BSD-3-Clause, Luca Elia and others): cave_v.cpp's screen_update and tilemap_draw, sprite013.cpp's
 *    sprites (DonPachi's: the "alt" format, no zoom), tmap038.cpp's tiles. Not yet: the layers' row-scroll and
 *    row-select (DonPachi does not use them, MAME's notes say), flipped layers and sprites.
 */
#include "cave/video.h"

#include "cave/machine.h"
#include "cave/rom_set.h"

#include <string.h>

#define LAYER_OFFS_X       0x13      /* video_start: m_layers_offs_x */
#define LAYER_OFFS_Y       (-0x12)
#define LAYER_COLOR_BASE   0x400     /* gfx_donpachi: the layers' colours from 400, 64 palettes of 16 */
#define BACKGROUND_PEN     (0x400 + 63 * 16)
#define SPRITE_BANK_WORDS  0x2000u   /* 0x4000 bytes a bank */
#define MAX_SPRITE_CLOCK   (448u * 272u * 2u)    /* sprite013: what the chip can draw in a frame */

static uint16_t sprite_layer[VIDEO_HEIGHT][VIDEO_WIDTH];    /* pen | colour and priority << 8, 0 for none */
static uint8_t priority[VIDEO_HEIGHT][VIDEO_WIDTH];

/* xGRB 555 to 0x00RRGGBB */
static uint32_t colour(unsigned pen)
{
	const uint16_t word = cave_board.palette[pen & (CAVE_PALETTE_WORDS - 1u)];
	const uint32_t g = word >> 10 & 31u, r = word >> 5 & 31u, b = word & 31u;

	return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

/* ---- the tile layers (tmap038) ---- */

/* The pen of layer 'layer' at its 512 x 512 map's point (x, y), with its tile's colour and priority */
static unsigned layer_pixel(int layer, unsigned x, unsigned y, unsigned *palette_out, unsigned *priority_out)
{
	const uint16_t *const ram = cave_board.layer_ram[layer];
	const uint8_t *const gfx = rom_set.layer[layer];
	const uint32_t tiles = (layer == 2 ? DONPACHI_LAYER2_SIZE : DONPACHI_LAYER01_SIZE) / 32u;
	const int big = (cave_board.layer_regs[layer][1] & 0x2000u) != 0 && layer != 2;
	uint32_t tile, code;
	uint8_t byte;

	x &= 511u;
	y &= 511u;
	if (big)
	{
		const unsigned index = (y >> 4) * 32u + (x >> 4);

		tile = (uint32_t)ram[index * 2u] << 16 | ram[index * 2u + 1u];
		code = (tile & 0xFFFFu) * 4u;
		code ^= (x >> 3) & 1u;
		code ^= ((y >> 3) & 1u) * 2u;
	}
	else
	{
		const unsigned index = (y >> 3) * 64u + (x >> 3);

		tile = (uint32_t)ram[0x2000u + index * 2u] << 16 | ram[0x2000u + index * 2u + 1u];
		code = tile & 0x3FFFFu;
	}
	*palette_out = tile >> 24 & 0x3Fu;
	*priority_out = tile >> 30;
	code %= tiles;
	byte = gfx[code * 32u + (y & 7u) * 4u + ((x & 7u) >> 1)];
	return (x & 1u) ? (byte & 15u) : (byte >> 4);
}

/* tilemap_draw: the tiles of priority 'tile_priority' of a layer whose own priority is 'layer_priority' */
static void draw_layer(uint32_t *frame, int layer, unsigned tile_priority, unsigned layer_priority)
{
	const uint16_t *const regs = cave_board.layer_regs[layer];
	const int big = (regs[1] & 0x2000u) != 0 && layer != 2;
	int offs_x = LAYER_OFFS_X;
	int sx, sy;

	if ((regs[2] & 3u) != layer_priority || (regs[2] & 0x10u))
		return;
	offs_x -= big ? (layer + 1) : (layer + 1 + 8);
	sx = (int)(regs[0] & 0x1FFu) - (int)cave_board.video_regs[0] - offs_x;
	sy = (int)(regs[1] & 0x1FFu) - (int)cave_board.video_regs[1] - LAYER_OFFS_Y;
	for (int y = 0; y < VIDEO_HEIGHT; y++)
		for (int x = 0; x < VIDEO_WIDTH; x++)
		{
			unsigned palette, tile_pri;
			const unsigned pen = layer_pixel(layer, (unsigned)(x + sx), (unsigned)(y + sy), &palette, &tile_pri);

			if (pen == 0 || tile_pri != tile_priority)
				continue;
			frame[y * VIDEO_WIDTH + x] = colour(LAYER_COLOR_BASE + palette * 16u + pen);
			priority[y][x] = (uint8_t)(tile_priority + 1u);
		}
}

/* ---- the sprites (sprite013, DonPachi's format: 8 words a sprite, no zoom) ---- */

static unsigned sprite_pen(uint32_t pixel)
{
	const uint8_t byte = rom_set.sprites[(pixel >> 1) & (DONPACHI_SPRITES_SIZE - 1u)];

	return (pixel & 1u) ? (byte & 15u) : (byte >> 4);
}

void video_latch_sprites(void)
{
	const unsigned bank = cave_board.video_regs[4] & 3u;
	const uint16_t *source = &cave_board.sprite_ram[SPRITE_BANK_WORDS * bank];
	const uint16_t *const finish = source + SPRITE_BANK_WORDS;
	uint32_t clock = 0;

	memset(sprite_layer, 0, sizeof(sprite_layer));
	for (; source < finish; source += 8)
	{
		const uint16_t attr = source[0];
		const uint32_t code = source[1] + ((uint32_t)(attr & 3u) << 16);
		int x = source[2] & 0x3FF, y = source[3] & 0x3FF;
		const uint16_t size = source[4];
		const int width = (size >> 8 & 0x1F) * 16, height = (size & 0x1F) * 16;
		const int flipx = (attr & 8u) != 0, flipy = (attr & 4u) != 0;
		const uint16_t colpri = (uint16_t)((attr & 0x0030u) << 10 | (attr & 0x3F00u));

		clock += 32u;
		if (clock > MAX_SPRITE_CLOCK)
			break;
		if (x & 0x200)
			x -= 0x400;
		if (y & 0x200)
			y -= 0x400;
		if (!width || !height || x + width <= 0 || x >= VIDEO_WIDTH || y + height <= 0 || y >= VIDEO_HEIGHT)
			continue;
		clock += (uint32_t)(width * height);
		if (clock > MAX_SPRITE_CLOCK)
			break;
		for (int row = 0; row < height; row++)
		{
			const int draw_y = y + row;
			const int src_y = flipy ? height - 1 - row : row;

			if (draw_y < 0 || draw_y >= VIDEO_HEIGHT)
				continue;
			for (int column = 0; column < width; column++)
			{
				const int draw_x = x + column;
				const int src_x = flipx ? width - 1 - column : column;
				unsigned pen;

				if (draw_x < 0 || draw_x >= VIDEO_WIDTH)
					continue;
				pen = sprite_pen((code << 8) + (uint32_t)(src_y * width + src_x));
				if (pen)
					sprite_layer[draw_y][draw_x] = (uint16_t)(pen | colpri);
			}
		}
	}
}

/* sprite013's draw with cave_v.cpp's colpri_cb: a sprite of priority p goes over the tiles of priorities below p */
static void draw_sprites(uint32_t *frame)
{
	for (int y = 0; y < VIDEO_HEIGHT; y++)
		for (int x = 0; x < VIDEO_WIDTH; x++)
		{
			const uint16_t pixel = sprite_layer[y][x];
			unsigned colpri, pri;

			if (!pixel)
				continue;
			colpri = pixel >> 8;
			pri = colpri >> 6 & 3u;
			if (priority[y][x] <= pri)
			{
				frame[y * VIDEO_WIDTH + x] = colour((pixel & 0xFFu) + (colpri & 0x3Fu) * 16u);
				priority[y][x] = 0xFF;
			}
		}
}

void video_draw(uint32_t *frame)
{
	const uint32_t background = colour(BACKGROUND_PEN);

	for (int index = 0; index < VIDEO_WIDTH * VIDEO_HEIGHT; index++)
		frame[index] = background;
	memset(priority, 0, sizeof(priority));
	for (unsigned pri = 0; pri <= 3u; pri++)
		for (unsigned pri2 = 0; pri2 <= 3u; pri2++)
			for (int layer = 0; layer < 3; layer++)
				draw_layer(frame, layer, pri, pri2);
	draw_sprites(frame);
}

void video_draw_layers(uint32_t *frame, unsigned layers)
{
	/* (as video_draw's passes leave it, in one: each pixel the last drawn of the layers' opaque ones, in the order
	   of the tile's priority, then the layer's, then the layer's number) */
	const uint32_t background = colour(BACKGROUND_PEN);
	int sx[3], sy[3], used[3], count = 0;

	for (int layer = 0; layer < 3; layer++)
	{
		const uint16_t *const regs = cave_board.layer_regs[layer];
		const int big = (regs[1] & 0x2000u) != 0 && layer != 2;
		const int offs_x = LAYER_OFFS_X - (big ? (layer + 1) : (layer + 1 + 8));

		if (!(layers & (1u << layer)) || (regs[2] & 0x10u))
			continue;
		sx[count] = (int)(regs[0] & 0x1FFu) - (int)cave_board.video_regs[0] - offs_x;
		sy[count] = (int)(regs[1] & 0x1FFu) - (int)cave_board.video_regs[1] - LAYER_OFFS_Y;
		used[count++] = layer;
	}
	for (int y = 0; y < VIDEO_HEIGHT; y++)
		for (int x = 0; x < VIDEO_WIDTH; x++)
		{
			uint32_t pixel = background;
			int best = -1;

			for (int at = 0; at < count; at++)
			{
				const int layer = used[at];
				unsigned palette, tile_pri;
				const unsigned pen = layer_pixel(layer, (unsigned)(x + sx[at]), (unsigned)(y + sy[at]), &palette,
					&tile_pri);
				const int key = (int)(tile_pri * 16u + (cave_board.layer_regs[layer][2] & 3u) * 4u) + layer;

				if (pen && key > best)
				{
					best = key;
					pixel = colour(LAYER_COLOR_BASE + palette * 16u + pen);
				}
			}
			frame[y * VIDEO_WIDTH + x] = pixel;
		}
}

void video_rotated(const uint32_t *frame, uint32_t *picture)
{
	/* (ROT270: the board's top row is the picture's left column, read from the bottom up) */
	for (int y = 0; y < VIDEO_HEIGHT; y++)
		for (int x = 0; x < VIDEO_WIDTH; x++)
			picture[(VIDEO_WIDTH - 1 - x) * VIDEO_HEIGHT + y] = frame[y * VIDEO_WIDTH + x];
}
