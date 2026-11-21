/**
 *    Name:        layer_filter.h
 *    Description: GRAPHICS > BY LAYER ([video] filter_layers): the pixel-art scaler (scaler.h) on each of the game's
 *                 layers and its sprites apart, put together again at the larger size by the board's own rules, in
 *                 place of the scaler over the finished picture
 *    Version:     0.1.0 (draft)
 *    Created:     20-11-2026
 *
 *    license:BSD-3-Clause
 *    Over the finished picture a scaler sees only colours: where a sprite's outline meets a busy background it takes
 *    the two for one shape, and the edges bend into the scenery. Here each plane (cave/video.h: a tile layer, or the
 *    sprites) is scaled against what it lets show through, so a sprite keeps its own silhouette and a background its
 *    own tiles; then the planes are put together pixel by pixel (video_compose: the same picture as video_draw's at
 *    the game's size). Only the scalers that pick among their input's pixels can (Scale2x, Scale3x, MMPX); the
 *    game's picture is the same, only its larger copy on the screen changes. What the port draws over the picture
 *    (the hitboxes, the bullets' points, a notice) is kept as it is, each of its pixels a block of the larger size.
 */
#ifndef LAYER_FILTER_H
#define LAYER_FILTER_H

#include <stdbool.h>
#include <stdint.h>

/* The frame drawn from the board's memories as video_draw draws it ('frame', the board's 320 x 240) and turned
   ('picture', 240 x 320: video_rotated's), its planes kept, turned as the picture, for layer_filter_scale */
void layer_filter_draw(uint32_t *frame, uint32_t *picture);

/* The kept planes made 'scaler' (scaler_by_layer) times larger one by one and put together into 'out' (240 x 320
   times the factor); where 'shown' (the picture as it goes to the screen) differs from 'game' (the game's own, as
   layer_filter_draw made it), the shown pixel over its block. False when it can't be done this way (no planes kept
   since the last layer_filter_forget, no memory, or most of the picture drawn over, as by the menu): the caller
   then scales the shown picture as a whole */
bool layer_filter_scale(int scaler, const uint32_t *shown, const uint32_t *game, uint32_t *out);

/* The planes kept are no longer the picture's (the picture drawn another way) */
void layer_filter_forget(void);

/* Lets go of its memory (the program ending) */
void layer_filter_quit(void);

#endif
