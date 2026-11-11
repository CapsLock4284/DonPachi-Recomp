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

#include "cave/cheats.h"
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

#define REPEAT_FIRST 18                 /* frames a direction is held before it repeats, then every REPEAT_NEXT */
#define REPEAT_NEXT  5
#define CAPTURE_FRAMES 345              /* the remapping's wait for a key or button: 6 s */
#define STICK_DEAD   16000

enum { PAGE_MAIN, PAGE_CONTROLS, PAGE_GRAPHICS, PAGE_GAME, PAGE_CHEATS, PAGE_FRONT, PAGE_TRAINING };
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
	ITEM_FAST_FORWARD_KEY = 90,   /* the hotkeys (both players': [hotkeys]): fast forward, save state, load state */
	ITEM_SAVE_STATE_KEY, ITEM_LOAD_STATE_KEY,
	ITEM_ACTION = 100,            /* + an action: a player's key and button for it */
	ITEM_CHEAT = 200              /* + a cheat of the cheat file (cave/cheats.h) */
};

static const char *const action_labels[ACTIONS] = {
	"UP", "DOWN", "LEFT", "RIGHT", "SHOT", "BOMB", "AUTO FIRE", "START", "COIN", "SERVICE"
};

static menu_host host;

static bool is_action(int item)
{
	return item >= ITEM_ACTION && item < ITEM_ACTION + ACTIONS;
}

static bool is_cheat(int item)
{
	return item >= ITEM_CHEAT;
}

/* the hotkeys' entries on the controls page */
static bool is_hotkey_item(int item)
{
	return item >= ITEM_FAST_FORWARD_KEY && item <= ITEM_LOAD_STATE_KEY;
}

static int item_hotkey(int item)
{
	return HOTKEY_FAST_FORWARD + (item - ITEM_FAST_FORWARD_KEY);
}
static bool open, just_closed;
static bool front;                      /* the main menu is up (the game not started): the options are its settings */
static int front_message;               /* frames "coming soon" is still shown */
static int training_pending;            /* START TRAINING: the frames until it starts (LOADING... shown meanwhile) */

void menu_open_front(void);
static int page, selected, player;
static int items[48], item_count;
static int capture = -1;                /* the action waiting for its key or button, or -1 */
static int capture_frames;
static int held_direction, held_frames; /* the gamepads' d-pad and stick, for their repeats */

/* ---- the entries ---- */

static void build_items(void)
{
	item_count = 0;
	if (page == PAGE_MAIN)
	{
		static const int main_items[] = { ITEM_RESUME, ITEM_RESET, ITEM_TIMING, ITEM_FAST_SPEED, ITEM_HITBOX,
			ITEM_VOLUME, ITEM_SAVE_STATE, ITEM_LOAD_STATE, ITEM_GAME, ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_MAIN_MENU,
			ITEM_QUIT };
		/* (the main menu's SETTINGS: what can be set before a game) */
		static const int settings_items[] = { ITEM_TIMING, ITEM_FAST_SPEED, ITEM_HITBOX, ITEM_VOLUME, ITEM_GAME,
			ITEM_GRAPHICS, ITEM_CONTROLS, ITEM_BACK };

		if (front)
			for (size_t index = 0; index < sizeof(settings_items) / sizeof(settings_items[0]); index++)
				items[item_count++] = settings_items[index];
		else
			for (size_t index = 0; index < sizeof(main_items) / sizeof(main_items[0]); index++)
			{
				/* (a training game: back to its page, before MAIN MENU) */
				if (main_items[index] == ITEM_MAIN_MENU && host.in_training && host.in_training())
					items[item_count++] = ITEM_TRAINING_MENU;
				items[item_count++] = main_items[index];
			}
	}
	else if (page == PAGE_TRAINING)
	{
		static const int training_items[] = { ITEM_T_AREA, ITEM_T_POINT, ITEM_T_SHIP, ITEM_T_POWER, ITEM_T_BOMBS,
			ITEM_T_RETRY, ITEM_T_HITBOX, ITEM_T_START, ITEM_BACK };

		for (size_t index = 0; index < sizeof(training_items) / sizeof(training_items[0]); index++)
			items[item_count++] = training_items[index];
	}
	else if (page == PAGE_FRONT)
	{
		static const int front_items[] = { ITEM_START_GAME, ITEM_TRAINING, ITEM_SETTINGS, ITEM_QUIT };

		for (size_t index = 0; index < sizeof(front_items) / sizeof(front_items[0]); index++)
			items[item_count++] = front_items[index];
	}
	else if (page == PAGE_GAME)
	{
		static const int game_items[] = { ITEM_SERVICE, ITEM_FREE_PLAY, ITEM_HISCORES, ITEM_CHEATS, ITEM_BACK };

		for (size_t index = 0; index < sizeof(game_items) / sizeof(game_items[0]); index++)
			items[item_count++] = game_items[index];
	}
	else if (page == PAGE_CHEATS)
	{
		/* the cheat file's that the game can have (they write its RAM) */
		for (int cheat = 0; cheat < cheats_count() && item_count < 40; cheat++)
			if (cheats_usable(cheat))
				items[item_count++] = ITEM_CHEAT + cheat;
		items[item_count++] = ITEM_BACK;
	}
	else if (page == PAGE_GRAPHICS)
	{
		static const int graphics_items[] = { ITEM_SCALING, ITEM_SMOOTH, ITEM_SCANLINES, ITEM_VSYNC, ITEM_ROTATE,
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
	case ITEM_T_AREA:
		in_use->training_area = (in_use->training_area + step + 10) % 10;
		if (in_use->training_area != 9 && in_use->training_point == TRAINING_TRUE_BOSS)
			in_use->training_point = TRAINING_BOSS;    /* (area 10's alone) */
		file->training_area = in_use->training_area;
		file->training_point = in_use->training_point;
		break;
	case ITEM_T_POINT:
	{
		const int points = in_use->training_area == 9 ? 3 : 2;

		in_use->training_point = file->training_point = (in_use->training_point + step + points) % points;
		break;
	}
	case ITEM_T_SHIP:
		in_use->training_ship = file->training_ship = (in_use->training_ship + step + 3) % 3;
		break;
	case ITEM_T_POWER:
		in_use->training_max_power = file->training_max_power = !in_use->training_max_power;
		break;
	case ITEM_T_BOMBS:
		in_use->training_max_bombs = file->training_max_bombs = !in_use->training_max_bombs;
		break;
	case ITEM_T_HITBOX:
		in_use->training_hitbox = file->training_hitbox = !in_use->training_hitbox;
		break;
	case ITEM_T_RETRY:
		in_use->training_retry = file->training_retry = !in_use->training_retry;
		break;
	case ITEM_FREE_PLAY:
		in_use->free_play = file->free_play = !in_use->free_play;
		break;
	case ITEM_HITBOX:
		in_use->hitbox = file->hitbox = !in_use->hitbox;
		break;
	case ITEM_HISCORES:
		in_use->save_high_scores = file->save_high_scores = !in_use->save_high_scores;
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
		in_use->volume = wrap && in_use->volume >= 100 ? 0 : SDL_clamp(in_use->volume + step * 10, 0, 100);
		file->volume = in_use->volume;
		break;
	case ITEM_SCALE:
		in_use->scale = wrap && in_use->scale >= 8 ? 1 : SDL_clamp(in_use->scale + step, 1, 8);
		file->scale = in_use->scale;
		host.apply_window();
		break;
	case ITEM_FULLSCREEN:
		in_use->fullscreen = file->fullscreen = !in_use->fullscreen;
		host.apply_window();
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
	case ITEM_SCANLINES:
		in_use->scanlines = file->scanlines = cycle(in_use->scanlines, 4, step, wrap);
		break;
	case ITEM_VSYNC:
		in_use->vsync = file->vsync = !in_use->vsync;
		host.apply_video();
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
		host.main_menu();
		menu_open_front();
		break;
	case ITEM_TRAINING_MENU:
		host.main_menu();
		menu_open_training();
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
	case ITEM_GAME:
		page = PAGE_GAME;
		selected = 0;
		build_items();
		break;
	case ITEM_CHEATS:
		page = PAGE_CHEATS;
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
			for (int hotkey = HOTKEY_FAST_FORWARD; hotkey <= HOTKEY_LOAD_STATE; hotkey++)
			{
				memcpy(s->hot_keys[hotkey], defaults.hot_keys[hotkey], sizeof(defaults.hot_keys[hotkey]));
				memcpy(s->hot_buttons[hotkey], defaults.hot_buttons[hotkey], sizeof(defaults.hot_buttons[hotkey]));
			}
		}
		save();
		break;
	}
	case ITEM_BACK:
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
			: page == PAGE_CHEATS ? ITEM_CHEATS : ITEM_CONTROLS;

		page = page == PAGE_CHEATS ? PAGE_GAME : PAGE_MAIN;
		build_items();
		for (selected = 0; selected < item_count - 1 && items[selected] != opened_by; selected++)
			;    /* (back on the entry that opened the page) */
		break;
	}
	default:
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

static void navigate(int nav)
{
	const int item = items[selected];

	switch (nav)
	{
	case NAV_UP:
		selected = (selected + item_count - 1) % item_count;
		break;
	case NAV_DOWN:
		selected = (selected + 1) % item_count;
		break;
	case NAV_LEFT:
	case NAV_RIGHT:
		if (page == PAGE_CONTROLS && item != ITEM_DEADZONE)
			player ^= 1;
		else
			change(item, nav == NAV_RIGHT ? 1 : -1, false);
		break;
	case NAV_CHOOSE:
		choose(item);
		break;
	case NAV_BACK:
		if (page == PAGE_FRONT)
			break;    /* (the main menu stays until a game starts) */
		if (page != PAGE_MAIN || front)
			choose(ITEM_BACK);
		else
			close_menu();
		break;
	case NAV_CLOSE:
		if (front)
		{
			if (page != PAGE_FRONT)
				menu_open_front();
			break;
		}
		close_menu();
		break;
	}
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

static void put_centre(uint32_t *out, int row, const char *text, uint32_t colour)
{
	put_text(out, (COLUMNS - (int)strlen(text)) / 2, row, text, colour);
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
		"AREA", "START AT", "SHIP", "POWER", "RETRY ON MISS", "START TRAINING", "BOMBS", "TRAINING MENU", "HITBOX"
	};

	if (is_hotkey_item(item))
		return item == ITEM_FAST_FORWARD_KEY ? "FAST FWD" : item == ITEM_SAVE_STATE_KEY ? "SAVE STATE" : "LOAD STATE";
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
	case ITEM_HISCORES:
		snprintf(out, size, "%s", s->save_high_scores ? "ON" : "OFF");
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
	case ITEM_SCALE:
		snprintf(out, size, "%dX", s->scale);
		break;
	case ITEM_FULLSCREEN:
		snprintf(out, size, "%s", s->fullscreen ? "ON" : "OFF");
		break;
	case ITEM_CONTROLS:
	case ITEM_GRAPHICS:
	case ITEM_GAME:
	case ITEM_CHEATS:
		snprintf(out, size, ">");
		break;
	case ITEM_T_AREA:
		snprintf(out, size, "< %d >", s->training_area + 1);
		break;
	case ITEM_T_POINT:
	{
		static const char *const points[3] = { "STAGE START", "BOSS", "TRUE BOSS" };

		snprintf(out, size, "%s", points[s->training_point < 0 || s->training_point > 2 ? 0 : s->training_point]);
		break;
	}
	case ITEM_T_SHIP:
		snprintf(out, size, "TYPE %c", 'A' + s->training_ship);
		break;
	case ITEM_T_POWER:
		snprintf(out, size, "%s", s->training_max_power ? "MAX" : "NORMAL");
		break;
	case ITEM_T_BOMBS:
		snprintf(out, size, "%s", s->training_max_bombs ? "MAX (7)" : "NORMAL (3)");
		break;
	case ITEM_T_HITBOX:
		snprintf(out, size, "%s", s->training_hitbox ? "ON" : "OFF");
		break;
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
	case ITEM_SCANLINES:
		snprintf(out, size, "%s", settings_scanline_names[s->scanlines]);
		break;
	case ITEM_VSYNC:
		snprintf(out, size, "%s", s->vsync ? "ON" : "OFF");
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
	put_centre(out, 12, "ARCADE  US VERSION", GREY);
	for (int index = 0; index < item_count; index++)
	{
		const bool chosen = index == selected;
		const char *const label = item_label(items[index]);
		const int column = (COLUMNS - (int)strlen(label)) / 2, y = 152 + index * 24;

		if (chosen)
		{
			put_char_y(out, column - 2, y, '>', GOLD);
			put_char_y(out, column + (int)strlen(label) + 1, y, '<', GOLD);
		}
		put_text_y(out, column, y, label, chosen ? GOLD : WHITE);
	}
	if (front_message > 0)
	{
		front_message--;
		put_centre(out, 32, "TRAINING MODE: COMING SOON", CYAN);
	}
	put_centre(out, 37, "A: CHOOSE", GREY);
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
	else if (page == PAGE_TRAINING)
		put_centre(out, PANEL_TOP + 1, "TRAINING", GOLD);
	else if (page == PAGE_CHEATS)
	{
		put_centre(out, PANEL_TOP + 1, "CHEATS", GOLD);
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
		const int step = page == PAGE_CONTROLS ? 12 : page == PAGE_MAIN && !front ? (item_count > 13 ? 14 : 15) : 16;
		const int gap = page == PAGE_CONTROLS || (page == PAGE_MAIN && !front) ? 6 : 8;
		int y = (PANEL_TOP + 4) * 8;

		for (int index = 0; index < item_count; index++)
		{
			const bool chosen = index == selected;
			char value[48];

			if ((items[index] == ITEM_QUIT && !(index > 0 && items[index - 1] == ITEM_MAIN_MENU))
					|| (items[index] == ITEM_MAIN_MENU && !(index > 0 && items[index - 1] == ITEM_TRAINING_MENU))
					|| items[index] == ITEM_TRAINING_MENU || items[index] == ITEM_FAST_FORWARD_KEY || items[index] == ITEM_DEADZONE
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
		if (page == PAGE_TRAINING && training_pending > 0)
			put_text_y(out, (COLUMNS - 10) / 2, y + 2, "LOADING...", GOLD);
		if (page == PAGE_TRAINING && front_message > 0)
		{
			front_message--;
			put_text_y(out, (COLUMNS - 22) / 2, y + 2, "COULD NOT GET THERE...", RED);
		}
		if (page == PAGE_CHEATS)
		{
			static const char *const warning[2] = { "HIGH SCORES SAVING DISABLED", "WHILE CHEATS ARE ACTIVE" };

			for (int line = 0; line < 2; line++)
				put_text_y(out, (COLUMNS - (int)strlen(warning[line])) / 2, y + 2 + line * 10, warning[line], RED);
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
		put_centre(out, PANEL_TOP + PANEL_ROWS - 3, "A: CHOOSE   <>: CHANGE", GREY);
		put_centre(out, PANEL_TOP + PANEL_ROWS - 2, front ? "B: BACK" : "B / VIEW: CLOSE", GREY);
	}
	else if (page == PAGE_GRAPHICS || page == PAGE_GAME || page == PAGE_CHEATS || page == PAGE_TRAINING)
	{
		put_centre(out, PANEL_TOP + PANEL_ROWS - 3, "A: CHOOSE   <>: CHANGE", GREY);
		put_centre(out, PANEL_TOP + PANEL_ROWS - 2, "B: BACK", GREY);
	}
	else
	{
		put_centre(out, PANEL_TOP + PANEL_ROWS - 1, "A:CHANGE <>:PLAYER B:BACK", GREY);
	}
}
