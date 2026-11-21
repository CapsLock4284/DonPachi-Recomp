/**
 *    Name:        layer_filter.c
 *    Description: the pixel-art scaler on each layer and the sprites apart (layer_filter.h)
 *    Version:     0.1.0 (draft)
 *    Created:     20-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "layer_filter.h"
#include "scaler.h"

#include "cave/video.h"

#include <stdlib.h>
#include <string.h>

#define PIXELS (VIDEO_WIDTH * VIDEO_HEIGHT)

static uint32_t board_planes[VIDEO_PLANES][PIXELS];    /* as video_draw_planes makes them */
static uint32_t turned[VIDEO_PLANES][PIXELS];          /* turned as the picture */
static uint32_t *scaled[VIDEO_PLANES];                 /* each made larger (the largest factor yet) */
static size_t scaled_room;                             /* pixels in each */
static int plane_count;                                /* 0: none kept */
static uint32_t background;

void layer_filter_draw(uint32_t *frame, uint32_t *picture)
{
	uint32_t *planes[VIDEO_PLANES];

	for (int plane = 0; plane < VIDEO_PLANES; plane++)
		planes[plane] = board_planes[plane];
	plane_count = video_draw_planes(planes, &background);
	video_compose((const uint32_t *const *)planes, plane_count, background, PIXELS, frame);
	video_rotated(frame, picture);
	for (int plane = 0; plane < plane_count; plane++)
		video_rotated(board_planes[plane], turned[plane]);    /* (a copy: the keys go with the colours) */
}

void layer_filter_forget(void)
{
	plane_count = 0;
}

bool layer_filter_scale(int scaler, const uint32_t *shown, const uint32_t *game, uint32_t *out)
{
	const int factor = scaler_factor(scaler), w = VIDEO_HEIGHT, h = VIDEO_WIDTH;    /* (the picture: 240 x 320) */
	const size_t room = (size_t)PIXELS * (size_t)(factor * factor);
	const uint32_t *planes[VIDEO_PLANES];
	int drawn_over = 0;

	if (plane_count == 0 || !scaler_by_layer(scaler) || factor < 2)
		return false;
	/* (what the port drew over the picture: a few pixels, kept sharp; most of it, as the menu's, the whole picture's
	   scaler instead) */
	for (int at = 0; at < PIXELS; at++)
		drawn_over += shown[at] != game[at];
	if (drawn_over > PIXELS / 4)
		return false;
	if (scaled_room < room)
	{
		for (int plane = 0; plane < VIDEO_PLANES; plane++)
		{
			free(scaled[plane]);
			scaled[plane] = malloc(room * sizeof(uint32_t));
		}
		scaled_room = room;
		for (int plane = 0; plane < VIDEO_PLANES; plane++)
			if (!scaled[plane])
			{
				layer_filter_quit();
				return false;
			}
	}
	for (int plane = 0; plane < plane_count; plane++)
	{
		scaler_run_layer(scaler, turned[plane], w, h, scaled[plane]);
		planes[plane] = scaled[plane];
	}
	video_compose(planes, plane_count, background, room, out);
	if (drawn_over)
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				if (shown[y * w + x] != game[y * w + x])
					for (int dy = 0; dy < factor; dy++)
						for (int dx = 0; dx < factor; dx++)
							out[(size_t)(y * factor + dy) * (size_t)(w * factor) + (size_t)(x * factor + dx)] =
								shown[y * w + x];
	return true;
}

void layer_filter_quit(void)
{
	for (int plane = 0; plane < VIDEO_PLANES; plane++)
	{
		free(scaled[plane]);
		scaled[plane] = NULL;
	}
	scaled_room = 0;
}
