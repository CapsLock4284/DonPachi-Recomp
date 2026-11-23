/**
 *    Name:        menu.c
 *    Description: the window port's options menu (menu.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The font is the game's text layer (atdp's text.u58, rom_set.layer[2]): its 8x8 tiles are ASCII less 0x20
 *    (the test mode's "1P LEVER :SELECTION" is 0x11 0x30 0x00 0x2C 0x25 ...), uppercase only ('>' and '<' are
 *    arrows), pens 1-6 a shade from light to dark and pen 15 the outline. The tiles lie as the board's screen does,
 *    turned for the cabinet: a tile's row is the picture's column.
 */
#include "menu.h"
#include "training.h"
#include "replay.h"
#include "stats.h"
#include "ui_sound.h"
#include "scaler.h"
#include "updater.h"

#include "cave/cheats.h"
#include "own_cheats.h"
#include "cave/machine.h"
#include "achievements.h"

#ifndef DONPACHI_VERSION
#define DONPACHI_VERSION "0.0.0"    /* (CMake passes the project's version) */
#endif
#include "cave/rom_set.h"
#include "cave/video.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define PICTURE_W   VIDEO_HEIGHT        /* 240: the picture as the cabinet shows it */
#define PICTURE_H   VIDEO_WIDTH         /* 320 */
#define COLUMNS     (PICTURE_W / 8)     /* 30 */

#define PANEL_LEFT  1                   /* the panel's columns and rows, in characters */
#define PANEL_RIGHT 28
#define PANEL_TOP   3
#define PANEL_ROWS  34

#define WHITE       0xF0F0F0u
#define GOLD        0xFFC83Cu
#define GREY        0x8C8C9Cu
#define CYAN        0x6CD8FFu
#define RED         0xFF4848u
#define GREEN_TEXT  0x5CE05Cu

#define REPEAT_FIRST 18                 /* frames a direction is held before it repeats, then every REPEAT_NEXT */
#define REPEAT_NEXT  5
#define CAPTURE_FRAMES 345              /* the remapping's wait for a key or button: 6 s */
#define STICK_DEAD   16000

enum { PAGE_MAIN, PAGE_CONTROLS, PAGE_GRAPHICS, PAGE_GAME, PAGE_CHEATS, PAGE_DEBUG, PAGE_FRONT, PAGE_TRAINING,
	PAGE_ACHIEVEMENTS, PAGE_STATS, PAGE_REPLAYS, PAGE_SAVE_REPLAY, PAGE_REPLAY_MENU, PAGE_RENAME, PAGE_DELETE,
	PAGE_UPDATE, PAGE_SOUND, PAGE_ARCADE };
enum { NAV_NONE, NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT, NAV_CHOOSE, NAV_BACK, NAV_CLOSE };

enum
{
	ITEM_RESUME, ITEM_RESET, ITEM_SERVICE, ITEM_FREE_PLAY, ITEM_TIMING, ITEM_FAST_SPEED, ITEM_HITBOX, ITEM_VOLUME,
	ITEM_SCALE, ITEM_FULLSCREEN, ITEM_CONTROLS, ITEM_QUIT,
	ITEM_DEFAULTS, ITEM_BACK,     /* (the controls page's, after its actions) */
	ITEM_GRAPHICS, ITEM_SCALING, ITEM_SMOOTH, ITEM_SCANLINES, ITEM_VSYNC, ITEM_ROTATE,
	ITEM_DEADZONE,                /* the player's left stick: how far before it counts */
	ITEM_PANELS,                  /* the side panels */
	ITEM_HISCORES,                /* the high score table kept in a file */
	ITEM_SAVE_STATE, ITEM_LOAD_STATE,   /* the save state: kept now, or gone back to */
	ITEM_GAME,                    /* the game options page */
	ITEM_CHEATS,                  /* the cheats page (from the game options) */
	ITEM_START_GAME, ITEM_TRAINING, ITEM_SETTINGS,    /* the main menu's (the title screen) */
	ITEM_MAIN_MENU,               /* back to the main menu from the game */
	ITEM_T_AREA, ITEM_T_POINT, ITEM_T_SHIP, ITEM_T_POWER, ITEM_T_RETRY, ITEM_T_START,    /* the training page */
	ITEM_T_BOMBS, ITEM_TRAINING_MENU,   /* (and the options' way back to it, in a training game) */
	ITEM_T_HITBOX,
	ITEM_DIFFICULTY,              /* the game's difficulty: the test mode's, in the EEPROM */
	ITEM_T_DIFFICULTY,            /* the training game's own */
	ITEM_BIG_HIT,                 /* the GP window's large HIT display, always on */
	ITEM_VERSION,                 /* the main menu's version of the game: the US one or the Japanese one */
	ITEM_ACHIEVEMENTS,            /* the achievements page (the main menu's, the game options') */
	ITEM_RETRY_TRAINING,          /* a training game: back to its place, as it began */
	ITEM_STATISTICS,              /* the lifetime statistics page (the main menu's, the game options') */
	ITEM_REPLAYS,                 /* the main menu's replays page */
	ITEM_STOP_REPLAY,             /* a replay playing: back to the replays page */
	ITEM_YES, ITEM_NO,            /* the question as a game ends: save its replay? */
	ITEM_SAVE_REPLAY,             /* a training game's: the attempt kept as a replay */
	ITEM_WATCH, ITEM_RENAME, ITEM_DELETE,    /* a replay chosen on the replays page */
	ITEM_DELETE_YES, ITEM_DELETE_NO,
	ITEM_SLOWDOWN,                /* the arcade's slowdown */
	ITEM_CRT,                     /* the arcade monitor's look (crt.h) */
	ITEM_STATE_SLOT,              /* the save states' slot */
	ITEM_DEBUG_TOOLS,             /* the debug tools page (from the game options): own_cheats.h's debug ones */
	ITEM_FILTER,                  /* the pixel-art scaler over the picture (scaler.h) */
	ITEM_AUTOBOMB,                /* a bomb set off for a ship about to be shot down (autobomb.h) */
	ITEM_RENDERER,                /* how the picture reaches the screen (settings.h): from the next start */
	ITEM_UPDATE, ITEM_UPDATE_YES, ITEM_UPDATE_NO,    /* the main menu's, a newer release found (updater.h) */
	ITEM_CHECK_UPDATES,           /* the game options': the newest release looked up as the program starts */
	ITEM_SOUND,                   /* the sound's page: VOLUME, and MUSIC, EFFECTS and VOICES on their own */
	ITEM_MUSIC_VOLUME, ITEM_EFFECTS_VOLUME, ITEM_VOICES_VOLUME,
	ITEM_BULLETS,                 /* the game options': the enemies' bullets' points shown (hitbox.h) */
	ITEM_FILTER_LAYERS,           /* the filter on each layer and the sprites apart (layer_filter.h) */
	ITEM_FPS,                     /* the FPS meter (main.c; F9) */
	ITEM_C_AUTO,                  /* the game options': button C's rapid fire, the test mode's (the EEPROM's) */
	ITEM_ARCADE,                  /* the game options': the ARCADE SETTINGS page (the test mode's other settings) */
	ITEM_A_CONTINUE, ITEM_A_CONTINUE_COST, ITEM_A_LIVES, ITEM_A_EXTEND, ITEM_A_COIN_A, ITEM_A_COIN_B,
	ITEM_A_DEMO_SOUND,            /* its entries: arcade_settings[item - ITEM_A_CONTINUE] */
	ITEM_FAST_FORWARD_KEY = 90,   /* the hotkeys (both players': [hotkeys]): fast forward, save state, load state */
	ITEM_SAVE_STATE_KEY, ITEM_LOAD_STATE_KEY, ITEM_SLOT_PREVIOUS_KEY, ITEM_SLOT_NEXT_KEY,
	ITEM_ACTION = 100,            /* + an action: a player's key and button for it */
	ITEM_CHEAT = 200,             /* + a cheat of the cheat file (cave/cheats.h) */
	ITEM_ACHIEVEMENT = 400,       /* + an achievement (achievements.h) */
	ITEM_STAT = 500,              /* + a line of the statistics page (stats.h) */
	ITEM_REPLAY = 600             /* + a replay of the replays page's list */
};

static const char *const action_labels[ACTIONS] = {
	"UP", "DOWN", "LEFT", "RIGHT", "SHOT", "BOMB", "AUTO FIRE", "START", "COIN", "SERVICE"
};

static menu_host host;
static bool update_found;               /* a newer release was found: UPDATE TO ... on the main menu */
static int update_listed = -1;          /* the updater's state the page's entries were made for */

static bool is_action(int item)
{
	return item >= ITEM_ACTION && item < ITEM_ACTION + ACTIONS;
}

static bool is_cheat(int item)
{
	return item >= ITEM_CHEAT && item < ITEM_ACHIEVEMENT;
}

static bool is_stat(int item)
{
	return item >= ITEM_STAT && item < ITEM_REPLAY;
}

static bool is_replay(int item)
{
	return item >= ITEM_REPLAY;
}

static int achievements_from = PAGE_FRONT;    /* the page the achievements page was opened from */
static int stats_from = PAGE_FRONT;           /* the page the statistics page was opened from */
#define REPLAYS_LISTED 100
static replay_info replays[REPLAYS_LISTED];    /* the replays page's list, as it was opened */
static int replay_count;
static int after_question = MENU_AFTER_RESUME;    /* what follows the replay question */
static int replay_chosen;                 /* the replays page's: the one chosen (replays[]) */
static char new_name[REPLAY_NAME_MOST + 1];    /* RENAME's name being written, and where in it */
static int name_at;
static unsigned blink;                    /* menu_tick's count: RENAME's cursor blinks with it */

/* the hotkeys' entries on the controls page */
static bool is_hotkey_item(int item)
{
	return item >= ITEM_FAST_FORWARD_KEY && item <= ITEM_SLOT_NEXT_KEY;
}

static int item_hotkey(int item)
{
	return HOTKEY_FAST_FORWARD + (item - ITEM_FAST_FORWARD_KEY);
}
static bool open, just_closed;
static bool front;                      /* the main menu is up (the game not started): the options are its settings */
static int front_message;               /* frames "coming soon" is still shown */
static int training_pending;
/* the test mode's settings, as the game keeps them in the EEPROM: word 4's high byte, its 2 low bits the difficulty
   (0 normal, 1 easy, 2 hard, 3 very hard; read into $101288 at boot, the game's at its start: the rank's base,
   $10121E, and the bullets' speed from it) */
#define DIFFICULTY_WORD 4            /* START TRAINING: the frames until it starts (LOADING... shown meanwhile) */
#define C_AUTO_BIT 0x0004u           /* (word 4's) "C BUTTON FULL-AUTO": read into $101287 bit 2 at boot */

/* The test mode's CONFIGURATION, its other settings (all but C BUTTON, GAME DIFFICULTY and INVERT SCREEN: bit 1,
   the board's screen flip, which the port's video does not do: the ROTATE setting instead): word 4's bits, read at
   boot (the low byte into $101287, the high byte into $101288's low byte), so in use from the next reset. Found by
   changing each in the test mode and reading the EEPROM it saved; the names are the test mode's (the program's text
   at 0239D), made shorter. EXTEND: the first extend from 006C68, the next from 006CA2 (its top bit: again and again
   by that much): 2M; 1M and 6M; 5M, 8M, 11M... */
typedef struct
{
	int shift, values;                 /* the setting's bits: 'values' of them (2 or 4) from bit 'shift' */
	const char *names[4];              /* each value's (the EEPROM's 0, 1, 2, 3) */
	const char *help;
} arcade_setting;

static const arcade_setting arcade_settings[] = {
	{ 15, 2, { "ON", "OFF" }, "WHETHER A GAME OVER CAN BE CONTINUED (THE ARCADE'S CONTINUE ON / OFF)." },
	{ 0, 2, { "NORMAL", "DISCOUNT" }, "DISCOUNT: A CONTINUE COSTS LESS THAN A NEW GAME (THE ARCADE'S DISCOUNT "
		"CONTINUE). IT MATTERS ONLY WITH FREE PLAY OFF." },
	{ 12, 4, { "3", "5", "2", "1" }, "THE SHIPS EACH GAME STARTS WITH (THE ARCADE'S HERO COUNTS)." },
	{ 10, 4, { "2M ONLY", "1M AND 6M", "5M, 8M, 11M...", "NONE" }, "THE SCORES THAT GIVE AN EXTRA SHIP: 2 MILLION "
		"ONLY; 1 AND 6 MILLION; OR 5, 8, 11 MILLION AND SO ON; OR NONE." },
	{ 4, 4, { "1 COIN 1 PLAY", "1 COIN 2 PLAYS", "2 COINS 1 PLAY", "3 COINS 1 PLAY" }, "THE CREDITS A COIN IN THE "
		"FIRST SLOT GIVES. IT MATTERS ONLY WITH FREE PLAY OFF." },
	{ 6, 4, { "1 COIN 1 PLAY", "1 COIN 2 PLAYS", "2 COINS 1 PLAY", "3 COINS 1 PLAY" }, "THE CREDITS A COIN IN THE "
		"SECOND SLOT GIVES (PLAYER 2'S COIN). IT MATTERS ONLY WITH FREE PLAY OFF." },
	{ 3, 2, { "ON", "OFF" }, "THE MUSIC AND SOUND WHILE THE GAME SHOWS ITS DEMO BETWEEN GAMES (THE ARCADE'S "
		"ADVERTISE SOUND)." },
};

static bool is_arcade_setting(int item)
{
	return item >= ITEM_A_CONTINUE && item <= ITEM_A_DEMO_SOUND;
}

static unsigned arcade_value(int item)
{
	const arcade_setting *const a = &arcade_settings[item - ITEM_A_CONTINUE];

	return (unsigned)(cave_board.eeprom.words[DIFFICULTY_WORD] >> a->shift) & (unsigned)(a->values - 1);
}

void menu_open_front(void);
static int page, selected, player;
static int items[128], item_count;
static int capture = -1;                /* the action waiting for its key or button, or -1 */
static int capture_frames;
static int held_direction, held_frames; /* the gamepads' d-pad and stick, for their repeats */

/* ---- the entries ---- */

static void build_items(void)
{
	item_count = 0;
	if (page == PAGE_MAIN)
	{
		static const int main_items[] = { ITEM_RESUME, ITEM_RESET, ITEM_TIMING, ITEM_HITBOX,
			ITEM_SOUND, ITEM_SAVE_STATE, ITEM_LOAD_STATE, ITEM_STATE_SLOT, ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS,
			ITEM_MAIN_MENU,
			ITEM_QUIT };
		/* (the main menu's SETTINGS: what can be set before a game) */
		static const int settings_items[] = { ITEM_TIMING, ITEM_HITBOX, ITEM_SOUND, ITEM_GAME,
			ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_BACK };

		/* (a training game's: its place again and its page at the top, no RESET or save states: the training
		   is its own save state, F5 / F8 still there) */
		static const int training_game_items[] = { ITEM_RESUME, ITEM_RETRY_TRAINING, ITEM_SAVE_REPLAY, ITEM_TRAINING_MENU,
			ITEM_TIMING,
			ITEM_HITBOX, ITEM_SOUND, ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_MAIN_MENU, ITEM_QUIT };

		if (front)
			for (size_t index = 0; index < sizeof(settings_items) / sizeof(settings_items[0]); index++)
				items[item_count++] = settings_items[index];
		else if (host.in_replay && host.in_replay())
		{
			static const int replay_items[] = { ITEM_RESUME, ITEM_STOP_REPLAY, ITEM_TIMING, ITEM_HITBOX, ITEM_SOUND,
				ITEM_GRAPHICS, ITEM_MAIN_MENU, ITEM_QUIT };

			for (size_t index = 0; index < sizeof(replay_items) / sizeof(replay_items[0]); index++)
				items[item_count++] = replay_items[index];
		}
		else if (host.in_training && host.in_training())
			for (size_t index = 0; index < sizeof(training_game_items) / sizeof(training_game_items[0]); index++)
				items[item_count++] = training_game_items[index];
		else
			for (size_t index = 0; index < sizeof(main_items) / sizeof(main_items[0]); index++)
				items[item_count++] = main_items[index];
	}
	else if (page == PAGE_ARCADE)
	{
		for (int item = ITEM_A_CONTINUE; item <= ITEM_A_DEMO_SOUND; item++)
			items[item_count++] = item;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_SOUND)
	{
		static const int sound_items[] = { ITEM_VOLUME, ITEM_MUSIC_VOLUME, ITEM_EFFECTS_VOLUME, ITEM_VOICES_VOLUME,
			ITEM_BACK };

		for (size_t index = 0; index < sizeof(sound_items) / sizeof(sound_items[0]); index++)
			items[item_count++] = sound_items[index];
	}
	else if (page == PAGE_TRAINING)
	{
		static const int training_items[] = { ITEM_T_AREA, ITEM_T_POINT, ITEM_T_SHIP, ITEM_T_DIFFICULTY, ITEM_T_POWER,
			ITEM_T_BOMBS, ITEM_T_RETRY, ITEM_T_HITBOX, ITEM_T_START, ITEM_BACK };

		for (size_t index = 0; index < sizeof(training_items) / sizeof(training_items[0]); index++)
			items[item_count++] = training_items[index];
	}
	else if (page == PAGE_FRONT)
	{
		static const int front_items[] = { ITEM_VERSION, ITEM_START_GAME, ITEM_TRAINING, ITEM_REPLAYS,
			ITEM_ACHIEVEMENTS, ITEM_STATISTICS, ITEM_SETTINGS, ITEM_QUIT };

		if (update_found)
			items[item_count++] = ITEM_UPDATE;    /* (first: seen at once) */
		for (size_t index = 0; index < sizeof(front_items) / sizeof(front_items[0]); index++)
			items[item_count++] = front_items[index];
		update_listed = update_found;
	}
	else if (page == PAGE_UPDATE)
	{
		/* (asked; while it downloads and installs, only BACK: the download goes on) */
		const int state = updater_state();

		if (state == UPDATER_AVAILABLE || state == UPDATER_FAILED)
		{
			items[item_count++] = ITEM_UPDATE_YES;
			items[item_count++] = ITEM_UPDATE_NO;
		}
		else
			items[item_count++] = ITEM_BACK;
		update_listed = state;
	}
	else if (page == PAGE_GAME)
	{
		static const int game_items[] = { ITEM_DIFFICULTY, ITEM_SLOWDOWN, ITEM_AUTOBOMB, ITEM_C_AUTO, ITEM_FAST_SPEED, ITEM_BIG_HIT,
			ITEM_BULLETS, ITEM_ARCADE, ITEM_FREE_PLAY, ITEM_HISCORES, ITEM_CHECK_UPDATES, ITEM_CHEATS, ITEM_DEBUG_TOOLS, ITEM_ACHIEVEMENTS, ITEM_STATISTICS, ITEM_BACK };

		for (size_t index = 0; index < sizeof(game_items) / sizeof(game_items[0]); index++)
			items[item_count++] = game_items[index];
	}
	else if (page == PAGE_ACHIEVEMENTS)
	{
		for (int index = 0; index < achievements_count(); index++)
			items[item_count++] = ITEM_ACHIEVEMENT + index;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_SAVE_REPLAY)
	{
		items[item_count++] = ITEM_YES;
		items[item_count++] = ITEM_NO;
	}
	else if (page == PAGE_REPLAY_MENU)
	{
		items[item_count++] = ITEM_WATCH;
		items[item_count++] = ITEM_RENAME;
		items[item_count++] = ITEM_DELETE;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_DELETE)
	{
		items[item_count++] = ITEM_DELETE_NO;
		items[item_count++] = ITEM_DELETE_YES;
	}
	else if (page == PAGE_REPLAYS)
	{
		for (int index = 0; index < replay_count; index++)
			items[item_count++] = ITEM_REPLAY + index;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_STATS)
	{
		for (int line = 0; line < stats_lines() && item_count < 120; line++)
			items[item_count++] = ITEM_STAT + line;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_CHEATS || page == PAGE_DEBUG)
	{
		/* the port's own, then the cheat file's that the game can have (they write its RAM); the debug tools on
		   their own page */
		for (int cheat = 0; cheat < cheats_count() && item_count < 40; cheat++)
			if (cheats_usable(cheat) && (page == PAGE_DEBUG) == (cheats_own(cheat)
					&& !strncmp(cheats_name(cheat), OWN_CHEATS_DEBUG, strlen(OWN_CHEATS_DEBUG))))
				items[item_count++] = ITEM_CHEAT + cheat;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_GRAPHICS)
	{
		static const int graphics_items[] = { ITEM_SCALING, ITEM_SMOOTH, ITEM_FILTER, ITEM_FILTER_LAYERS, ITEM_SCANLINES, ITEM_CRT, ITEM_VSYNC, ITEM_RENDERER, ITEM_FPS, ITEM_ROTATE,
			ITEM_PANELS, ITEM_SCALE, ITEM_FULLSCREEN, ITEM_BACK };

		for (size_t index = 0; index < sizeof(graphics_items) / sizeof(graphics_items[0]); index++)
			items[item_count++] = graphics_items[index];
	}
	else
	{
		for (int action = 0; action < ACTIONS; action++)
			items[item_count++] = ITEM_ACTION + action;
		items[item_count++] = ITEM_FAST_FORWARD_KEY;
		items[item_count++] = ITEM_SAVE_STATE_KEY;
		items[item_count++] = ITEM_LOAD_STATE_KEY;
		items[item_count++] = ITEM_SLOT_PREVIOUS_KEY;
		items[item_count++] = ITEM_SLOT_NEXT_KEY;
		items[item_count++] = ITEM_DEADZONE;
		items[item_count++] = ITEM_DEFAULTS;
		items[item_count++] = ITEM_BACK;
	}
	if (selected >= item_count)
		selected = item_count - 1;
}

static void save(void)
{
	const char *const problem = settings_save(host.file, host.path);

	if (problem)
		fprintf(stderr, "%s: %s\n", host.path, problem);
}

static void close_menu(void)
{
	ui_sound_preview_stop();
	open = false;
	front = false;    /* (a game goes on: START GAME, or the service menu's reset) */
	just_closed = true;
	capture = -1;
}

/* the key or button names as the menu shows them */
static const char *pad_label(pad_input input)
{
	static const struct { int button; const char *label; } labels[] = {
		{ SDL_CONTROLLER_BUTTON_A, "A" }, { SDL_CONTROLLER_BUTTON_B, "B" }, { SDL_CONTROLLER_BUTTON_X, "X" },
		{ SDL_CONTROLLER_BUTTON_Y, "Y" }, { SDL_CONTROLLER_BUTTON_BACK, "VIEW" },
		{ SDL_CONTROLLER_BUTTON_GUIDE, "XBOX" }, { SDL_CONTROLLER_BUTTON_START, "START" },
		{ SDL_CONTROLLER_BUTTON_LEFTSTICK, "LS" }, { SDL_CONTROLLER_BUTTON_RIGHTSTICK, "RS" },
		{ SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "LB" }, { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "RB" },
		{ SDL_CONTROLLER_BUTTON_DPAD_UP, "D-UP" }, { SDL_CONTROLLER_BUTTON_DPAD_DOWN, "D-DOWN" },
		{ SDL_CONTROLLER_BUTTON_DPAD_LEFT, "D-LEFT" }, { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "D-RIGHT" },
		{ SDL_CONTROLLER_BUTTON_MISC1, "SHARE" },
	};

	if (input.kind == PAD_TRIGGER)
		return input.index == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? "LT" : "RT";
	if (input.kind != PAD_BUTTON)
		return "-";
	for (size_t index = 0; index < sizeof(labels) / sizeof(labels[0]); index++)
		if (labels[index].button == input.index)
			return labels[index].label;
	return "PAD";
}

static void key_label(SDL_Scancode key, char *out, size_t size)
{
	const char *const name = key == SDL_SCANCODE_UNKNOWN ? "-" : SDL_GetScancodeName(key);

	snprintf(out, size, "%s", *name ? name : "?");
	for (char *letter = out; *letter; letter++)
		*letter = (char)toupper((unsigned char)*letter);
}

/* ---- changing the settings (in use and the file's) ---- */

/* a choice of 'count' moved by 'step', stopping at the ends, or round to the start (wrap) */
static int cycle(int value, int count, int step, bool wrap)
{
	value += step;
	if (value >= count)
		return wrap ? 0 : count - 1;
	if (value < 0)
		return wrap ? count - 1 : 0;
	return value;
}

/* a setting changed: by 'step' (left -1, right +1, stopping at the ends), or by A (wrap: round to the start) */
static void change(int item, int step, bool wrap)
{
	settings *const in_use = host.options, *const file = host.file;

	if (is_cheat(item))
	{
		cheats_set(item - ITEM_CHEAT, !cheats_on(item - ITEM_CHEAT));    /* (for this run: not saved) */
		return;
	}
	switch (item)
	{
	case ITEM_VERSION:
		/* the other version, from its reset (main.c: its data, EEPROM and high scores) */
		if (host.version && host.set_version)
			host.set_version(!host.version());
		return;
	case ITEM_DIFFICULTY:
	{
		/* in the order the menu goes through them: easy, normal, hard, very hard (the EEPROM's 1, 0, 2, 3) */
		static const unsigned order[4] = { 1u, 0u, 2u, 3u };
		const unsigned now = cave_board.eeprom.words[DIFFICULTY_WORD] >> 8 & 3u;
		int at = 0;

		while (order[at] != now)
			at++;
		at = (at + step + 4) % 4;
		cave_board.eeprom.words[DIFFICULTY_WORD] =
			(uint16_t)((cave_board.eeprom.words[DIFFICULTY_WORD] & ~0x0300u) | order[at] << 8);
		return;    /* (the EEPROM: the port keeps it in its file) */
	}
	case ITEM_T_AREA:
		in_use->training_area = (in_use->training_area + step + 10) % 10;
		if (in_use->training_area != 9 && in_use->training_point == TRAINING_TRUE_BOSS)
			in_use->training_point = TRAINING_BOSS;    /* (area 10's alone) */
		file->training_area = in_use->training_area;
		file->training_point = in_use->training_point;
		break;
	case ITEM_T_POINT:
	{
		/* (in the order of the area: its start, the points part way, the mid-boss, the boss, area 10's true boss) */
		const int parts = TRAINING_PERCENT_MOST / TRAINING_PERCENT_STEP;
		const int count = 1 + parts + (in_use->training_area == 9 ? 3 : 2);
		int at = in_use->training_point == TRAINING_PART_WAY
			? SDL_clamp(in_use->training_percent / TRAINING_PERCENT_STEP, 1, parts)
			: in_use->training_point == TRAINING_START ? 0 : parts + in_use->training_point;

		at = (at + step + count) % count;
		if (at == 0)
			in_use->training_point = TRAINING_START;
		else if (at <= parts)
		{
			in_use->training_point = TRAINING_PART_WAY;
			in_use->training_percent = file->training_percent = at * TRAINING_PERCENT_STEP;
		}
		else
			in_use->training_point = at - parts;
		file->training_point = in_use->training_point;
		break;
	}
	case ITEM_T_SHIP:
		in_use->training_ship = file->training_ship = (in_use->training_ship + step + 3) % 3;
		break;
	case ITEM_T_POWER:
		in_use->training_power = file->training_power = (in_use->training_power + step + TRAINING_POWERS) % TRAINING_POWERS;
		break;
	case ITEM_T_BOMBS:
		in_use->training_max_bombs = file->training_max_bombs = !in_use->training_max_bombs;
		break;
	case ITEM_T_HITBOX:
		in_use->training_hitbox = file->training_hitbox = !in_use->training_hitbox;
		break;
	case ITEM_T_DIFFICULTY:
	{
		static const int order[4] = { 1, 0, 2, 3 };    /* easy, normal, hard, very hard */
		int at = 0;

		while (order[at] != (in_use->training_difficulty & 3))
			at++;
		at = wrap ? (at + 1) % 4 : SDL_clamp(at + step, 0, 3);
		in_use->training_difficulty = file->training_difficulty = order[at];
		break;
	}
	case ITEM_T_RETRY:
		in_use->training_retry = file->training_retry = !in_use->training_retry;
		break;
	case ITEM_FREE_PLAY:
		in_use->free_play = file->free_play = !in_use->free_play;
		break;
	case ITEM_HITBOX:
		in_use->hitbox = file->hitbox = !in_use->hitbox;
		break;
	case ITEM_BULLETS:
		in_use->bullets = file->bullets = !in_use->bullets;
		break;
	case ITEM_HISCORES:
		in_use->save_high_scores = file->save_high_scores = !in_use->save_high_scores;
		break;
	case ITEM_BIG_HIT:
		in_use->big_hit = file->big_hit = !in_use->big_hit;
		break;
	case ITEM_CHECK_UPDATES:
		in_use->check_updates = file->check_updates = !in_use->check_updates;    /* (from the next start) */
		break;
	case ITEM_AUTOBOMB:
		in_use->autobomb = file->autobomb = !in_use->autobomb;
		break;
	case ITEM_A_CONTINUE: case ITEM_A_CONTINUE_COST: case ITEM_A_LIVES: case ITEM_A_EXTEND: case ITEM_A_COIN_A:
	case ITEM_A_COIN_B: case ITEM_A_DEMO_SOUND:
	{
		/* (LIVES in the menu's order, 1 2 3 5: the EEPROM's 3, 2, 0, 1; the others as the test mode has them) */
		static const unsigned lives_order[4] = { 3u, 2u, 0u, 1u };
		const arcade_setting *const a = &arcade_settings[item - ITEM_A_CONTINUE];
		const unsigned now = arcade_value(item), mask = (unsigned)(a->values - 1) << a->shift;
		unsigned next;

		if (item == ITEM_A_LIVES)
		{
			int at = 0;

			while (lives_order[at] != now)
				at++;
			at = wrap ? (at + 1) % 4 : SDL_clamp(at + step, 0, 3);
			next = lives_order[at];
		}
		else if (wrap || a->values == 2)
			next = (now + (unsigned)(step + a->values)) % (unsigned)a->values;
		else
			next = (unsigned)SDL_clamp((int)now + step, 0, a->values - 1);
		cave_board.eeprom.words[DIFFICULTY_WORD] =
			(uint16_t)((cave_board.eeprom.words[DIFFICULTY_WORD] & ~mask) | next << a->shift);
		return;    /* (the EEPROM: the port keeps it in its file; the game reads it at its next reset) */
	}
	case ITEM_C_AUTO:
		if (host.set_c_auto)
			host.set_c_auto(!(cave_board.eeprom.words[DIFFICULTY_WORD] & C_AUTO_BIT));
		break;
	case ITEM_SLOWDOWN:
		in_use->slowdown = file->slowdown = !in_use->slowdown;    /* (main.c puts it in use: game_program.c) */
		break;
	case ITEM_TIMING:
		in_use->arcade_timing = file->arcade_timing = !in_use->arcade_timing;
		host.apply_timing();
		break;
	case ITEM_FAST_SPEED:
	{
		/* the speeds the menu offers (the file may hold any of 2-16: the next one up or down from it) */
		static const int speeds[] = { 2, 3, 4, 8, 16 };
		const int count = (int)(sizeof(speeds) / sizeof(speeds[0]));
		int at = 0;

		while (at < count - 1 && speeds[at] < in_use->fast_forward_speed)
			at++;
		if (step > 0 && speeds[at] <= in_use->fast_forward_speed)
			at = at == count - 1 ? (wrap ? 0 : at) : at + 1;
		else if (step < 0)
			at = at > 0 ? at - 1 : 0;
		in_use->fast_forward_speed = file->fast_forward_speed = speeds[at];
		break;
	}
	case ITEM_VOLUME:
		in_use->volume = wrap && in_use->volume >= 100 ? 0 : SDL_clamp(in_use->volume + step * 5, 0, 100);
		file->volume = in_use->volume;
		ui_sound_preview(UI_PREVIEW_MUSIC);    /* (heard as it changes: the music, at the volume being set) */
		break;
	case ITEM_MUSIC_VOLUME:
	case ITEM_EFFECTS_VOLUME:
	case ITEM_VOICES_VOLUME:
	{
		/* (applied by main.c each frame: cave_sound_set_level) */
		int *const now = item == ITEM_MUSIC_VOLUME ? &in_use->music_volume
			: item == ITEM_EFFECTS_VOLUME ? &in_use->effects_volume : &in_use->voices_volume;
		int *const kept = item == ITEM_MUSIC_VOLUME ? &file->music_volume
			: item == ITEM_EFFECTS_VOLUME ? &file->effects_volume : &file->voices_volume;

		*now = wrap && *now >= 100 ? 0 : SDL_clamp(*now + step * 5, 0, 100);
		*kept = *now;
		/* (heard as it changes: an example of the group, at its level) */
		ui_sound_preview(item == ITEM_MUSIC_VOLUME ? UI_PREVIEW_MUSIC : item == ITEM_EFFECTS_VOLUME
			? UI_PREVIEW_EFFECTS : UI_PREVIEW_VOICES);
		break;
	}
	case ITEM_SCALE:
		in_use->scale = wrap && in_use->scale >= 8 ? 1 : SDL_clamp(in_use->scale + step, 1, 8);
		file->scale = in_use->scale;
		host.apply_window();
		break;
	case ITEM_FULLSCREEN:
	{
		/* OFF, BORDERLESS, EXCLUSIVE */
		int at = !in_use->fullscreen ? 0 : in_use->exclusive ? 2 : 1;

		at = wrap ? (at + 1) % 3 : SDL_clamp(at + step, 0, 2);
		in_use->fullscreen = file->fullscreen = at != 0;
		if (at != 0)
			in_use->exclusive = file->exclusive = at == 2;
		host.apply_window();
	}
		break;
	case ITEM_PANELS:
		in_use->panels = file->panels = !in_use->panels;
		host.apply_window();    /* (the window widens for them) */
		break;
	case ITEM_DEADZONE:
		in_use->stick_dead[player] = file->stick_dead[player] =
			wrap && in_use->stick_dead[player] >= 95 ? 5 : SDL_clamp(in_use->stick_dead[player] + step * 5, 5, 95);
		break;
	case ITEM_SCALING:
		in_use->scaling = file->scaling = cycle(in_use->scaling, VIDEO_SCALINGS, step, wrap);
		break;
	case ITEM_SMOOTH:
		in_use->smooth = file->smooth = !in_use->smooth;
		host.apply_video();
		break;
	case ITEM_FILTER:
		in_use->filter = file->filter = cycle(in_use->filter, SCALERS, step, wrap);
		host.apply_video();
		break;
	case ITEM_FILTER_LAYERS:
		in_use->filter_layers = file->filter_layers = !in_use->filter_layers;
		break;
	case ITEM_SCANLINES:
		in_use->scanlines = file->scanlines = cycle(in_use->scanlines, 4, step, wrap);
		break;
	case ITEM_CRT:
		in_use->crt = file->crt = cycle(in_use->crt, 3, step, wrap);
		break;
	case ITEM_STATE_SLOT:
		if (host.state_slot)
			host.state_slot(step);
		break;
	case ITEM_VSYNC:
		in_use->vsync = file->vsync = !in_use->vsync;
		host.apply_video();
		break;
	case ITEM_FPS:
		in_use->show_fps = file->show_fps = !in_use->show_fps;
		break;
	case ITEM_RENDERER:
		in_use->renderer = file->renderer = cycle(in_use->renderer, RENDERERS, step, wrap);
		menu_notice("USED FROM THE NEXT START");
		break;
	case ITEM_ROTATE:
		in_use->rotate = file->rotate = cycle(in_use->rotate, ROTATIONS, step, wrap);
		host.apply_window();    /* (the window's shape turns with it) */
		break;
	default:
		return;
	}
	save();
}

/* the replays page again, its list read again, on the replay 'at' (-1: the first) */
static void replays_page(int at)
{
	replay_count = replay_list(replays, REPLAYS_LISTED);
	page = PAGE_REPLAYS;
	build_items();
	selected = at >= 0 && at < item_count ? at : 0;
}

static void choose(int item)
{
	switch (item)
	{
	case ITEM_RESUME:
		close_menu();
		break;
	case ITEM_START_GAME:
		front = false;
		close_menu();    /* (the game starts: it runs from its reset, as the board switched on) */
		break;
	case ITEM_TRAINING:
		page = PAGE_TRAINING;
		selected = 0;
		build_items();
		break;
	case ITEM_T_START:
		front_message = 0;
		training_pending = 2;    /* (LOADING... shown first: menu_draw starts it, a frame later) */
		break;
	case ITEM_SETTINGS:
		page = PAGE_MAIN;
		selected = 0;
		build_items();
		break;
	case ITEM_MAIN_MENU:
		if (!front && host.replay_in_game && host.replay_in_game())
		{
			/* (a game left part way: its replay kept or not, first) */
			host.hold_replay();
			menu_ask_save_replay(true);
			break;
		}
		host.main_menu();
		menu_open_front();
		break;
	case ITEM_YES:
	case ITEM_NO:
		host.keep_replay(item == ITEM_YES);
		if (after_question == MENU_AFTER_MAIN_MENU)
		{
			host.main_menu();
			menu_open_front();
		}
		else if (after_question == MENU_AFTER_TRAINING_PAGE)
		{
			host.main_menu();
			menu_open_training();
		}
		else
			close_menu();
		break;
	case ITEM_SAVE_REPLAY:
		host.save_training_replay();
		close_menu();
		break;
	case ITEM_WATCH:
		if (host.play_replay(replays[replay_chosen].file))
		{
			front = false;
			close_menu();
		}
		break;
	case ITEM_RENAME:
		snprintf(new_name, sizeof(new_name), "%s", replays[replay_chosen].name);
		name_at = (int)strlen(new_name);
		page = PAGE_RENAME;
		selected = 0;
		build_items();
		break;
	case ITEM_DELETE:
		page = PAGE_DELETE;
		selected = 0;
		build_items();
		break;
	case ITEM_DELETE_YES:
	case ITEM_DELETE_NO:
		if (item == ITEM_DELETE_YES)
			menu_notice(replay_delete(replays[replay_chosen].file) ? "REPLAY DELETED" : "COULD NOT DELETE IT");
		replays_page(item == ITEM_DELETE_YES ? -1 : replay_chosen);
		break;
	case ITEM_TRAINING_MENU:
		host.main_menu();
		menu_open_training();
		break;
	case ITEM_RETRY_TRAINING:
		host.retry_training();
		close_menu();
		break;
	case ITEM_RESET:
	case ITEM_SERVICE:
		host.reset(item == ITEM_SERVICE);
		close_menu();
		break;
	case ITEM_CONTROLS:
		page = PAGE_CONTROLS;
		selected = 0;
		build_items();
		break;
	case ITEM_GRAPHICS:
		page = PAGE_GRAPHICS;
		selected = 0;
		build_items();
		break;
	case ITEM_ARCADE:
		page = PAGE_ARCADE;
		selected = 0;
		build_items();
		break;
	case ITEM_GAME:
		page = PAGE_GAME;
		selected = 0;
		build_items();
		break;
	case ITEM_SOUND:
		page = PAGE_SOUND;
		selected = 0;
		build_items();
		break;
	case ITEM_CHEATS:
		page = PAGE_CHEATS;
		selected = 0;
		build_items();
		break;
	case ITEM_DEBUG_TOOLS:
		page = PAGE_DEBUG;
		selected = 0;
		build_items();
		break;
	case ITEM_ACHIEVEMENTS:
		achievements_from = page;
		page = PAGE_ACHIEVEMENTS;
		selected = 0;
		build_items();
		break;
	case ITEM_REPLAYS:
		replay_count = replay_list(replays, REPLAYS_LISTED);
		page = PAGE_REPLAYS;
		selected = 0;
		build_items();
		break;
	case ITEM_STOP_REPLAY:
		host.main_menu();
		menu_open_replays();
		break;
	case ITEM_STATISTICS:
		stats_from = page;
		page = PAGE_STATS;
		selected = 0;
		build_items();
		break;
	case ITEM_QUIT:
		host.quit();
		close_menu();
		break;
	case ITEM_SAVE_STATE:
	case ITEM_LOAD_STATE:
		close_menu();
		host.state(item == ITEM_SAVE_STATE);
		break;
	case ITEM_DEFAULTS:
	{
		settings defaults;

		settings_defaults(&defaults);
		memcpy(host.options->keys[player], defaults.keys[player], sizeof(defaults.keys[player]));
		memcpy(host.options->buttons[player], defaults.buttons[player], sizeof(defaults.buttons[player]));
		memcpy(host.file->keys[player], defaults.keys[player], sizeof(defaults.keys[player]));
		memcpy(host.file->buttons[player], defaults.buttons[player], sizeof(defaults.buttons[player]));
		host.options->stick_dead[player] = host.file->stick_dead[player] = defaults.stick_dead[player];
		host.options->left_stick[player] = host.file->left_stick[player] = defaults.left_stick[player];
		for (settings *s = host.options; s; s = s == host.options ? host.file : NULL)
		{
			for (int hotkey = HOTKEY_FAST_FORWARD; hotkey <= HOTKEY_SLOT_NEXT; hotkey++)
			{
				memcpy(s->hot_keys[hotkey], defaults.hot_keys[hotkey], sizeof(defaults.hot_keys[hotkey]));
				memcpy(s->hot_buttons[hotkey], defaults.hot_buttons[hotkey], sizeof(defaults.hot_buttons[hotkey]));
			}
		}
		save();
		break;
	}
	case ITEM_UPDATE:
		page = PAGE_UPDATE;
		selected = 0;
		build_items();
		break;
	case ITEM_UPDATE_YES:
		updater_download();    /* (on its thread: the page shows how far it is) */
		build_items();
		selected = 0;
		break;
	case ITEM_UPDATE_NO:
	case ITEM_BACK:
		if (page == PAGE_UPDATE)
		{
			/* (back to the main menu, on UPDATE) */
			page = PAGE_FRONT;
			build_items();
			for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_UPDATE; selected++)
				;
			break;
		}
		if (page == PAGE_REPLAY_MENU)
		{
			replays_page(replay_chosen);
			break;
		}
		if (page == PAGE_REPLAYS)
		{
			page = PAGE_FRONT;
			build_items();
			for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_REPLAYS; selected++)
				;
			break;
		}
		if (page == PAGE_STATS)
		{
			page = stats_from;
			build_items();
			for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_STATISTICS; selected++)
				;
			break;
		}
		if (page == PAGE_ACHIEVEMENTS)
		{
			page = achievements_from;
			build_items();
			for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_ACHIEVEMENTS; selected++)
				;
			break;
		}
		if ((page == PAGE_MAIN || page == PAGE_TRAINING) && front)
		{
			const int opened_by = page == PAGE_TRAINING ? ITEM_TRAINING : ITEM_SETTINGS;

			page = PAGE_FRONT;
			build_items();
			for (selected = 0; selected < item_count - 1 && items[selected] != opened_by; selected++)
				;
			break;
		}
		{
		const int opened_by = page == PAGE_GRAPHICS ? ITEM_GRAPHICS : page == PAGE_GAME ? ITEM_GAME
			: page == PAGE_SOUND ? ITEM_SOUND : page == PAGE_CHEATS ? ITEM_CHEATS : page == PAGE_DEBUG ? ITEM_DEBUG_TOOLS
			: page == PAGE_ARCADE ? ITEM_ARCADE : ITEM_CONTROLS;

		page = page == PAGE_CHEATS || page == PAGE_DEBUG || page == PAGE_ARCADE ? PAGE_GAME : PAGE_MAIN;
		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != opened_by; selected++)
			;    /* (back on the entry that opened the page) */
		break;
	}
	default:
		if (is_stat(item))
			break;    /* (a line to read) */
		if (is_replay(item))
		{
			replay_chosen = item - ITEM_REPLAY;
			page = PAGE_REPLAY_MENU;
			selected = 0;
			build_items();
			break;
		}
		if (is_action(item) || is_hotkey_item(item))
		{
			capture = item;
			capture_frames = CAPTURE_FRAMES;
		}
		else
			change(item, 1, true);
		break;
	}
}

/* RENAME: the letters a name can have (up and down go through them) */
static const char name_letters[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";

static void name_insert(char letter)
{
	const int length = (int)strlen(new_name);

	if (length >= REPLAY_NAME_MOST || !strchr(name_letters, letter) || !letter)
		return;
	memmove(new_name + name_at + 1, new_name + name_at, (size_t)(length - name_at + 1));
	new_name[name_at++] = letter;
}

static void name_delete(void)
{
	if (name_at == 0)
		return;
	memmove(new_name + name_at - 1, new_name + name_at, strlen(new_name) - (size_t)name_at + 1u);
	name_at--;
}

static void name_done(bool keep)
{
	if (keep)
		menu_notice(replay_rename(replays[replay_chosen].file, new_name) ? "REPLAY RENAMED" : "COULD NOT RENAME IT");
	if (keep)
		replays_page(replay_chosen);
	else
	{
		page = PAGE_REPLAY_MENU;
		build_items();
		selected = 1;    /* (on RENAME) */
	}
}

static void rename_navigate(int nav)
{
	const int length = (int)strlen(new_name);

	switch (nav)
	{
	case NAV_UP:
	case NAV_DOWN:
	{
		/* the letter at the cursor, one on (a new one at the end) */
		const char *const at = name_at < length ? strchr(name_letters, new_name[name_at]) : NULL;
		const int count = (int)sizeof(name_letters) - 1, index = at ? (int)(at - name_letters) : 0;

		if (name_at >= length)
		{
			if (length >= REPLAY_NAME_MOST)
				break;
			new_name[length] = 'A';
			new_name[length + 1] = '\0';
			break;
		}
		new_name[name_at] = name_letters[(index + (nav == NAV_UP ? 1 : count - 1)) % count];
		break;
	}
	case NAV_LEFT:
		if (name_at > 0)
			name_at--;
		break;
	case NAV_RIGHT:
		if (name_at < length && name_at < REPLAY_NAME_MOST - 1)
			name_at++;
		break;
	case NAV_CHOOSE:
		name_done(true);
		break;
	case NAV_BACK:
	case NAV_CLOSE:
		name_done(false);
		break;
	}
	ui_sound_play(nav == NAV_CHOOSE ? UI_SOUND_CHOOSE : nav == NAV_BACK || nav == NAV_CLOSE ? UI_SOUND_BACK
		: UI_SOUND_MOVE);
}

static void navigate(int nav)
{
	const int item = items[selected];

	if (page == PAGE_RENAME)
	{
		rename_navigate(nav);
		return;
	}
	if (page == PAGE_DELETE && (nav == NAV_BACK || nav == NAV_CLOSE))
	{
		ui_sound_play(UI_SOUND_BACK);
		choose(ITEM_DELETE_NO);
		return;
	}
	if (page == PAGE_REPLAY_MENU && nav == NAV_CLOSE)
	{
		ui_sound_play(UI_SOUND_BACK);
		choose(ITEM_BACK);
		return;
	}

	switch (nav)
	{
	case NAV_UP:
		selected = (selected + item_count - 1) % item_count;
		ui_sound_play(UI_SOUND_MOVE);
		break;
	case NAV_DOWN:
		selected = (selected + 1) % item_count;
		ui_sound_play(UI_SOUND_MOVE);
		break;
	case NAV_LEFT:
	case NAV_RIGHT:
		if (page == PAGE_CONTROLS && item != ITEM_DEADZONE)
			player ^= 1;
		else
			change(item, nav == NAV_RIGHT ? 1 : -1, false);
		if (page != PAGE_SOUND)
			ui_sound_play(UI_SOUND_MOVE);    /* (the SOUND page: its example is heard instead) */
		break;
	case NAV_CHOOSE:
		ui_sound_play(item == ITEM_BACK || item == ITEM_RESUME ? UI_SOUND_BACK : UI_SOUND_CHOOSE);
		choose(item);
		break;
	case NAV_BACK:
		if (page == PAGE_FRONT)
			break;    /* (the main menu stays until a game starts) */
		if (page == PAGE_SAVE_REPLAY)
		{
			ui_sound_play(UI_SOUND_BACK);
			choose(ITEM_NO);
			break;
		}
		ui_sound_play(UI_SOUND_BACK);
		if (page != PAGE_MAIN || front)
			choose(ITEM_BACK);
		else
			close_menu();
		break;
	case NAV_CLOSE:
		if (page == PAGE_SAVE_REPLAY)
		{
			ui_sound_play(UI_SOUND_BACK);
			choose(ITEM_NO);
			break;
		}
		if (front)
		{
			if (page != PAGE_FRONT)
			{
				ui_sound_play(UI_SOUND_BACK);
				menu_open_front();
			}
			break;
		}
		ui_sound_play(UI_SOUND_BACK);
		close_menu();
		break;
	}
}

static const char *item_label(int item);

/* ---- the tooltips: what the chosen entry does, on the right side panel while the menu is up ---- */

static const char *item_help(int item)
{
	if (is_action(item))
		return "THE KEYS AND GAMEPAD BUTTONS FOR THIS. A: SET A NEW ONE (PRESS IT), LEFT / RIGHT: THE OTHER PLAYER.";
	if (is_cheat(item))
	{
		/* an own cheat: what it does; then, as for the file's, what a cheat costs */
		static char text[512];

		if (page == PAGE_DEBUG)
			snprintf(text, sizeof(text), "%s COUNTS AS A CHEAT.", cheats_help(item - ITEM_CHEAT));
		else
			snprintf(text, sizeof(text), "%s%sON FOR THIS RUN ONLY. WHILE ANY CHEAT IS ON: NO HIGH SCORES, "
				"ACHIEVEMENTS, STATISTICS OR REPLAYS.", cheats_own(item - ITEM_CHEAT) ? cheats_help(item - ITEM_CHEAT)
				: "A CHEAT FROM MAME'S CHEAT FILE.", " ");
		return text;
	}
	if (item >= ITEM_ACHIEVEMENT && item < ITEM_STAT)
		return "EARNED IN ARCADE GAMES ONLY: NOT IN TRAINING, OR A GAME WITH A CHEAT OR A LOADED SAVE STATE.";
	if (is_stat(item))
		return "TOTALS FROM EVERY ARCADE GAME YOU HAVE PLAYED. KEPT IN DONPACHI.STATS BESIDE THE GAME.";
	if (is_replay(item))
		return "A SAVED REPLAY. A: WATCH, RENAME OR DELETE IT. T IS A TRAINING RUN, A THE AREA REACHED.";
	if (is_arcade_setting(item))
		return arcade_settings[item - ITEM_A_CONTINUE].help;
	switch (item)
	{
	case ITEM_RESUME:
		return "BACK TO THE GAME.";
	case ITEM_RESET:
		return "RESTART THE ARCADE BOARD, AS IF SWITCHED OFF AND ON (F3).";
	case ITEM_SERVICE:
		return "THE ARCADE'S OWN TEST MENU: ITS SETTINGS, SOUND TEST AND CHECKS (F4). THE BOARD RESTARTS INTO IT.";
	case ITEM_FREE_PLAY:
		return "START PUTS IN THE CREDITS FOR YOU: NO COIN NEEDED.";
	case ITEM_TIMING:
		return "FAST: LESS INPUT LAG THAN THE ARCADE. ARCADE: THE BOARD'S OWN TIMING, AS ON THE REAL MACHINE.";
	case ITEM_FAST_SPEED:
		return "HOW FAST THE FAST FORWARD KEY RUNS THE GAME WHILE HELD.";
	case ITEM_HITBOX:
		return "SHOWS YOUR SHIP'S REAL HITBOX: THE SMALL BOX THE GAME TESTS BULLETS AGAINST.";
	case ITEM_BULLETS:
		return "MARKS THE POINT OF EACH ENEMY BULLET THE GAME TESTS: IT HITS ONLY IF THAT POINT GETS INSIDE YOUR HITBOX.";
	case ITEM_VOLUME:
		return "ALL OF THE SOUND: THE MUSIC, THE EFFECTS AND THE VOICES TOGETHER.";
	case ITEM_SOUND:
		return "THE VOLUME: ALL OF THE SOUND, OR THE MUSIC, THE EFFECTS AND THE VOICES ON THEIR OWN.";
	case ITEM_MUSIC_VOLUME:
		return "THE MUSIC'S VOLUME, UNDER THE VOLUME ABOVE.";
	case ITEM_EFFECTS_VOLUME:
		return "THE SOUND EFFECTS' VOLUME (SHOTS, EXPLOSIONS, ITEMS), UNDER THE VOLUME ABOVE.";
	case ITEM_VOICES_VOLUME:
		return "THE VOICES' VOLUME, UNDER THE VOLUME ABOVE.";
	case ITEM_SCALE:
		return "THE WINDOW'S SIZE, AS TIMES THE ARCADE'S PICTURE.";
	case ITEM_FULLSCREEN:
		return "BORDERLESS: A WINDOW OVER THE WHOLE SCREEN (QUICK TO SWITCH). EXCLUSIVE: TRUE FULL SCREEN. "
			"ALT+ENTER OR F11 SWITCHES.";
	case ITEM_CONTROLS:
		return "THE KEYS AND GAMEPAD BUTTONS FOR BOTH PLAYERS, AND THE HOTKEYS.";
	case ITEM_QUIT:
		return "CLOSE THE GAME.";
	case ITEM_DEFAULTS:
		return "THIS PLAYER'S KEYS AND BUTTONS, AND THE HOTKEYS, BACK AS THEY CAME.";
	case ITEM_BACK:
		return "BACK TO THE PAGE BEFORE.";
	case ITEM_GRAPHICS:
		return "SCALING, SMOOTHING, FILTERS, SCANLINES, VSYNC, ROTATION, THE SIDE PANELS.";
	case ITEM_SCALING:
		return "FIT: AS BIG AS IT GOES. INTEGER: WHOLE PIXELS ONLY (SHARPEST). STRETCH: FILLS THE SCREEN.";
	case ITEM_SMOOTH:
		return "SOFTENS THE PIXELS' EDGES WHEN SCALED.";
	case ITEM_FILTER:
		return "PIXEL-ART SCALERS: SMOOTHER EDGES, STILL SHARP. SCALE2X/3X THE LIGHTEST, MMPX KEEPS THE ART'S LOOK, "
			"XBR THE SMOOTHEST. NOT WITH THE CRT LOOK.";
	case ITEM_FILTER_LAYERS:
		return "THE FILTER ON EACH OF THE GAME'S LAYERS AND ITS SPRITES APART, THEN PUT TOGETHER: SHARPER SPRITE EDGES, "
			"NOTHING SMEARED INTO THE BACKGROUND. SCALE2X, SCALE3X AND MMPX ONLY.";
	case ITEM_SCANLINES:
		return "DARK LINES ACROSS THE PICTURE, AS ON AN ARCADE SCREEN.";
	case ITEM_CRT:
		return "THE ARCADE MONITOR'S LOOK: A CURVED SCREEN, ITS LINES, PHOSPHOR STRIPES AND GLOW. LIGHT OR FULL. "
			"IT HAS ITS OWN LINES (SCANLINES IS LEFT ASIDE). BEST IN FULL SCREEN.";
	case ITEM_VSYNC:
		return "SHOWS EACH PICTURE IN STEP WITH YOUR SCREEN: NO TEARING, MAYBE A LITTLE MORE LAG.";
	case ITEM_FPS:
		return "THE FRAME RATE AND THE PC'S TIME A FRAME, ON THIS PANEL (F9 TOO). EACH SECOND IS ALSO WRITTEN TO "
			"DONPACHI_FPS.LOG BESIDE THE PROGRAM.";
	case ITEM_RENDERER:
		return "HOW THE PICTURE REACHES THE SCREEN. AUTO: DIRECT3D 12 ON WINDOWS, THE QUICKEST. "
			"USED FROM THE NEXT START.";
	case ITEM_ROTATE:
		return "TURNS THE PICTURE FOR A SCREEN ON ITS SIDE (TATE).";
	case ITEM_DEADZONE:
		return "HOW FAR THE STICK MUST MOVE BEFORE IT COUNTS.";
	case ITEM_PANELS:
		return "THESE PANELS AT THE SIDES ON A WIDE SCREEN: CHAIN, RANK, BEES, SCORE GRAPH, STAGE AND BOSS HEALTH.";
	case ITEM_HISCORES:
		return "KEEPS THE HIGH SCORE TABLE BETWEEN SESSIONS (THE ARCADE FORGETS IT WHEN SWITCHED OFF).";
	case ITEM_SAVE_STATE:
		return "KEEPS THE GAME AS IT IS NOW IN THE CHOSEN SLOT, TO GO BACK TO (F5). NO HIGH SCORES OR REPLAY FROM A GAME "
			"THAT LOADS ONE.";
	case ITEM_LOAD_STATE:
		return "BACK TO THE CHOSEN SLOT'S SAVED STATE (F8).";
	case ITEM_STATE_SLOT:
		return "WHICH OF THE 5 SAVE STATES SAVE AND LOAD USE (F6 / F7). THEY ARE KEPT UNTIL THE GAME CLOSES.";
	case ITEM_GAME:
		return "DIFFICULTY, FAST FORWARD SPEED, BIG HIT, ARCADE SETTINGS, FREE PLAY, HIGH SCORES, CHEATS AND MORE.";
	case ITEM_DEBUG_TOOLS:
		return "THE DEVELOPERS' DEBUG MODE AND RAM EDITOR, STILL IN THE GAME: PAUSE AND MOVE THROUGH A STAGE, STAGE "
			"SELECT, OBJECT SPAWNER, PALETTE EDITOR. COUNT AS CHEATS.";
	case ITEM_CHEATS:
		return "INFINITE LIVES AND BOMBS, INVINCIBILITY, MAX POWER, ENDLESS CHAIN; MORE WITH MAME'S CHEAT FILE "
			"IN THE CHEATS FOLDER.";
	case ITEM_START_GAME:
		return "PLAY THE ARCADE GAME FROM THE START.";
	case ITEM_TRAINING:
		return "PRACTISE ANY AREA: FROM ITS START, MID-BOSS OR BOSS, WITH YOUR CHOICE OF SHIP AND POWER.";
	case ITEM_SETTINGS:
		return "TIMING, HITBOX, VOLUME, GAME OPTIONS, GRAPHICS, CONTROLS.";
	case ITEM_MAIN_MENU:
		return "LEAVE THIS GAME FOR THE MAIN MENU.";
	case ITEM_T_AREA:
		return "THE AREA TO PRACTISE: 1-5 THE FIRST LOOP, 6-10 THE SECOND (HARDER).";
	case ITEM_T_POINT:
		return "WHERE IN THE AREA TO START: ITS START, PART WAY (5-95% OF THE WAY TO THE BOSS, AS THE SIDE PANEL'S "
			"STAGE PROGRESS), JUST BEFORE ITS MID-BOSS, ITS BOSS (AREA 10: THE TRUE BOSS).";
	case ITEM_T_SHIP:
		return "A: FASTEST, NARROW SHOT. B: MIDDLE SPEED, ITS SHOT TURNS AS YOU MOVE. C: SLOWEST, WIDE SPREAD SHOT.";
	case ITEM_T_POWER:
		return "THE SHOT'S POWER TO START WITH. ON PACE: AS IF EVERY POWER-UP ON THE WAY THERE WAS TAKEN.";
	case ITEM_T_RETRY:
		return "A MISS GOES STRAIGHT BACK TO THE START POINT.";
	case ITEM_T_START:
		return "GO! THE GAME IS RUN TO THE PLACE CHOSEN IN A SECOND OR SO.";
	case ITEM_T_BOMBS:
		return "NORMAL: 3 BOMBS. MAX: 7 BOMBS IN 7 SLOTS, THE MOST THE GAME ALLOWS.";
	case ITEM_TRAINING_MENU:
		return "BACK TO THE TRAINING PAGE, TO CHOOSE AGAIN.";
	case ITEM_T_HITBOX:
		return "SHOWS YOUR SHIP'S HITBOX IN TRAINING (THE NORMAL GAME HAS ITS OWN SETTING).";
	case ITEM_DIFFICULTY:
		return "THE ARCADE'S DIFFICULTY (AS ITS TEST MENU SETS IT). IT TAKES EFFECT AFTER A RESET (F3) OR RESTART.";
	case ITEM_T_DIFFICULTY:
		return "THE DIFFICULTY FOR TRAINING ONLY: THE NORMAL GAME KEEPS ITS OWN.";
	case ITEM_BIG_HIT:
		return "THE ARCADE'S HIDDEN LARGE HIT COUNTER, ALWAYS ON (NORMALLY A SECRET STICK COMMAND).";
	case ITEM_CHECK_UPDATES:
		return "LOOKS FOR A NEWER VERSION ON GITHUB AS THE GAME STARTS. IT IS OFFERED ON THE MAIN MENU, AND ONLY "
			"INSTALLED IF YOU SAY YES. FROM THE NEXT START.";
	case ITEM_UPDATE:
		return "A NEWER VERSION IS OUT: DOWNLOAD AND INSTALL IT (YOU ARE ASKED FIRST).";
	case ITEM_ARCADE:
		return "THE ARCADE'S OWN SETTINGS FROM ITS TEST MENU: CONTINUES, LIVES, EXTENDS, COINS AND DEMO "
			"SOUND. THEY TAKE EFFECT AFTER A RESET (F3) OR RESTART.";
	case ITEM_C_AUTO:
		return "ON: HOLD BUTTON C FOR RAPID FIRE (THE ARCADE'S C BUTTON FULL-AUTO SETTING). OFF: BUTTON C DOES "
			"NOTHING. IN USE AT ONCE.";
	case ITEM_AUTOBOMB:
		return "A BOMB GOES OFF BY ITSELF WHEN YOUR SHIP IS ABOUT TO BE HIT, IF YOU HAVE ONE AND IT WILL SAVE YOU. "
			"GAMES WITH IT ON DON'T COUNT FOR HIGH SCORES, ACHIEVEMENTS OR STATISTICS.";
	case ITEM_SLOWDOWN:
		return "ON: THE GAME SLOWS WHEN THE SCREEN IS BUSY, AS THE ARCADE BOARD DID. OFF: ALWAYS FULL SPEED.";
	case ITEM_VERSION:
		return "USA OR JAPAN. THE JAPANESE VERSION NEEDS MAME'S DONPACHIJ.ZIP ONCE. EACH KEEPS ITS OWN HIGH SCORES.";
	case ITEM_ACHIEVEMENTS:
		return "23 GOALS, AND WHEN YOU MET THEM. EARNED IN ARCADE GAMES ONLY.";
	case ITEM_RETRY_TRAINING:
		return "BACK TO THE START POINT OF THIS TRAINING, AS IT BEGAN.";
	case ITEM_STATISTICS:
		return "YOUR TOTALS AND RECORDS FROM EVERY ARCADE GAME: PLAY TIME, CLEARS, DEATHS, BEST SCORES AND MORE.";
	case ITEM_REPLAYS:
		return "WATCH YOUR SAVED GAMES AGAIN. WHILE WATCHING: SPACE (A) PAUSES, RIGHT STEPS A FRAME.";
	case ITEM_STOP_REPLAY:
		return "STOP WATCHING: BACK TO THE REPLAYS.";
	case ITEM_YES:
		return "SAVES THIS GAME'S REPLAY IN THE REPLAYS FOLDER, TO WATCH OR SHARE.";
	case ITEM_NO:
		return "THE REPLAY IS NOT KEPT.";
	case ITEM_SAVE_REPLAY:
		return "KEEPS THIS ATTEMPT AS A REPLAY (JUST AFTER A MISS: THE ONE THAT ENDED).";
	case ITEM_WATCH:
		return "PLAY IT. SPACE (A) PAUSES, RIGHT (D-PAD) STEPS A FRAME, FAST FORWARD WORKS TOO.";
	case ITEM_RENAME:
		return "GIVE IT A NAME, SHOWN ON THE LIST INSTEAD OF ITS DATE.";
	case ITEM_DELETE:
		return "REMOVE IT FOR GOOD (IT ASKS FIRST).";
	case ITEM_DELETE_YES:
		return "ITS FILE IS DELETED: THIS CAN'T BE UNDONE.";
	case ITEM_DELETE_NO:
		return "KEEP IT.";
	case ITEM_FAST_FORWARD_KEY:
		return "HOLD IT TO RUN THE GAME FASTER (NO SOUND MEANWHILE).";
	case ITEM_SAVE_STATE_KEY:
		return "KEEPS THE GAME AS IT IS NOW (A SAVE STATE).";
	case ITEM_LOAD_STATE_KEY:
		return "BACK TO THE CHOSEN SLOT'S SAVE STATE.";
	case ITEM_SLOT_PREVIOUS_KEY:
		return "THE SAVE STATES' SLOT BEFORE THIS ONE.";
	case ITEM_SLOT_NEXT_KEY:
		return "THE SAVE STATES' SLOT AFTER THIS ONE.";
	}
	return NULL;
}

bool menu_help(const char **title, const char **text)
{
	if (!open || capture >= 0 || page == PAGE_RENAME || selected >= item_count
			|| !(*text = item_help(items[selected])))
		return false;
	*title = item_label(items[selected]);
	return true;
}

/* ---- remapping: the next key or button is the action's ---- */

/* the keys and buttons of the item being remapped, in 's' */
static SDL_Scancode *captured_keys(settings *s)
{
	return is_hotkey_item(capture) ? s->hot_keys[item_hotkey(capture)] : s->keys[player][capture - ITEM_ACTION];
}

static pad_input *captured_buttons(settings *s)
{
	return is_hotkey_item(capture) ? s->hot_buttons[item_hotkey(capture)]
		: s->buttons[player][capture - ITEM_ACTION];
}

static void bind_key(SDL_Scancode key)
{
	SDL_Scancode keys[BINDINGS_MAX] = { key };

	memcpy(captured_keys(host.options), keys, sizeof(keys));
	memcpy(captured_keys(host.file), keys, sizeof(keys));
	capture = -1;
	save();
}

static void bind_pad(pad_input input)
{
	pad_input inputs[BINDINGS_MAX] = { input };

	memcpy(captured_buttons(host.options), inputs, sizeof(inputs));
	memcpy(captured_buttons(host.file), inputs, sizeof(inputs));
	capture = -1;
	save();
}

static bool capture_event(const SDL_Event *event)
{
	if (event->type == SDL_KEYDOWN && !event->key.repeat)
	{
		if (event->key.keysym.scancode == SDL_SCANCODE_ESCAPE)
			capture = -1;    /* (cancel) */
		else
			bind_key(event->key.keysym.scancode);
	}
	else if (event->type == SDL_CONTROLLERBUTTONDOWN)
		bind_pad((pad_input){ PAD_BUTTON, event->cbutton.button });
	else if (event->type == SDL_CONTROLLERAXISMOTION && event->caxis.value > 16384
			&& (event->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || event->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
		bind_pad((pad_input){ PAD_TRIGGER, event->caxis.axis });
	return true;
}

/* ---- the menu's own input ---- */

void menu_init(const menu_host *menu)
{
	host = *menu;
}

bool menu_is_open(void)
{
	return open;
}

void menu_open_front(void)
{
	menu_open();
	front = true;
	front_message = 0;
	page = PAGE_FRONT;
	build_items();
	for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_START_GAME; selected++)
		;
}

void menu_ask_save_replay(int after)
{
	open = true;
	front = false;
	page = PAGE_SAVE_REPLAY;
	selected = 0;
	capture = -1;
	held_direction = NAV_NONE;
	after_question = after;
	build_items();
}

void menu_open_replays(void)
{
	menu_open_front();
	replay_count = replay_list(replays, REPLAYS_LISTED);
	page = PAGE_REPLAYS;
	selected = 0;
	build_items();
}

void menu_open_training(void)
{
	menu_open_front();
	page = PAGE_TRAINING;
	build_items();
	for (selected = 0; selected < item_count - 1 && items[selected] != ITEM_T_START; selected++)
		;    /* (on START TRAINING: again at once) */
}

bool menu_in_front(void)
{
	return open && front;
}

void menu_open(void)
{
	open = true;
	front = false;
	page = PAGE_MAIN;
	selected = 0;
	capture = -1;
	held_direction = NAV_NONE;
	build_items();
}

bool menu_just_closed(void)
{
	const bool closed = just_closed;

	just_closed = false;
	return closed;
}

bool menu_event(const SDL_Event *event)
{
	if (!open)
		return false;
	if (capture >= 0)
		return capture_event(event);
	if (page == PAGE_RENAME)
	{
		/* typed: letters and digits (the text input), Backspace, Enter, Escape, the arrows; a gamepad's X deletes */
		if (event->type == SDL_TEXTINPUT)
		{
			for (const char *c = event->text.text; *c; c++)
				name_insert((char)toupper((unsigned char)*c));
			return true;
		}
		if (event->type == SDL_KEYDOWN)
		{
			const SDL_Scancode key = event->key.keysym.scancode;

			if (key == SDL_SCANCODE_BACKSPACE || key == SDL_SCANCODE_DELETE)
				name_delete();
			else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER)
				rename_navigate(NAV_CHOOSE);
			else if (key == SDL_SCANCODE_ESCAPE)
				rename_navigate(NAV_CLOSE);
			else if (key == SDL_SCANCODE_LEFT)
				rename_navigate(NAV_LEFT);
			else if (key == SDL_SCANCODE_RIGHT)
				rename_navigate(NAV_RIGHT);
			else if (key == SDL_SCANCODE_UP)
				rename_navigate(NAV_UP);
			else if (key == SDL_SCANCODE_DOWN)
				rename_navigate(NAV_DOWN);
			return true;
		}
		if (event->type == SDL_CONTROLLERBUTTONDOWN && event->cbutton.button == SDL_CONTROLLER_BUTTON_X)
		{
			name_delete();
			return true;
		}
	}
	if (event->type == SDL_KEYDOWN)
	{
		const SDL_Scancode key = event->key.keysym.scancode;
		int nav = NAV_NONE;

		if (key == SDL_SCANCODE_UP)
			nav = NAV_UP;
		else if (key == SDL_SCANCODE_DOWN)
			nav = NAV_DOWN;
		else if (key == SDL_SCANCODE_LEFT)
			nav = NAV_LEFT;
		else if (key == SDL_SCANCODE_RIGHT)
			nav = NAV_RIGHT;
		else if (event->key.repeat)
			nav = NAV_NONE;    /* (only the arrows repeat) */
		else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER || key == SDL_SCANCODE_Z
				|| key == SDL_SCANCODE_SPACE)
			nav = NAV_CHOOSE;
		else if (key == SDL_SCANCODE_BACKSPACE || key == SDL_SCANCODE_X)
			nav = NAV_BACK;
		else if (key == SDL_SCANCODE_ESCAPE)
			nav = NAV_CLOSE;
		navigate(nav);
	}
	else if (event->type == SDL_CONTROLLERBUTTONDOWN)
	{
		const int button = event->cbutton.button;

		if (button == SDL_CONTROLLER_BUTTON_A || button == SDL_CONTROLLER_BUTTON_START)
			navigate(NAV_CHOOSE);
		else if (button == SDL_CONTROLLER_BUTTON_B)
			navigate(NAV_BACK);
		else if (button == SDL_CONTROLLER_BUTTON_BACK)
			navigate(NAV_CLOSE);
		/* (the d-pad is read in menu_tick, with the stick, for their repeats) */
	}
	return true;
}

/* the direction the gamepads' d-pads and sticks point */
static int pad_direction(void)
{
	for (int index = 0; index < 2; index++)
	{
		SDL_GameController *const pad = host.pads[index];
		int x, y;

		if (!pad)
			continue;
		x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -STICK_DEAD)
			return NAV_UP;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > STICK_DEAD)
			return NAV_DOWN;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -STICK_DEAD)
			return NAV_LEFT;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > STICK_DEAD)
			return NAV_RIGHT;
	}
	return NAV_NONE;
}

void menu_tick(void)
{
	int direction;

	if (!open)
		return;
	blink++;
	{
		/* the SOUND page's example stops as its entry is left (another entry, page or the menu closed) */
		static int previewed = -1;
		const int now = page == PAGE_SOUND ? items[selected] : -1;

		if (now != previewed)
			ui_sound_preview_stop();
		previewed = now;
	}
	if (updater_state() == UPDATER_AVAILABLE)
		update_found = true;
	if (page == PAGE_FRONT && update_listed != update_found)
	{
		/* (the check has found a newer release while the main menu was up: its entry added, the choice kept) */
		const int was = items[selected];

		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != was; selected++)
			;
	}
	if (page == PAGE_UPDATE)
	{
		if (updater_state() == UPDATER_READY && host.install_update)
			host.install_update();    /* (the files put in place and the program closed to start again, or FAILED) */
		if (update_listed != updater_state())
		{
			build_items();
			selected = 0;
		}
	}
	if (capture >= 0)
	{
		if (--capture_frames <= 0)
			capture = -1;    /* (nothing pressed: no change) */
		return;
	}
	direction = pad_direction();
	if (direction != held_direction)
	{
		held_direction = direction;
		held_frames = 0;
		navigate(direction);
	}
	else if (direction != NAV_NONE && ++held_frames >= REPEAT_FIRST && (held_frames - REPEAT_FIRST) % REPEAT_NEXT == 0)
		navigate(direction);
}

/* ---- drawing ---- */

static uint32_t shade(uint32_t colour, int percent)
{
	const uint32_t r = (colour >> 16 & 255u) * (uint32_t)percent / 100u, g = (colour >> 8 & 255u) * (uint32_t)percent / 100u,
		b = (colour & 255u) * (uint32_t)percent / 100u;

	return r << 16 | g << 8 | b;
}

/* character 'ch' at character column 'column' and row 'row' of the picture */
/* (at pixel line y) */
static void put_glyph(uint32_t *out, int x, int y, char ch, uint32_t colour, int scale)
{
	static const int pens[16] = { 0, 100, 92, 84, 76, 68, 60, 60, 60, 60, 60, 60, 60, 60, 60, -1 };
	const int code = toupper((unsigned char)ch) - 0x20;
	const uint8_t *tile;

	if (code <= 0 || code >= 0x60 || x < 0 || x + 8 * scale > PICTURE_W || y < 0 || y + 8 * scale > PICTURE_H
			|| !rom_set.layer[2])
		return;
	tile = rom_set.layer[2] + code * 32;
	for (int i = 0; i < 8; i++)          /* the picture's x within the character: the tile's row */
		for (int j = 0; j < 8; j++)      /* its y: the tile's column, from the right */
		{
			const int tx = 7 - j, ty = i;
			const uint8_t byte = tile[ty * 4 + tx / 2];
			const int pen = (tx & 1) ? (byte & 15) : (byte >> 4);
			const uint32_t value = pens[pen] < 0 ? 0x101018u : shade(colour, pens[pen]);

			if (pen == 0)
				continue;
			for (int dy = 0; dy < scale; dy++)
				for (int dx = 0; dx < scale; dx++)
					out[(y + j * scale + dy) * PICTURE_W + x + i * scale + dx] = value;
		}
}

static void put_char_y(uint32_t *out, int column, int y, char ch, uint32_t colour)
{
	if (column >= 0 && column < COLUMNS)
		put_glyph(out, column * 8, y, ch, colour, 1);
}

static void put_text_y(uint32_t *out, int column, int y, const char *text, uint32_t colour)
{
	for (; *text; text++, column++)
		put_char_y(out, column, y, *text, colour);
}

static void put_text(uint32_t *out, int column, int row, const char *text, uint32_t colour)
{
	put_text_y(out, column, row * 8, text, colour);
}

static void put_right_y(uint32_t *out, int right_column, int y, const char *text, uint32_t colour)
{
	put_text_y(out, right_column - (int)strlen(text) + 1, y, text, colour);
}

/* ---- the notice ---- */

#define NOTICE_FRAMES 100

static char notice[COLUMNS + 1];
static int notice_frames;

void menu_notice(const char *text)
{
	snprintf(notice, sizeof(notice), "%s", text);
	notice_frames = NOTICE_FRAMES;
}

const uint32_t *menu_with_notice(const uint32_t *picture)
{
	static uint32_t out[PICTURE_W * PICTURE_H];
	const int width = (int)strlen(notice), column = (COLUMNS - width) / 2, row = 5;

	if (notice_frames <= 0)
		return picture;
	notice_frames--;
	memcpy(out, picture, sizeof(out));
	/* a dark band under it */
	for (int y = row * 8 - 3; y < row * 8 + 11; y++)
		for (int x = column * 8 - 6; x < (column + width) * 8 + 6; x++)
			if (x >= 0 && x < PICTURE_W && y >= 0 && y < PICTURE_H)
				out[y * PICTURE_W + x] = 0x000000u;
	put_text(out, column, row, notice, GOLD);
	return out;
}

/* a line centred to the pixel (an odd number of letters half a letter off the columns) */
static void put_centre_y(uint32_t *out, int y, const char *text, uint32_t colour)
{
	for (int x = (PICTURE_W - (int)strlen(text) * 8) / 2; *text; text++, x += 8)
		put_glyph(out, x, y, *text, colour, 1);
}

static void put_centre(uint32_t *out, int row, const char *text, uint32_t colour)
{
	put_centre_y(out, row * 8, text, colour);
}

static void fill(uint32_t *out, int x0, int y0, int x1, int y1, uint32_t colour)
{
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++)
			out[y * PICTURE_W + x] = colour;
}

static const char *item_label(int item)
{
	static const char *const labels[] = {
		"RESUME", "RESET", "SERVICE MENU", "FREE PLAY", "INPUT TIMING", "FAST FWD SPEED", "HITBOX", "VOLUME",
		"WINDOW SIZE", "FULL SCREEN",
		"CONTROLS", "QUIT TO DESKTOP", "RESET TO DEFAULTS", "BACK",
		"GRAPHICS", "SCALING", "SMOOTHING", "SCANLINES", "VSYNC", "ROTATE", "STICK DEADZONE", "SIDE PANELS",
		"SAVE HIGH SCORES", "SAVE STATE", "LOAD STATE", "GAME OPTIONS", "CHEATS",
		"START GAME", "TRAINING", "SETTINGS", "MAIN MENU",
		"AREA", "START AT", "SHIP", "POWER", "RETRY ON MISS", "START TRAINING", "BOMBS", "TRAINING MENU", "HITBOX", "DIFFICULTY", "DIFFICULTY", "BIG HIT DISPLAY", "VERSION: USA", "ACHIEVEMENTS",
		"RETRY TRAINING", "STATISTICS", "REPLAYS", "STOP REPLAY", "YES", "NO",
		"SAVE REPLAY", "WATCH", "RENAME", "DELETE", "YES, DELETE IT", "NO", "SLOWDOWN", "CRT", "STATE SLOT", "DEBUG TOOLS",
		"FILTER", "AUTOBOMB", "RENDERER", "UPDATE", "YES, UPDATE", "NOT NOW", "CHECK FOR UPDATES", "SOUND", "MUSIC",
		"EFFECTS", "VOICES", "BULLETS", "BY LAYER", "FPS METER", "C AUTO FIRE",
		"ARCADE SETTINGS", "CONTINUE", "CONTINUE COST", "LIVES", "EXTEND", "COIN A", "COIN B", "DEMO SOUND"
	};

	if (is_hotkey_item(item))
	{
		static const char *const names[] = { "FAST FWD", "SAVE STATE", "LOAD STATE", "PREV SLOT", "NEXT SLOT" };

		return names[item - ITEM_FAST_FORWARD_KEY];
	}
	if (is_cheat(item))
	{
		/* the cheat file's name, made to fit beside its ON / OFF ("Maximum" as "Max") */
		static char name[64];
		const char *const full = cheats_name(item - ITEM_CHEAT), *const longer = strstr(full, "Maximum");

		if (longer)
			snprintf(name, sizeof(name), "%.*sMax%s", (int)(longer - full), full, longer + 7);
		else
			snprintf(name, sizeof(name), "%s", full);
		name[19] = '\0';
		return name;
	}
	if (is_replay(item))
		return replays[item - ITEM_REPLAY].date;
	if (is_stat(item))
		return stats_label(item - ITEM_STAT);
	if (item >= ITEM_ACHIEVEMENT)
		return achievements_name(item - ITEM_ACHIEVEMENT);
	if (item == ITEM_VERSION && host.version && host.version() == 1)
		return "VERSION: JAPAN";
	if (item == ITEM_UPDATE)
	{
		static char label[40];

		snprintf(label, sizeof(label), "UPDATE TO V%s", updater_latest());
		return label;
	}
	if (item == ITEM_UPDATE_YES && updater_state() == UPDATER_FAILED)
		return "TRY AGAIN";
	return is_action(item) ? action_labels[item - ITEM_ACTION] : labels[item];
}

static void item_value(int item, char *out, size_t size)
{
	const settings *const s = host.options;

	out[0] = '\0';
	switch (item)
	{
	case ITEM_FREE_PLAY:
		snprintf(out, size, "%s", s->free_play ? "ON" : "OFF");
		break;
	case ITEM_HITBOX:
		snprintf(out, size, "%s", s->hitbox ? "ON" : "OFF");
		break;
	case ITEM_BULLETS:
		snprintf(out, size, "%s", s->bullets ? "ON" : "OFF");
		break;
	case ITEM_HISCORES:
		snprintf(out, size, "%s", s->save_high_scores ? "ON" : "OFF");
		break;
	case ITEM_CHECK_UPDATES:
		snprintf(out, size, "%s", s->check_updates ? "ON" : "OFF");
		break;
	case ITEM_BIG_HIT:
		snprintf(out, size, "%s", s->big_hit ? "ON" : "OFF");
		break;
	case ITEM_AUTOBOMB:
		snprintf(out, size, "%s", s->autobomb ? "ON" : "OFF");
		break;
	case ITEM_C_AUTO:
		snprintf(out, size, "%s", cave_board.eeprom.words[DIFFICULTY_WORD] & C_AUTO_BIT ? "ON" : "OFF");
		break;
	case ITEM_A_CONTINUE: case ITEM_A_CONTINUE_COST: case ITEM_A_LIVES: case ITEM_A_EXTEND: case ITEM_A_COIN_A:
	case ITEM_A_COIN_B: case ITEM_A_DEMO_SOUND:
		snprintf(out, size, "%s", arcade_settings[item - ITEM_A_CONTINUE].names[arcade_value(item)]);
		break;
	case ITEM_SLOWDOWN:
		snprintf(out, size, "%s", s->slowdown ? "ON" : "OFF");
		break;
	case ITEM_TIMING:
		snprintf(out, size, "%s", s->arcade_timing ? "ARCADE" : "FAST");
		break;
	case ITEM_FAST_SPEED:
		snprintf(out, size, "%dX", s->fast_forward_speed);
		break;
	case ITEM_VOLUME:
		snprintf(out, size, "%d%%", s->volume);
		break;
	case ITEM_MUSIC_VOLUME:
		snprintf(out, size, "%d%%", s->music_volume);
		break;
	case ITEM_EFFECTS_VOLUME:
		snprintf(out, size, "%d%%", s->effects_volume);
		break;
	case ITEM_VOICES_VOLUME:
		snprintf(out, size, "%d%%", s->voices_volume);
		break;
	case ITEM_SCALE:
		snprintf(out, size, "%dX", s->scale);
		break;
	case ITEM_FULLSCREEN:
		snprintf(out, size, "%s", !s->fullscreen ? "OFF" : s->exclusive ? "EXCLUSIVE" : "BORDERLESS");
		break;
	case ITEM_CONTROLS:
	case ITEM_GRAPHICS:
	case ITEM_GAME:
	case ITEM_ARCADE:
	case ITEM_CHEATS:
	case ITEM_DEBUG_TOOLS:
	case ITEM_ACHIEVEMENTS:
	case ITEM_STATISTICS:
	case ITEM_SOUND:
		snprintf(out, size, ">");
		break;
	case ITEM_T_AREA:
		snprintf(out, size, "< %d >", s->training_area + 1);
		break;
	case ITEM_T_POINT:
	{
		static const char *const points[TRAINING_POINTS] = { "STAGE START", "MID-BOSS", "BOSS", "TRUE BOSS", "" };

		if (s->training_point == TRAINING_PART_WAY)
			snprintf(out, size, "%d%% OF THE WAY", s->training_percent);
		else
			snprintf(out, size, "%s", points[s->training_point < 0 || s->training_point >= TRAINING_POINTS ? 0 : s->training_point]);
		break;
	}
	case ITEM_T_SHIP:
		snprintf(out, size, "TYPE %c", 'A' + s->training_ship);
		break;
	case ITEM_T_POWER:
	{
		const training_setup setup = { s->training_area, s->training_point, 0, s->training_power, false, 0,
			s->training_percent };

		snprintf(out, size, "%s", training_power_name(&setup));
		break;
	}
	case ITEM_T_BOMBS:
		snprintf(out, size, "%s", s->training_max_bombs ? "MAX (7)" : "NORMAL (3)");
		break;
	case ITEM_T_HITBOX:
		snprintf(out, size, "%s", s->training_hitbox ? "ON" : "OFF");
		break;
	case ITEM_T_DIFFICULTY:
		snprintf(out, size, "%s", training_difficulty_name(s->training_difficulty));
		break;
	case ITEM_DIFFICULTY:
	{
		static const char *const names[4] = { "NORMAL", "EASY", "HARD", "VERY HARD" };

		snprintf(out, size, "%s", names[cave_board.eeprom.words[DIFFICULTY_WORD] >> 8 & 3u]);
		break;
	}
	case ITEM_T_RETRY:
		snprintf(out, size, "%s", s->training_retry ? "ON" : "OFF");
		break;
	case ITEM_PANELS:
		snprintf(out, size, "%s", s->panels ? "ON" : "OFF");
		break;
	case ITEM_DEADZONE:
		snprintf(out, size, "%d%%", s->stick_dead[player]);
		break;
	case ITEM_SCALING:
		snprintf(out, size, "%s", settings_scaling_names[s->scaling]);
		break;
	case ITEM_SMOOTH:
		snprintf(out, size, "%s", s->smooth ? "ON" : "OFF");
		break;
	case ITEM_FILTER:
		snprintf(out, size, "%s", scaler_name(s->filter));
		break;
	case ITEM_FILTER_LAYERS:
		/* (xBR blends: by layer it can't, so it stays on the whole picture) */
		snprintf(out, size, "%s", !s->filter_layers ? "OFF" : scaler_by_layer(s->filter) || s->filter == SCALER_OFF
			? "ON" : "NO (XBR)");
		break;
	case ITEM_SCANLINES:
		snprintf(out, size, "%s", settings_scanline_names[s->scanlines]);
		break;
	case ITEM_CRT:
		snprintf(out, size, "%s", settings_crt_names[s->crt]);
		break;
	case ITEM_STATE_SLOT:
		if (host.state_slot)
		{
			const int slot = host.state_slot(0);

			snprintf(out, size, "%d%s", slot + 1, host.state_used && !host.state_used(slot) ? " (EMPTY)" : "");
		}
		break;
	case ITEM_VSYNC:
		snprintf(out, size, "%s", s->vsync ? "ON" : "OFF");
		break;
	case ITEM_FPS:
		snprintf(out, size, "%s", s->show_fps ? "ON" : "OFF");
		break;
	case ITEM_RENDERER:
		snprintf(out, size, "%s", settings_renderer_names[s->renderer]);
		break;
	case ITEM_ROTATE:
		snprintf(out, size, "%s", s->rotate == ROTATE_NONE ? "OFF" : settings_rotate_names[s->rotate]);
		break;
	default:
		if (is_cheat(item))
			snprintf(out, size, "%s", cheats_on(item - ITEM_CHEAT) ? "ON" : "OFF");
		else if (is_action(item) || is_hotkey_item(item))
		{
			const bool hotkey = is_hotkey_item(item);
			char key[32];

			key_label(hotkey ? s->hot_keys[item_hotkey(item)][0] : s->keys[player][item - ITEM_ACTION][0], key,
				sizeof(key));
			if (strlen(key) > 10)
				key[10] = '\0';
			snprintf(out, size, "%s / %s", key,
				pad_label(hotkey ? s->hot_buttons[item_hotkey(item)][0] : s->buttons[player][item - ITEM_ACTION][0]));
		}
		break;
	}
}

/* the main menu: the title over a night sky, its four entries */
static void draw_front(uint32_t *out)
{
	static const char title[] = "DONPACHI";
	const int scale = 3, title_x = (PICTURE_W - (int)strlen(title) * 8 * scale) / 2;

	for (int y = 0; y < PICTURE_H; y++)
	{
		/* dark blue to black, with a few fixed stars */
		const uint32_t blue = (uint32_t)(48 - y * 40 / PICTURE_H);

		for (int x = 0; x < PICTURE_W; x++)
		{
			const unsigned hash = (unsigned)(x * 73856093u ^ y * 19349663u);

			out[y * PICTURE_W + x] = hash % 997u == 0u ? 0x9090A8u : (blue / 3u) << 16 | (blue / 3u) << 8 | blue;
		}
	}
	/* the title, its shadow under it */
	for (int at = 0; title[at]; at++)
		put_glyph(out, title_x + at * 8 * scale + 3, 63, title[at], 0x301000u, scale);
	for (int at = 0; title[at]; at++)
		put_glyph(out, title_x + at * 8 * scale, 60, title[at], GOLD, scale);
	put_centre(out, 12, host.version && host.version() == 1 ? "ARCADE  JAPANESE VERSION" : "ARCADE  US VERSION", GREY);
	for (int index = 0; index < item_count; index++)
	{
		const bool chosen = index == selected;
		const char *const label = item_label(items[index]);
		const int x = (PICTURE_W - (int)strlen(label) * 8) / 2, y = 128 + index * (item_count > 8 ? 16 : 19);

		if (chosen)
		{
			put_glyph(out, x - 16, y, '>', GOLD, 1);
			put_glyph(out, x + ((int)strlen(label) + 1) * 8, y, '<', GOLD, 1);
		}
		put_centre_y(out, y, label, chosen ? GOLD : items[index] == ITEM_UPDATE ? CYAN : WHITE);
	}
	if (front_message > 0)
	{
		front_message--;
		put_centre(out, 35, "TRAINING MODE: COMING SOON", CYAN);
	}
	if (notice_frames > 0)
	{
		notice_frames--;
		put_centre(out, 34, notice, CYAN);
	}
	put_centre(out, 37, "A: CHOOSE", GREY);
	/* the version, small in the corner (CMake's project version) */
	{
		char version[24];

		snprintf(version, sizeof(version), "V%s", DONPACHI_VERSION);
		put_text_y(out, COLUMNS - (int)strlen(version) - 1, PICTURE_H - 12, version, 0x64647Eu);
	}
}

/* The two lines of hints at the panel's foot, 11 pixels apart (8 let the arrows' tails touch the line below) */
static void put_hints(uint32_t *out, const char *first, const char *second)
{
	const int y = (PANEL_TOP + PANEL_ROWS - 3) * 8;

	put_text_y(out, (COLUMNS - (int)strlen(first)) / 2, y, first, GREY);
	put_text_y(out, (COLUMNS - (int)strlen(second)) / 2, y + 11, second, GREY);
}

/* The achievements page: the list (14 at a time, scrolled with the choice), each with a box filled once earned; under
   it what the chosen one asks for and when it was earned */
#define ACHIEVEMENT_ROWS 14

static void draw_achievements(uint32_t *out)
{
	char line[48];
	const int first = selected < ACHIEVEMENT_ROWS / 2 ? 0
		: selected > item_count - ACHIEVEMENT_ROWS / 2 ? item_count - ACHIEVEMENT_ROWS : selected - ACHIEVEMENT_ROWS / 2;

	put_centre(out, PANEL_TOP + 1, "ACHIEVEMENTS", GOLD);
	snprintf(line, sizeof(line), "UNLOCKED %d / %d", achievements_done(), achievements_count());
	put_text_y(out, (COLUMNS - (int)strlen(line)) / 2, (PANEL_TOP + 2) * 8 + 3, line, GREY);
	for (int row = 0; row < ACHIEVEMENT_ROWS && first + row < item_count; row++)
	{
		const int index = first + row, item = items[index], y = (PANEL_TOP + 4) * 8 + row * 12;
		const bool chosen = index == selected;

		if (chosen)
			put_char_y(out, PANEL_LEFT + 1, y, '>', GOLD);
		if (item == ITEM_BACK)
		{
			put_text_y(out, PANEL_LEFT + 3, y, "BACK", chosen ? GOLD : WHITE);
			continue;
		}
		{
			const bool earned = achievements_date(item - ITEM_ACHIEVEMENT) != NULL;
			const int x = (PANEL_LEFT + 3) * 8;

			/* a box: filled gold once earned */
			fill(out, x, y, x + 7, y + 7, earned ? GOLD : 0x2A2A44u);
			if (!earned)
				fill(out, x + 1, y + 1, x + 6, y + 6, 0x0C0C1Cu);
			put_text_y(out, PANEL_LEFT + 5, y, achievements_name(item - ITEM_ACHIEVEMENT),
				chosen ? GOLD : earned ? WHITE : GREY);
		}
	}
	if (items[selected] >= ITEM_ACHIEVEMENT)
	{
		const int index = items[selected] - ITEM_ACHIEVEMENT;
		const char *const date = achievements_date(index);
		const int y = (PANEL_TOP + 4) * 8 + ACHIEVEMENT_ROWS * 12 + 6;

		put_text_y(out, (COLUMNS - (int)strlen(achievements_text(index))) / 2, y, achievements_text(index), CYAN);
		if (date)
			snprintf(line, sizeof(line), "EARNED %s", date);
		else
			snprintf(line, sizeof(line), "NOT YET");
		put_text_y(out, (COLUMNS - (int)strlen(line)) / 2, y + 12, line, date ? GREEN_TEXT : GREY);
	}
	put_hints(out, "UP / DOWN: SCROLL", "B: BACK");
}

/* The cheats page's list: CHEAT_ROWS cheats at a time, scrolled with the choice (BACK under them, always shown): the
   first shown, and the cheats in all (the items before BACK) */
#define CHEAT_ROWS 10

static int cheats_first(int *total)
{
	const int cheats = item_count - 1, chosen = selected < cheats ? selected : cheats - 1;
	int first = chosen - CHEAT_ROWS / 2;

	if (first > cheats - CHEAT_ROWS)
		first = cheats - CHEAT_ROWS;
	if (first < 0)
		first = 0;
	*total = cheats;
	return first;
}

/* The statistics page: its lines (16 at a time, scrolled with the choice): headings in gold, each total with its
   value on the right */
#define STAT_ROWS 17

static void draw_stats(uint32_t *out)
{
	const int first = item_count <= STAT_ROWS || selected < STAT_ROWS / 2 ? 0
		: selected > item_count - STAT_ROWS / 2 ? item_count - STAT_ROWS : selected - STAT_ROWS / 2;

	put_centre(out, PANEL_TOP + 1, "STATISTICS", GOLD);
	put_centre_y(out, (PANEL_TOP + 2) * 8 + 3, "ARCADE GAMES ONLY", GREY);
	for (int row = 0; row < STAT_ROWS && first + row < item_count; row++)
	{
		const int index = first + row, item = items[index], y = (PANEL_TOP + 4) * 8 + row * 12;
		const bool chosen = index == selected;
		const char *value;

		if (chosen)
			put_char_y(out, PANEL_LEFT + 1, y, '>', GOLD);
		if (item == ITEM_BACK)
		{
			put_text_y(out, PANEL_LEFT + 3, y, "BACK", chosen ? GOLD : WHITE);
			continue;
		}
		value = stats_value(item - ITEM_STAT);
		if (!value)
		{
			put_text_y(out, PANEL_LEFT + 3, y, stats_label(item - ITEM_STAT), GOLD);
			continue;
		}
		put_text_y(out, PANEL_LEFT + 3, y, stats_label(item - ITEM_STAT), chosen ? GOLD : WHITE);
		put_right_y(out, PANEL_RIGHT, y, value, chosen ? GOLD : CYAN);
	}
	put_hints(out, "UP / DOWN: SCROLL", "B: BACK");
}

/* The replays page: the saved ones, newest first (14 at a time): the date, the area reached and the score; under it
   the chosen one's ship, difficulty, length and end */
#define REPLAY_ROWS 14

/* a replay's ship, difficulty and length, and its end (or its training place) */
static void replay_details(uint32_t *out, const replay_info *r, int y)
{
	static const char *const difficulties[4] = { "NORMAL", "EASY", "HARD", "V.HARD" };
	const unsigned long seconds = (unsigned long)((double)r->frames * 17376.0 / 1000000.0);
	char line[48];

	snprintf(line, sizeof(line), "TYPE %c  %s  %lu:%02lu", 'A' + (r->ship % 3), difficulties[r->difficulty & 3],
		seconds / 60u, seconds % 60u);
	put_centre_y(out, y, line, CYAN);
	if (r->training)
		snprintf(line, sizeof(line), "%sTRAINING: %s", r->version == 1 ? "JPN " : "", r->place);
	else
		snprintf(line, sizeof(line), "%s%s", r->version == 1 ? "JAPAN: " : "",
			r->all_clear ? "ALL CLEAR" : r->finished ? "GAME OVER" : "QUIT");
	put_centre_y(out, y + 12, line, GREY);
}

/* the page's entries from 'y', 12 apart, the chosen one marked */
static void draw_item_rows(uint32_t *out, int y)
{
	for (int index = 0; index < item_count; index++, y += 12)
	{
		const bool chosen = index == selected;

		if (chosen)
			put_char_y(out, PANEL_LEFT + 1, y, '>', GOLD);
		put_text_y(out, PANEL_LEFT + 3, y, item_label(items[index]), chosen ? GOLD : WHITE);
	}
}

static void draw_replays(uint32_t *out)
{
	const int first = item_count <= REPLAY_ROWS || selected < REPLAY_ROWS / 2 ? 0
		: selected > item_count - REPLAY_ROWS / 2 ? item_count - REPLAY_ROWS : selected - REPLAY_ROWS / 2;
	char line[48];

	put_centre(out, PANEL_TOP + 1, "REPLAYS", GOLD);
	if (replay_count == 0)
		put_centre_y(out, (PANEL_TOP + 2) * 8 + 3, "NONE YET: PLAY A GAME", GREY);
	else
	{
		snprintf(line, sizeof(line), "%d SAVED", replay_count);
		put_centre_y(out, (PANEL_TOP + 2) * 8 + 3, line, GREY);
	}
	for (int row = 0; row < REPLAY_ROWS && first + row < item_count; row++)
	{
		const int index = first + row, item = items[index], y = (PANEL_TOP + 4) * 8 + row * 12;
		const bool chosen = index == selected;

		if (chosen)
			put_char_y(out, PANEL_LEFT + 1, y, '>', GOLD);
		if (item == ITEM_BACK)
		{
			put_text_y(out, PANEL_LEFT + 3, y, "BACK", chosen ? GOLD : WHITE);
			continue;
		}
		{
			const replay_info *const r = &replays[item - ITEM_REPLAY];

			/* "11-11 15:20 A3" (or its name, T3 from training) ... the score */
			if (r->name[0])
				snprintf(line, sizeof(line), "%-11.11s %c%d", r->name, r->training ? 'T' : 'A', r->area);
			else
				snprintf(line, sizeof(line), "%.5s %.5s %c%d", r->date + 5, r->date + 11, r->training ? 'T' : 'A',
					r->area);
			put_text_y(out, PANEL_LEFT + 3, y, line, chosen ? GOLD : WHITE);
			snprintf(line, sizeof(line), "%u", r->score);
			put_right_y(out, PANEL_RIGHT, y, line, chosen ? GOLD : CYAN);
		}
	}
	if (selected < item_count && is_replay(items[selected]))
		replay_details(out, &replays[items[selected] - ITEM_REPLAY], (PANEL_TOP + 4) * 8 + REPLAY_ROWS * 12 + 6);
	put_hints(out, "A: CHOOSE", "B: BACK");
}

/* A replay chosen: what it is, then WATCH, RENAME, DELETE, BACK; RENAME's name being written; DELETE's question */
static void draw_replay_pages(uint32_t *out)
{
	const replay_info *const r = &replays[replay_chosen];
	char line[48];
	int y = (PANEL_TOP + 3) * 8;

	put_centre(out, PANEL_TOP + 1, page == PAGE_RENAME ? "RENAME REPLAY" : page == PAGE_DELETE ? "DELETE REPLAY"
		: "REPLAY", GOLD);
	put_centre_y(out, y, r->date, WHITE);
	if (r->name[0])
		put_centre_y(out, y += 12, r->name, GOLD);
	snprintf(line, sizeof(line), "SCORE %u", r->score);
	put_centre_y(out, y += 12, line, CYAN);
	replay_details(out, r, y += 12);
	y += 36;
	if (page == PAGE_RENAME)
	{
		/* the name in a box, the cursor under its letter */
		const int width = REPLAY_NAME_MOST * 8, left = (PICTURE_W - width) / 2;

		put_centre_y(out, y, "ITS NAME", GREY);
		y += 14;
		fill(out, left - 4, y - 4, left + width + 4, y + 12, 0x2A2A44u);
		fill(out, left - 3, y - 3, left + width + 3, y + 11, 0x0C0C1Cu);
		for (int at = 0; new_name[at]; at++)
			put_glyph(out, left + at * 8, y, new_name[at], WHITE, 1);
		if ((blink / 16) % 2 == 0)
			fill(out, left + name_at * 8, y + 9, left + name_at * 8 + 7, y + 10, GOLD);
		put_centre_y(out, y + 22, "UP / DOWN: A LETTER", GREY);
		put_centre_y(out, y + 33, "OR TYPE IT", GREY);
		put_hints(out, "A/ENTER: OK  X/BKSP: DEL", "B/ESC: CANCEL");
		return;
	}
	if (page == PAGE_DELETE)
	{
		put_centre_y(out, y, "DELETE THIS REPLAY?", RED);
		put_centre_y(out, y + 12, "(ITS FILE IS REMOVED)", GREY);
	}
	draw_item_rows(out, y + (page == PAGE_DELETE ? 30 : 6));
	put_hints(out, "A: CHOOSE", "B: BACK");
}

/* the update's page: the versions, and what is asked or how far it is */
static void draw_update(uint32_t *out)
{
	const int state = updater_state();
	char line[64];
	int y = (PANEL_TOP + 3) * 8;

	put_centre(out, PANEL_TOP + 1, "UPDATE", GOLD);
	snprintf(line, sizeof(line), "V%s  >  V%s", DONPACHI_VERSION, updater_latest());
	put_centre_y(out, y, line, WHITE);
	y += 24;
	if (state == UPDATER_AVAILABLE)
	{
		put_centre_y(out, y, "DOWNLOAD AND INSTALL IT?", WHITE);
		put_centre_y(out, y += 16, "THE GAME STARTS AGAIN", GREY);
		put_centre_y(out, y += 16, "SETTINGS, SCORES, REPLAYS", GREY);
		put_centre_y(out, y += 12, "AND THE ROM ARE KEPT", GREY);
		put_centre_y(out, y += 16, "(NOTES: GITHUB RELEASES)", GREY);
	}
	else if (state == UPDATER_DOWNLOADING || state == UPDATER_READY)
	{
		const int percent = state == UPDATER_READY ? 100 : updater_progress();
		const int left = PANEL_LEFT * 8 + 24, right = (PANEL_RIGHT + 1) * 8 - 24;

		snprintf(line, sizeof(line), state == UPDATER_READY ? "INSTALLING..." : "DOWNLOADING  %d%%", percent);
		put_centre_y(out, y, line, CYAN);
		y += 20;
		fill(out, left, y, right, y + 8, 0x2A2A44u);
		fill(out, left, y, left + (right - left) * percent / 100, y + 8, GOLD);
	}
	else if (state == UPDATER_FAILED)
	{
		/* (why, in capitals, broken into lines of the panel's width) */
		const char *why = updater_problem();

		put_centre_y(out, y, "IT DID NOT WORK:", RED);
		for (y += 16; *why && y < (PANEL_TOP + 16) * 8; y += 12)
		{
			size_t length = strlen(why), cut = length;

			if (length > 26)
			{
				for (cut = 26; cut > 0 && why[cut] != ' '; cut--)
					;
				if (cut == 0)
					cut = 26;
			}
			for (size_t at = 0; at < cut; at++)
				line[at] = (char)toupper((unsigned char)why[at]);
			line[cut] = '\0';
			put_centre_y(out, y, line, WHITE);
			why += cut;
			while (*why == ' ')
				why++;
		}
	}
	draw_item_rows(out, (PANEL_TOP + 18) * 8);
	put_hints(out, "A: CHOOSE", "B: BACK");
}

void menu_draw(const uint32_t *game, uint32_t *out)
{
	if (training_pending > 0 && --training_pending == 0)
	{
		/* (the LOADING... frame is on the screen: the game run to the place now, a second or so) */
		if (host.training())
		{
			close_menu();    /* (the game is at the place: it goes on from there) */
			memcpy(out, game, sizeof(uint32_t) * PICTURE_W * PICTURE_H);
			return;
		}
		front_message = 180;
	}
	if (page == PAGE_FRONT)
	{
		draw_front(out);
		return;
	}
	const int left = PANEL_LEFT * 8 - 4, right = (PANEL_RIGHT + 1) * 8 + 4;
	const int top = PANEL_TOP * 8 - 4, bottom = (PANEL_TOP + PANEL_ROWS) * 8 + 4;

	for (int index = 0; index < PICTURE_W * PICTURE_H; index++)
		out[index] = shade(game[index], 30);
	fill(out, left, top, right, bottom, 0x0C0C1Cu);
	fill(out, left, top, right, top + 2, GOLD);
	fill(out, left, bottom - 2, right, bottom, GOLD);
	fill(out, left, top, left + 2, bottom, GOLD);
	fill(out, right - 2, top, right, bottom, GOLD);

	if (page == PAGE_MAIN)
		put_centre(out, PANEL_TOP + 1, front ? "SETTINGS" : "OPTIONS", GOLD);
	else if (page == PAGE_GRAPHICS)
		put_centre(out, PANEL_TOP + 1, "GRAPHICS", GOLD);
	else if (page == PAGE_GAME)
		put_centre(out, PANEL_TOP + 1, "GAME OPTIONS", GOLD);
	else if (page == PAGE_SOUND)
		put_centre(out, PANEL_TOP + 1, "SOUND", GOLD);
	else if (page == PAGE_ARCADE)
		put_centre(out, PANEL_TOP + 1, "ARCADE SETTINGS", GOLD);
	else if (page == PAGE_TRAINING)
		put_centre(out, PANEL_TOP + 1, "TRAINING", GOLD);
	else if (page == PAGE_ACHIEVEMENTS)
	{
		draw_achievements(out);
		return;
	}
	else if (page == PAGE_STATS)
	{
		draw_stats(out);
		return;
	}
	else if (page == PAGE_REPLAYS)
	{
		draw_replays(out);
		return;
	}
	else if (page == PAGE_REPLAY_MENU || page == PAGE_RENAME || page == PAGE_DELETE)
	{
		draw_replay_pages(out);
		return;
	}
	else if (page == PAGE_UPDATE)
	{
		draw_update(out);
		return;
	}
	else if (page == PAGE_SAVE_REPLAY)
	{
		put_centre(out, PANEL_TOP + 1, "REPLAY", GOLD);
		put_centre_y(out, (PANEL_TOP + 3) * 8, "DO YOU WANT TO SAVE", WHITE);
		put_centre_y(out, (PANEL_TOP + 3) * 8 + 12, "THE REPLAY?", WHITE);
	}
	else if (page == PAGE_DEBUG)
	{
		put_centre(out, PANEL_TOP + 1, "DEBUG TOOLS", GOLD);
		put_centre(out, PANEL_TOP + 2, "THE DEVELOPERS' OWN", GREY);
	}
	else if (page == PAGE_CHEATS)
	{
		int total;
		const int first = cheats_first(&total);

		put_centre(out, PANEL_TOP + 1, "CHEATS", GOLD);
		if (total > CHEAT_ROWS)
		{
			char shown[32];

			snprintf(shown, sizeof(shown), "%d-%d OF %d", first + 1, first + CHEAT_ROWS, total);
			put_centre(out, PANEL_TOP + 2, shown, GREY);
		}
	}
	else
	{
		char title[48];

		snprintf(title, sizeof(title), "CONTROLS  < PLAYER %d >", player + 1);
		put_centre(out, PANEL_TOP + 1, title, GOLD);
		put_centre(out, PANEL_TOP + 2, "KEY / GAMEPAD", GREY);
	}
	{
		/* the entries, in pixel lines: 16 apart, the controls page's 12 (it has 16 of them) */
		const int step = page == PAGE_CONTROLS ? 12 : page == PAGE_MAIN && !front ? (item_count > 13 ? 14 : 15)
			: page == PAGE_GAME ? 11 : page == PAGE_GRAPHICS ? 14 : 16;    /* (the game options: their note under them as
			   well, above the hints; the graphics page's 13 entries) */
		const int gap = page == PAGE_CONTROLS || (page == PAGE_MAIN && !front) ? 6 : 8;
		int y = (PANEL_TOP + 4) * 8 + (page == PAGE_SAVE_REPLAY ? 32 : 0);    /* (the question above) */
		int cheats_total = 0;
		const int cheats_shown = page == PAGE_CHEATS ? cheats_first(&cheats_total) : 0;

		for (int index = 0; index < item_count; index++)
		{
			const bool chosen = index == selected;
			char value[48];

			if (page == PAGE_CHEATS && index < cheats_total
					&& (index < cheats_shown || index >= cheats_shown + CHEAT_ROWS))
				continue;    /* (scrolled out of the list) */

			if ((items[index] == ITEM_QUIT && !(index > 0 && items[index - 1] == ITEM_MAIN_MENU))
					|| items[index] == ITEM_MAIN_MENU || (index > 0 && items[index - 1] == ITEM_TRAINING_MENU)
					|| items[index] == ITEM_FAST_FORWARD_KEY || items[index] == ITEM_DEADZONE
					|| (items[index] == ITEM_BACK && page != PAGE_CONTROLS) || items[index] == ITEM_SCALE
					|| items[index] == ITEM_SAVE_STATE || items[index] == ITEM_GAME || items[index] == ITEM_T_START)
				y += gap;    /* (a gap before them) */
			if (chosen)
				put_char_y(out, PANEL_LEFT + 1, y, '>', GOLD);
			put_text_y(out, PANEL_LEFT + 3, y, item_label(items[index]), chosen ? GOLD : WHITE);
			item_value(items[index], value, sizeof(value));
			if (chosen && capture >= 0)
				snprintf(value, sizeof(value), "PRESS...");
			put_right_y(out, PANEL_RIGHT - 1, y, value, chosen ? CYAN : GREY);
			y += step;
		}
		if (page == PAGE_GAME || page == PAGE_ARCADE)
		{
			static const char *const note[2] = { "DIFFICULTY CHANGES ON", "F3 RESET OR RESTART" };
			static const char *const arcade_note[2] = { "THESE CHANGE ON", "F3 RESET OR RESTART" };
			const char *const *const lines = page == PAGE_ARCADE ? arcade_note : note;

			for (int line = 0; line < 2; line++)
				put_text_y(out, (COLUMNS - (int)strlen(lines[line])) / 2, y + 2 + line * 10, lines[line], GOLD);
		}
		if (page == PAGE_TRAINING && training_pending > 0)
			put_text_y(out, (COLUMNS - 10) / 2, y + 2, "LOADING...", GOLD);
		if (page == PAGE_TRAINING && front_message > 0)
		{
			front_message--;
			put_text_y(out, (COLUMNS - 22) / 2, y + 2, "COULD NOT GET THERE...", RED);
		}
		if (page == PAGE_CHEATS || page == PAGE_DEBUG)
		{
			static const char *const warning[2] = { "HIGH SCORES SAVING DISABLED", "WHILE CHEATS ARE ACTIVE" };

			for (int line = 0; line < 2; line++)
				put_text_y(out, (COLUMNS - (int)strlen(warning[line])) / 2, y + 2 + line * 10, warning[line], RED);
		}
		if (page == PAGE_DEBUG)
		{
			/* the tools' controls (sudden-desu.net's notes): player 2's controls, mostly */
			static const char *const controls[][2] = {
				{ "DEBUG MODE", NULL },
				{ "P2 START", "PAUSE" },
				{ "  P1 C + UP/DN", "MOVE ON" },
				{ "  P1 START", "GO ON" },
				{ "P1 START+P2 UP/DN", "NUMBER" },
				{ "P2 SHOT+BOMB", "STAGE 2-5" },
				{ "P2 BOMB+C", "OBJECT" },
				{ "P2 STICK", "PALETTE" },
				{ "  HOLD P2 BOMB/C", "CHANGE" },
				{ "RAM EDITOR", NULL },
				{ "P2 STICK", "CURSOR" },
				{ "P2 C + UP/DN", "VALUE" },
			};
			int line_y = y + 26;

			for (size_t line = 0; line < sizeof(controls) / sizeof(controls[0]); line++, line_y += 10)
			{
				if (!controls[line][1])
				{
					line_y += line ? 3 : 0;
					put_text_y(out, PANEL_LEFT + 1, line_y, controls[line][0], GOLD);
					continue;
				}
				put_text_y(out, PANEL_LEFT + 1, line_y, controls[line][0], GREY);
				put_right_y(out, PANEL_RIGHT - 1, line_y, controls[line][1], WHITE);
			}
		}
	}

	if (capture >= 0)
	{
		char seconds[40];

		snprintf(seconds, sizeof(seconds), "PRESS A KEY OR BUTTON  %d", capture_frames / 58 + 1);
		put_centre(out, PANEL_TOP + PANEL_ROWS - 3, seconds, CYAN);
		put_centre(out, PANEL_TOP + PANEL_ROWS - 2, "(ESC: NO CHANGE)", GREY);
	}
	else if (page == PAGE_MAIN)
	{
		put_hints(out, "A: CHOOSE   <>: CHANGE", front ? "B: BACK" : "B / VIEW: CLOSE");
	}
	else if (page == PAGE_GRAPHICS || page == PAGE_GAME || page == PAGE_CHEATS || page == PAGE_DEBUG
			|| page == PAGE_TRAINING || page == PAGE_SOUND || page == PAGE_ARCADE)
	{
		put_hints(out, "A: CHOOSE   <>: CHANGE", "B: BACK");
	}
	else if (page == PAGE_SAVE_REPLAY)
	{
		put_hints(out, "A: CHOOSE", "B: NO");
	}
	else
	{
		put_centre(out, PANEL_TOP + PANEL_ROWS - 1, "A:CHANGE <>:PLAYER B:BACK", GREY);
	}
}

void menu_put_meter(uint32_t *picture, const char *const *lines, int count)
{
	int widest = 0;

	for (int line = 0; line < count; line++)
		if ((int)strlen(lines[line]) > widest)
			widest = (int)strlen(lines[line]);
	if (widest > COLUMNS)
		widest = COLUMNS;
	{
		const int left = PICTURE_W - widest * 8 - 2, top = PICTURE_H - count * 10 - 2;

		fill(picture, left - 2, top - 2, PICTURE_W, PICTURE_H, 0x000000u);
		for (int line = 0; line < count; line++)
			for (int at = 0; lines[line][at] && at < widest; at++)
				put_glyph(picture, left + at * 8, top + line * 10, lines[line][at], line ? WHITE : GOLD, 1);
	}
}
