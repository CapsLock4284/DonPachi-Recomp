/**
 *    Name:        widgets.h
 *    Description: the side panels: what the game is doing, from its memory, beside the picture on a wide screen:
 *                 the Get Point System's chain, the rank and the run's statistics (left); the score against the #1
 *                 high score, and the area, loop and how far through the stage (right)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    From the game's memory, player 1 / player 2 (the game's own code: func_0066C4 scores a kill, 006E3A counts the
 *    chain down once a frame, 0331A0 starts a game, 0069A2 sets the high score table):
 *      $101742 / $101760  the chain timer: 30 on each kill, down 1 a frame; at 0 the chain is over
 *      $10174E / $10176C  the chain's hits          $101754 / $101772  the best chain ("MAX GP COUNTS")
 *      $10174A / $101768  the chain's points, the HUD's "+N GP" (BCD; $101746 / $101764 the sum of the base values
 *                         that builds them)          $10177A / $10177E  the score (BCD)
 *      $101846            the high score table's #1 (BCD; then #2-#5)
 *      $101A40            the stage of the loop, 0-4 (5: between the loops, the ending)   $101A42  the loop, 0/1
 *      $101A56            the stage's scroll position (signed: from -1; the bosses at 630 (area 1: from 256, after
 *                         the fly-in), 566, 647, 920, 548 / 1011 in area 10)
 *      $101A32 (low byte) the rank, 0-63      $101A2E / $101A30  the lives      $10217A / $1021BA  the bombs
 *      $106E14 bit 15     a bomb going off
 *    The game is not touched: the panels only read.
 */
#ifndef DONPACHI_WIDGETS_H
#define DONPACHI_WIDGETS_H

#include <stdint.h>

#define WIDGET_W    120     /* a panel, in the game's pixels: drawn at the picture's scale */
#define WIDGET_H    320
#define WIDGET_GAP  8       /* between the picture and a panel */

/* After each of the game's frames: what the graph keeps (the score's history, the stages' marks) */
void widgets_frame(void);

/* Forgets the run (the game reset) */
void widgets_reset(void);

/* Both panels, WIDGET_W x WIDGET_H each, upright as the player sees them */
void widgets_draw(uint32_t *left, uint32_t *right);

#endif
