/**
 *    Name:        hitbox.h
 *    Description: the ships' hitboxes shown over the picture: a white glowing dot, the box the game itself tests
 *                 bullets and enemies against; and the bullets' points, what it tests
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's test (ship_hit_test, run for each ship from 000950 and 000966): a bullet's point, or an enemy's box,
 *    against the ship's box, from its record (player 1 at $102158, player 2 at $102198): position +06 (along the
 *    board's x: up the cabinet's screen) and +08 (the board's y: across it), in 64ths of a pixel; the box reaches
 *    +10 above +06 and +12 below it, +14 above +08 and +16 below it (narrower as the ship banks). A ship is in play
 *    while bit 15 of its word +00 is set. The sprites on the screen are a frame behind the game's memory (two with
 *    arcade timing: the sprite banks flip at VBLANK), so the dot drawn is the box of the frame the ship shown is from.
 *    The record is in play before the ship is on the screen (from the ship select, where the game's first area
 *    begins, before the ship flies in), so a box is drawn only while the ship's own sprite (its code, the record's
 *    +04) is in the sprite bank shown, near it, during play: from its third frame there, kept 8 frames past it.
 *    The bullets (BULLETS): the 75 records from $104E94, $40 bytes each, that ship_hit_test goes through: one counts
 *    while its +00's top byte has bit 7 (in play) and bit 0, and not bit 4 (it has hit); its point is +06, +08, as a
 *    ship's position (the test moves both by $800, the same). Each one's point is drawn, a white pixel in a green
 *    cross: a bullet hits as that point comes inside a ship's box.
 */
#ifndef DONPACHI_HITBOX_H
#define DONPACHI_HITBOX_H

#include <stdbool.h>
#include <stdint.h>

/* After each of the game's frames: both ships' boxes, kept for a few frames */
void hitbox_record(void);

/* The boxes of 'frames_ago' frames ago drawn on 'picture' (240 x 320, as the cabinet shows it): the ships', the
   bullets' points, as asked */
void hitbox_draw(uint32_t *picture, int frames_ago, bool ships, bool bullets);

#endif
