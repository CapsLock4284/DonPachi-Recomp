/**
 *    Name:        video.h
 *    Description: DonPachi's picture: the three NEC 038 tile layers, the 013 sprites and the palette, drawn from
 *                 the board's memories into a 320 x 240 frame
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After MAME (BSD-3-Clause): src/mame/atlus/cave_v.cpp (screen_update, tilemap_draw, colpri_cb), sprite013.cpp
 *    (get_sprite_info, draw_sprites, draw_single_sprite_nozoom, draw) and src/devices/video/tmap038.cpp
 *    (get_tile_info). The screen is turned on its side in the cabinet (MAME's ROT270): the frame is the board's,
 *    320 wide; video_rotated gives the picture as the player sees it, 240 wide.
 */
#ifndef CAVE_VIDEO_H
#define CAVE_VIDEO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_WIDTH   320
#define VIDEO_HEIGHT  240

/* The sprites, from the sprite RAM's bank the program shows: when VBLANK starts, as the 013 does */
void video_latch_sprites(void);

/* The frame, from the board's memories: 0x00RRGGBB pixels, VIDEO_WIDTH x VIDEO_HEIGHT */
void video_draw(uint32_t *frame);

/* The frame turned as the cabinet shows it: VIDEO_HEIGHT wide, VIDEO_WIDTH high */
void video_rotated(const uint32_t *frame, uint32_t *picture);

#ifdef __cplusplus
}
#endif

#endif
