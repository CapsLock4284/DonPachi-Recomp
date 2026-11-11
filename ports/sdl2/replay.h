/**
 *    Name:        replay.h
 *    Description: replays: every arcade game recorded as it is played and saved as a file (replays/ beside the
 *                 program) that can be shared and played back from the main menu's REPLAYS
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    The game is deterministic: from the board's reset, the same inputs each frame make the same game. So a replay is
 *    the EEPROM the board started with (the difficulty and the test mode's settings), the inputs of each frame as the
 *    program read them (after the port's autofire and free play), the port's own writes to the game's memory (the
 *    high score table put back from its file; the big HIT display's flags, a bit with the inputs), and a check of the
 *    board every so often to tell a replay gone out of step. Recorded from each reset; a game in it (from its ship
 *    select to the attract mode) is saved as it ends, from the reset (playback runs the frames before it unseen).
 *    Not recorded: a run with training, a cheat or a loaded save state in it since the reset.
 */
#ifndef DONPACHI_REPLAY_H
#define DONPACHI_REPLAY_H

#include <stdbool.h>
#include <stdint.h>

#define REPLAY_BIG_HIT  1u    /* the frame's flags: the big HIT display on */

/* ---- recording ---- */

/* The folder replays are saved in and read from (made when the first is saved) */
void replay_folder(const char *path);

/* The board was just reset (with the EEPROM it has now): a new recording */
void replay_reset(void);

/* This run can't be a replay (a cheat, a save state loaded, training), until the next reset */
void replay_taint(void);

/* Before each frame of the game: the inputs it is given, and the frame's flags (REPLAY_BIG_HIT) */
void replay_input(uint16_t in0, uint16_t in1, unsigned flags);

/* The port wrote the game's memory after the frame (the high score table put back): 'size' bytes at 'address' */
void replay_patch(uint32_t address, unsigned size);

/* After each frame: the checks, and a game begun or ended (saved as it ends). The name of a replay just saved (for
   a notice), or NULL */
const char *replay_after_frame(void);

/* A game being recorded is saved as it is (the game reset, the main menu, the program closing); its name, or NULL */
const char *replay_save_unfinished(void);

/* ---- the list ---- */

typedef struct
{
	char file[256];             /* its name in the folder */
	char date[20];              /* "2026-11-11 15:20" */
	uint32_t score;             /* player 1's best */
	int area;                   /* the furthest area reached, 1-10 */
	int ship;                   /* 0-2 */
	int difficulty;             /* the EEPROM's: 0 normal, 1 easy, 2 hard, 3 very hard */
	bool finished;              /* played to the game's end (not stopped part way) */
	uint32_t frames;            /* the game's frames, from its ship select */
} replay_info;

/* The replays in the folder, newest first, at most 'most': how many */
int replay_list(replay_info *out, int most);

/* ---- playback ---- */

/* The replay loaded and the board reset to its start (its EEPROM; the player's own kept to put back): NULL, or what
   is wrong with it. The frames before its game are run at once, unseen */
const char *replay_play(const char *file);

bool replay_playing(void);

/* Before each frame of playback: the frame's inputs set on the board; its flags. False: the replay is over */
bool replay_next(unsigned *flags);

/* After each frame of playback: its patches put back, its check made (false: out of step with the recording, once) */
bool replay_after_playback_frame(void);

/* Playback stopped: the player's EEPROM back (the port resets the board) */
void replay_stop(void);

#endif
