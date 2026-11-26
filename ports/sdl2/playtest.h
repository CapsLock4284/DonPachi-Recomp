/**
 *    Name:        playtest.h
 *    Description: --playtest: the game played through by itself, as fast as the PC runs it, player 1 kept from
 *                 harm, to see a build go all the way (both loops, area 10's true last boss, the ending) after a change
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    donpachi --playtest [all | A | B | C]   (all: the default, each ship in turn). The board from its power on with
 *    the ROM set's own EEPROM (the arcade's settings: the player's EEPROM, high scores, achievements, statistics,
 *    replays and settings neither read nor written), the arcade's slowdown on, no sound, no free play. A coin and
 *    Start, the ship chosen on the select screen (training.c's presses), then it plays as training's way to a place
 *    does (training.c's towards_target): the stick towards the strongest enemy on the screen, the shot tapped, the
 *    laser held now and then, a bomb every 900 frames; player 1's respawn shield held ($102196, as the test tools'
 *    shielded player and the P1 INVINCIBLE cheat) and its shot at MAX power (as the MAX POWER cheat: the bosses down
 *    within their time). Each area's start is noted with the game's time; a run passes when the ending has come
 *    (after area 10's true last boss, which comes only when the area's first boss is shot down) and the game has
 *    gone back to its title. It fails when the ship was lost (the continue's count), the game ended before the
 *    ending, the ending came without the true last boss, the ship chosen was not the one asked for, or an area went
 *    on for 30 minutes of the game's time. The report: donpachi_playtest.txt beside the program, the window's title
 *    as it goes, and a message at the end (the exit code: 0 all passed, 1 not). A crash ends it as any crash does
 *    (donpachi.log), the report then saying where the game was
 */
#ifndef DONPACHI_PLAYTEST_H
#define DONPACHI_PLAYTEST_H

#include <stdbool.h>

enum { PLAYTEST_GOING, PLAYTEST_PASSED, PLAYTEST_FAILED };

/* The runs asked for ("all", or a ship: "B"; case free): false if not one */
bool playtest_choose(const char *which);

/* Its start: the report's file ('path') and the game's version (0 US, 1 Japan), for the report */
void playtest_begin(const char *path, int version);

/* Player 1's inputs for the next frame (the board's bits, 1 = held: the port puts them in IN0 inverted) */
unsigned playtest_inputs(void);

/* After each frame: the shield held, the progress noted. PLAYTEST_GOING, or the end: PLAYTEST_PASSED (every run) or
   PLAYTEST_FAILED (the first that failed, and the report written) */
int playtest_frame(void);

/* What it is doing, for the window's title: "type B (2 of 3)  AREA 7  41:10" */
const char *playtest_status(void);

/* The end: the report written (with the time it took, in seconds), and its text */
const char *playtest_finish(double seconds, bool stopped_by_player);

/* Its last line, after playtest_finish: "ALL PASSED: 1:09:54 of the game's time in 104 seconds ..." */
const char *playtest_summary(void);

#endif
