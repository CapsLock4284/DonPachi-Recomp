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
} menu_host;

void menu_init(const menu_host *host);

bool menu_is_open(void);
void menu_open(void);

/* An event while it is open: true when the menu took it (all are taken while it is open) */
bool menu_event(const SDL_Event *event);

/* Once a frame while it is open: the stick's and held buttons' repeats, the remapping's wait */
void menu_tick(void);

/* The picture to show: the game's picture (240 x 320, as the cabinet shows it) dimmed, the menu over it */
void menu_draw(const uint32_t *game, uint32_t *out);

/* Closed since the last call (then the port lets the game ignore what is still held, and starts the sound again) */
bool menu_just_closed(void);

#endif
