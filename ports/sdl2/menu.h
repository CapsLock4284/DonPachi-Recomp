/**
 *    Name:        menu.h
 *    Description: the window port's options menu: drawn over the paused game with the game's own 8x8 font (its text
 *                 layer's tiles, from the player's ROM set), worked with a gamepad or the keys
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The gamepad's View button or Escape opens it; the game is not run while it is open (it goes on from the same
 *    frame). D-pad, stick or arrows move; A, Enter, Z or Space choose; left and right change a value; B, Backspace or
 *    X go back; View or Escape close. Its changes go to donpachi.ini at once.
 */
#ifndef DONPACHI_MENU_H
#define DONPACHI_MENU_H

#include "settings.h"

#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>

/* What the menu works on: the port's settings and what it does with them */
typedef struct
{
	settings *options;          /* the settings in use (the file's, and the command line's over them) */
	settings *file;             /* the file's: the menu's changes go to both, and this is saved */
	const char *path;           /* donpachi.ini */
	SDL_GameController *const *pads;    /* the two players' gamepads (NULL: none) */
	void (*reset)(bool service_menu);
	void (*apply_window)(void); /* the window's size and full screen from options */
	void (*apply_video)(void);  /* the filter and vsync from options */
	void (*apply_timing)(void); /* options->arcade_timing */
	void (*quit)(void);
	void (*state)(bool save);   /* the save state: kept, or gone back to (the chosen slot's) */
	int (*state_slot)(int step);    /* the save states' slot, 0 to STATE_SLOTS - 1, moved by 'step' (0: as it is) */
	bool (*state_used)(int slot);   /* a save state is kept in the slot */
	void (*main_menu)(void);    /* the game reset, for the main menu (then it shows) */
	bool (*training)(void);     /* the game to training's place (options->training_...): false if it could not */
	bool (*in_training)(void);  /* a training game is on (the options then have RETRY TRAINING and TRAINING MENU) */
	void (*retry_training)(void);   /* the training game back to its place, as it began */
	bool (*play_replay)(const char *file);    /* a replay played (its file in the replays folder): false if not */
	bool (*in_replay)(void);    /* a replay is playing (the options then have STOP REPLAY) */
	void (*keep_replay)(bool keep);    /* the answer to "save the replay?" */
	bool (*replay_in_game)(void);      /* an arcade game is being recorded (MAIN MENU asks about its replay) */
	void (*hold_replay)(void);         /* that game, stopped there, to keep or not */
	void (*save_training_replay)(void);    /* a training game's SAVE REPLAY */
} menu_host;

void menu_init(const menu_host *host);

bool menu_is_open(void);
void menu_open(void);

/* The main menu (the title screen: START GAME, TRAINING, SETTINGS, QUIT TO DESKTOP), as the program starts and from
   the options' MAIN MENU: the game is not run while it is up (menu_is_open); START GAME closes it */
void menu_open_front(void);
bool menu_in_front(void);

/* "DO YOU WANT TO SAVE THE REPLAY?" YES / NO, as a game ends (the game paused until it is answered); after it, the
   game goes on, or the main menu (the question came from MAIN MENU), or the training page (an area cleared in
   training) */
enum { MENU_AFTER_RESUME, MENU_AFTER_MAIN_MENU, MENU_AFTER_TRAINING_PAGE };
void menu_ask_save_replay(int after);

/* The replays page (the game reset first by the port: a replay's end, or STOP REPLAY) */
void menu_open_replays(void);

/* The training page (the game reset first by the port: its training game over, or TRAINING MENU) */
void menu_open_training(void);

/* The chosen entry's tooltip, for the right side panel: its name and what it does (false: none, or the menu is not
   up) */
bool menu_help(const char **title, const char **text);

/* An event while it is open: true when the menu took it (all are taken while it is open) */
bool menu_event(const SDL_Event *event);

/* Once a frame while it is open: the stick's and held buttons' repeats, the remapping's wait */
void menu_tick(void);

/* The picture to show: the game's picture (240 x 320, as the cabinet shows it) dimmed, the menu over it */
void menu_draw(const uint32_t *game, uint32_t *out);

/* A line of text over the game for a moment ("STATE SAVED"), in the game's font */
void menu_notice(const char *text);

/* The picture with the notice over it while there is one (else the picture itself): once a frame shown */
const uint32_t *menu_with_notice(const uint32_t *picture);

/* Closed since the last call (then the port lets the game ignore what is still held, and starts the sound again) */
bool menu_just_closed(void);

#endif
