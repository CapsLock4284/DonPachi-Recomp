/**
 *    Name:        settings.h
 *    Description: the window port's settings: donpachi.ini beside the program (the options, the keys and the gamepad
 *                 buttons of each player), read at the start; the command line's options go over it
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The file is text, "name = value" lines under [sections], with ; or # comments: settings_save writes it with
 *    every setting and what it does, and a missing file is written so. The keys are SDL's names ("Z", "Left",
 *    "Left Shift", "Keypad 8"), the gamepad's are SDL's game controller names, the Xbox layout's ("a", "b", "x",
 *    "y", "back", "start", "leftshoulder", "rightshoulder", "dpup", "dpdown", "dpleft", "dpright", "leftstick",
 *    "rightstick", "guide") and the triggers ("lefttrigger", "righttrigger"); several are put with commas.
 */
#ifndef DONPACHI_SETTINGS_H
#define DONPACHI_SETTINGS_H

#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>

/* What a player's key or button does: the board's inputs (cave/machine.h's CAVE_UP ...) */
enum
{
	ACTION_UP, ACTION_DOWN, ACTION_LEFT, ACTION_RIGHT,
	ACTION_SHOT, ACTION_BOMB, ACTION_BUTTON3, ACTION_START, ACTION_COIN, ACTION_SERVICE,
	ACTIONS
};

/* What the port does itself, from either player's keyboard or gamepad: [hotkeys] */
enum
{
	HOTKEY_RESET,               /* the board's reset */
	HOTKEY_SERVICE_MENU,        /* the reset, with the service switch held for the player: the test mode */
	HOTKEY_FAST_FORWARD,        /* held: the game runs fast_forward_speed frames a frame */
	HOTKEYS
};

#define BINDINGS_MAX 4          /* keys or buttons an action may have */

enum { PAD_NONE, PAD_BUTTON, PAD_TRIGGER };

typedef struct
{
	int kind;                   /* PAD_BUTTON: an SDL_GameControllerButton; PAD_TRIGGER: an SDL_GameControllerAxis */
	int index;
} pad_input;

typedef struct
{
	/* [game] */
	bool free_play;
	bool arcade_timing;
	int fast_forward_speed;     /* 2-16 */
	bool hitbox;                /* the ships' hitboxes shown */
	/* [window] */
	int scale;
	bool fullscreen;
	/* [sound] */
	bool sound;
	int volume;                 /* 0-100 */
	/* [files] */
	char rom[512];
	char eeprom[512];           /* empty: donpachi.eeprom beside the program */
	char cheats[512];
	/* [player1.keyboard] ... [player2.gamepad] */
	SDL_Scancode keys[2][ACTIONS][BINDINGS_MAX];
	pad_input buttons[2][ACTIONS][BINDINGS_MAX];
	/* [hotkeys] */
	SDL_Scancode hot_keys[HOTKEYS][BINDINGS_MAX];
	pad_input hot_buttons[HOTKEYS][BINDINGS_MAX];
	bool left_stick[2];         /* the left stick moves too */
	int stick_dead[2];          /* how far it goes before it counts, in % */
} settings;

extern const uint16_t settings_action_bits[ACTIONS];

/* The defaults: the keys and buttons the port had before the file (the Xbox layout: X shot, A bomb, B button C) */
void settings_defaults(settings *s);

/* The file over 's' (which holds the defaults): NULL, or what was wrong with it (the lines it could not take, which
   keep their defaults). A file that is not there is written with the defaults: *written is then true */
const char *settings_load(settings *s, const char *path, bool *written);

/* All the settings into the file, with what each does: NULL, or what went wrong */
const char *settings_save(const settings *s, const char *path);

/* A pad input pressed on 'pad' (a trigger counts past half way) */
bool settings_pad_pressed(SDL_GameController *pad, pad_input input);

#endif
