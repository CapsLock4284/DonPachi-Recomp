/**
 *    Name:        stagemap.h
 *    Description: the stage mini-map's pictures: each area's background, its start to its boss, made from the game
 *                 itself once (donpachi_maps.dat beside the program) and shown on the right side panel
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    No picture of a whole area exists in the ROM set: the game streams its tiles into the layers' 512 x 512 maps as
 *    it scrolls. So each area is played through once, unseen (training's way to its start, the ship shielded, no
 *    input), and the background layers (0 and 1: the ground and what stands on it; 2 is the HUD) are drawn every 16
 *    frames and joined: each picture placed by how far the layers scrolled (layer 0's x scroll register, the
 *    cabinet's up). The joined picture is made small (104 pixels wide) and kept in RGB 565, with the place in it of
 *    each step of the stage's scroll ($101A56), so the panel knows where the screen is. The second loop's areas are
 *    the first's (the same backgrounds). The file is made from the player's own ROM set: never shipped.
 */
#ifndef DONPACHI_STAGEMAP_H
#define DONPACHI_STAGEMAP_H

#include <stdbool.h>
#include <stdint.h>

#define STAGEMAP_WIDTH 104      /* the pictures' width, in the panel's pixels */
#define STAGEMAP_SCREEN 138     /* the screen's height in them (320 game pixels made small) */

/* The maps' file read: false if it is not there (or not right): then stagemap_build */
bool stagemap_load(const char *path);

/* The maps made (each area played through unseen: about two seconds an area) and saved to 'path': NULL, or what went
   wrong. 'each' (or NULL) is called before each area (0-4), for the port to show how far it is. The board is left in
   the middle of area 5: the port resets it after */
const char *stagemap_build(const char *path, void (*each)(int stage));

bool stagemap_ready(void);

/* Stage 0-4's picture: its height (0: none) and its pixels (RGB 565, STAGEMAP_WIDTH a row, the top first) */
int stagemap_height(int stage);
const uint16_t *stagemap_pixels(int stage);

/* Where the screen's top is in stage 0-4's picture (a row, from the picture's top) at the stage's scroll 'scroll' */
int stagemap_screen_top(int stage, int scroll);

#endif
