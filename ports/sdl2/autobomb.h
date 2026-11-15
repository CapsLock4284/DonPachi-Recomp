/**
 *    Name:        autobomb.h
 *    Description: the bomb set off for a player whose ship is about to be shot down (GAME OPTIONS > AUTOBOMB, as
 *                 many newer shooters have it): only when a bomb is left, and only when it really saves the ship
 *    Version:     0.1.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 *    The game is deterministic, so instead of guessing from the bullets it looks: before each frame of play, the
 *    board is saved and the next frames run with the inputs as they are (the player's hands held as they are now);
 *    if a ship is shot down there, they are run again with its bomb button pressed in this frame, and if the ship
 *    lives through them that way, the button is pressed for real. Then the board is put back as it was. A bomb
 *    pressed in the frame the ship is hit is too late (the game has it a frame later): it must come a frame or two
 *    before, so it looks AUTOBOMB_AHEAD frames ahead. It sets off a normal bomb, not the laser's (the shot held into
 *    the bomb): where it would be the laser's, it lets the shot go for a frame first, if there is time for that. The press is an input like the player's own, so replays keep it.
 *    A game with it on doesn't count for the high scores, achievements or statistics (an assist, as the cheats)
 */
#ifndef AUTOBOMB_H
#define AUTOBOMB_H

#include <stdbool.h>

#define AUTOBOMB_AHEAD 4    /* the frames looked ahead */

/* Before a frame is run, the board's inputs (cave_board.in0, in1) as they will be: the bomb button added for a
   player whose ship it saves. 'player2' false: player 2 left as it is (the AI player bombs for itself). True if it
   pressed one */
bool autobomb_frame(bool player2);

/* Forgets a bomb it planned for the next frame (a save state loaded, the game reset) */
void autobomb_reset(void);

/* The bombs it has set off since the program started, and how many of them were the laser's (the test tools) */
unsigned autobomb_count(void);
unsigned autobomb_laser_count(void);

#endif
