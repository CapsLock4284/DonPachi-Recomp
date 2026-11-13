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
	HOTKEY_SAVE_STATE,          /* the game as it is now, kept in memory in the chosen slot (until the program closes) */
	HOTKEY_LOAD_STATE,          /* back to the chosen slot's */
	HOTKEY_SLOT_PREVIOUS,       /* the save states' slot: the one before (STATE_SLOTS of them) */
	HOTKEY_SLOT_NEXT,           /* the one after */
	HOTKEYS
};

#define BINDINGS_MAX 4          /* keys or buttons an action may have */
#define STATE_SLOTS 5           /* the save states kept at once (in memory) */

enum { PAD_NONE, PAD_BUTTON, PAD_TRIGGER };
enum { VIDEO_FIT, VIDEO_INTEGER, VIDEO_STRETCH, VIDEO_SCALINGS };
enum { ROTATE_NONE, ROTATE_RIGHT, ROTATE_LEFT, ROTATIONS };

/* the names of the [video] values, as the file and the menu have them (lower case in the file) */
extern const char *const settings_scaling_names[VIDEO_SCALINGS];
extern const char *const settings_scanline_names[4];
extern const char *const settings_crt_names[3];
extern const char *const settings_rotate_names[ROTATIONS];

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
	bool save_high_scores;      /* the high score table kept in donpachi.hiscore (hiscore.h) */
	bool big_hit;               /* the GP window's large HIT count always on, the best chain shown between chains */
	bool slowdown;              /* the arcade's slowdown: a busy frame takes two (cave/program.h) */
	bool main_menu;             /* the main menu (START GAME, TRAINING, SETTINGS) as the program starts */
	/* [training] (training.h) */
	int training_area;          /* 0-9 */
	int training_point;         /* TRAINING_START, TRAINING_MIDBOSS, TRAINING_BOSS, TRAINING_TRUE_BOSS */
	int training_ship;          /* 0-2: types A, B, C */
	int training_power;         /* 0-3 (levels 1-4), TRAINING_POWER_ON_PACE, TRAINING_POWER_MAX */
	bool training_max_bombs;
	bool training_retry;        /* a miss goes back to the place at once */
	bool training_hitbox;       /* the hitbox shown in a training game (whatever [game] hitbox is) */
	int training_difficulty;    /* the training game's: 0 normal, 1 easy, 2 hard, 3 very hard (the game options' is
	                               put back after) */
	/* [window] */
	int scale;
	bool fullscreen;
	bool exclusive;             /* full screen as exclusive (the display the game's alone); false: borderless */
	/* [video] */
	int scaling;                /* VIDEO_FIT, VIDEO_INTEGER, VIDEO_STRETCH */
	bool smooth;                /* a soft filter instead of sharp pixels */
	int scanlines;              /* 0 off, 1 light, 2 medium, 3 heavy */
	int crt;                    /* the arcade monitor's look (crt.h): 0 off, 1 light, 2 full (its own lines then) */
	bool vsync;
	int rotate;                 /* ROTATE_NONE, ROTATE_RIGHT (90 degrees clockwise), ROTATE_LEFT */
	bool panels;                /* the side panels (widgets.h) beside the picture */
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
