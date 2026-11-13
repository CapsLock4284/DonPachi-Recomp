/**
 *    Name:        ai_player.h
 *    Description: player 2 flown by the computer, beside player 1 (the cheats' AI PLAYER 2: own_cheats.h). It joins
 *                 the game itself (a coin in player 2's slot, then its Start) while player 1 plays, and again after
 *                 a game over; it dodges the bullets it can see coming, goes for the strongest enemy, taps its shot
 *                 and bombs when there's no way out
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 */
#ifndef AI_PLAYER_H
#define AI_PLAYER_H

/* Player 2's inputs for the next frame (the board's bits, 1 = held), from the game's memory now: the port puts them in
   cave_board.in1 (inverted) instead of player 2's keys and gamepad */
unsigned ai_player_inputs(void);

/* Forgets what it saw (the game reset, a save state loaded) */
void ai_player_reset(void);

#endif
