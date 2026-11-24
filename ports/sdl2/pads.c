/**
 *    Name:        pads.c
 *    Description: the players' gamepads and joysticks (pads.h)
 *    Version:     0.1.0 (draft)
 *    Created:     23-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "pads.h"

#include "cave/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AXIS_HALF   16384    /* an axis or trigger past half way: held */
#define AXIS_MOVED  24576    /* an axis this far from where it rests: bound on the controls page */

void pads_load_mappings(const char *folder)
{
	char path[1024];
	int added;

	snprintf(path, sizeof(path), "%sgamecontrollerdb.txt", folder);
	added = SDL_GameControllerAddMappingsFromFile(path);
	if (added >= 0)
		fprintf(stderr, "gamepads: %d layouts from %s\n", added, path);
}

/* the device's kind as SDL tells it, for the log */
static const char *type_name(SDL_GameControllerType type)
{
	switch (type)
	{
	case SDL_CONTROLLER_TYPE_XBOX360: return "Xbox 360";
	case SDL_CONTROLLER_TYPE_XBOXONE: return "Xbox One";
	case SDL_CONTROLLER_TYPE_PS3: return "PlayStation 3";
	case SDL_CONTROLLER_TYPE_PS4: return "PlayStation 4";
	case SDL_CONTROLLER_TYPE_PS5: return "PlayStation 5";
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return "Switch Pro";
	default: return "gamepad";
	}
}

bool pads_added(pad_device players[2], int device_index)
{
	const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device_index);
	const char *const name = SDL_JoystickNameForIndex(device_index);
	char guid[64];

	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device_index), guid, sizeof(guid));
	for (int player = 0; player < 2; player++)
		if (players[player].joy && players[player].id == id)
			return false;    /* (already open) */
	for (int player = 0; player < 2; player++)
	{
		pad_device *const device = &players[player];

		if (device->joy)
			continue;
		if (SDL_IsGameController(device_index))
		{
			if ((device->pad = SDL_GameControllerOpen(device_index)) == NULL)
			{
				fprintf(stderr, "gamepad \"%s\" (%s): %s\n", name ? name : "?", guid, SDL_GetError());
				return false;
			}
			device->joy = SDL_GameControllerGetJoystick(device->pad);
			fprintf(stderr, "gamepad \"%s\" (%s): player %d's, a %s\n", name ? name : "?", guid, player + 1,
				type_name(SDL_GameControllerGetType(device->pad)));
		}
		else
		{
			SDL_Joystick *const joy = SDL_JoystickOpen(device_index);

			if (!joy)
			{
				fprintf(stderr, "joystick \"%s\" (%s): %s\n", name ? name : "?", guid, SDL_GetError());
				return false;
			}
			if ((SDL_JoystickNumAxes(joy) < 2 && SDL_JoystickNumHats(joy) < 1) || SDL_JoystickNumButtons(joy) < 2)
			{
				/* (nothing to move a ship with: a keyboard's, mouse's or headset's extra controls) */
				fprintf(stderr, "joystick \"%s\" (%s): left alone (%d axes, %d hats, %d buttons)\n", name ? name : "?",
					guid, SDL_JoystickNumAxes(joy), SDL_JoystickNumHats(joy), SDL_JoystickNumButtons(joy));
				SDL_JoystickClose(joy);
				return false;
			}
			device->pad = NULL;
			device->joy = joy;
			fprintf(stderr, "joystick \"%s\" (%s): player %d's, read as it is (%d axes, %d hats, %d buttons; no "
				"layout for it in SDL or gamecontrollerdb.txt)\n", name ? name : "?", guid, player + 1,
				SDL_JoystickNumAxes(joy), SDL_JoystickNumHats(joy), SDL_JoystickNumButtons(joy));
		}
		device->id = id;
		return true;
	}
	return false;
}

void pads_scan(pad_device players[2])
{
	for (int player = 0; player < 2; player++)
		if (players[player].joy && !SDL_JoystickGetAttached(players[player].joy))
			pads_removed(players, players[player].id);    /* (unplugged meanwhile) */
	for (int index = 0; index < SDL_NumJoysticks(); index++)
	{
		const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
		bool open = false;

		for (int player = 0; player < 2; player++)
			open |= players[player].joy && players[player].id == id;
		if (!open)
			pads_added(players, index);
	}
}

void pads_removed(pad_device players[2], SDL_JoystickID id)
{
	for (int player = 0; player < 2; player++)
	{
		pad_device *const device = &players[player];

		if (!device->joy || device->id != id)
			continue;
		if (device->pad)
			SDL_GameControllerClose(device->pad);
		else
			SDL_JoystickClose(device->joy);
		device->pad = NULL;
		device->joy = NULL;
	}
}

bool pads_pressed(const pad_device *device, pad_input input)
{
	if (!device || !device->joy)
		return false;
	switch (input.kind)
	{
	case PAD_BUTTON:
		return device->pad && SDL_GameControllerGetButton(device->pad, (SDL_GameControllerButton)input.index) != 0;
	case PAD_TRIGGER:
		return device->pad && SDL_GameControllerGetAxis(device->pad, (SDL_GameControllerAxis)input.index) > AXIS_HALF;
	case PAD_JOY_BUTTON:
		return SDL_JoystickGetButton(device->joy, input.index) != 0;
	case PAD_JOY_AXIS:
		return SDL_JoystickGetAxis(device->joy, input.index) * input.dir > AXIS_HALF;
	case PAD_JOY_HAT:
		return (SDL_JoystickGetHat(device->joy, input.index) & input.dir) != 0;
	default:
		return false;
	}
}

static bool is_plain(pad_input input)
{
	return input.kind == PAD_JOY_BUTTON || input.kind == PAD_JOY_AXIS || input.kind == PAD_JOY_HAT;
}

/* a plain joystick's default button for an action (-1: none), until the action is given a plain one */
static int default_button(SDL_Joystick *joy, int action)
{
	const int buttons = SDL_JoystickNumButtons(joy);

	switch (action)
	{
	case ACTION_SHOT: return 0;
	case ACTION_BOMB: return 1;
	case ACTION_BUTTON3: return buttons > 2 ? 2 : -1;
	case ACTION_START: return buttons >= 4 ? buttons - 1 : -1;
	case ACTION_COIN: return buttons >= 5 ? buttons - 2 : -1;
	default: return -1;
	}
}

uint16_t pads_board_inputs(const pad_device *device, const pad_input bindings[ACTIONS][BINDINGS_MAX],
	bool left_stick, int dead_percent)
{
	const int dead = dead_percent * 32767 / 100;
	uint16_t pressed = 0;
	int x = 0, y = 0;

	if (!device || !device->joy)
		return 0;
	for (int action = 0; action < ACTIONS; action++)
	{
		bool plain = false;

		for (int binding = 0; binding < BINDINGS_MAX; binding++)
		{
			plain |= is_plain(bindings[action][binding]);
			if (pads_pressed(device, bindings[action][binding]))
				pressed |= settings_action_bits[action];
		}
		if (!device->pad && !plain)
		{
			const int button = default_button(device->joy, action);

			if (button >= 0 && SDL_JoystickGetButton(device->joy, button))
				pressed |= settings_action_bits[action];
		}
	}
	if (device->pad)
	{
		if (left_stick)
		{
			x = SDL_GameControllerGetAxis(device->pad, SDL_CONTROLLER_AXIS_LEFTX);
			y = SDL_GameControllerGetAxis(device->pad, SDL_CONTROLLER_AXIS_LEFTY);
		}
	}
	else
	{
		/* a plain joystick: its first hat, and its first two axes (the stick) */
		if (SDL_JoystickNumHats(device->joy) > 0)
		{
			const Uint8 hat = SDL_JoystickGetHat(device->joy, 0);

			pressed |= (hat & SDL_HAT_UP ? CAVE_UP : 0) | (hat & SDL_HAT_DOWN ? CAVE_DOWN : 0)
				| (hat & SDL_HAT_LEFT ? CAVE_LEFT : 0) | (hat & SDL_HAT_RIGHT ? CAVE_RIGHT : 0);
		}
		if (left_stick && SDL_JoystickNumAxes(device->joy) >= 2)
		{
			x = SDL_JoystickGetAxis(device->joy, 0);
			y = SDL_JoystickGetAxis(device->joy, 1);
		}
	}
	if (x < -dead)
		pressed |= CAVE_LEFT;
	else if (x > dead)
		pressed |= CAVE_RIGHT;
	if (y < -dead)
		pressed |= CAVE_UP;
	else if (y > dead)
		pressed |= CAVE_DOWN;
	return pressed;
}

int pads_direction(const pad_device *device, int dead)
{
	int x = 0, y = 0;
	Uint8 hat = 0;

	if (!device || !device->joy)
		return 0;
	if (device->pad)
	{
		SDL_GameController *const pad = device->pad;

		x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
		hat = (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) ? SDL_HAT_UP : 0)
			| (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) ? SDL_HAT_DOWN : 0)
			| (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) ? SDL_HAT_LEFT : 0)
			| (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ? SDL_HAT_RIGHT : 0);
	}
	else
	{
		if (SDL_JoystickNumHats(device->joy) > 0)
			hat = SDL_JoystickGetHat(device->joy, 0);
		if (SDL_JoystickNumAxes(device->joy) >= 2)
		{
			x = SDL_JoystickGetAxis(device->joy, 0);
			y = SDL_JoystickGetAxis(device->joy, 1);
		}
	}
	if ((hat & SDL_HAT_UP) || y < -dead)
		return 1;
	if ((hat & SDL_HAT_DOWN) || y > dead)
		return 2;
	if ((hat & SDL_HAT_LEFT) || x < -dead)
		return 3;
	if ((hat & SDL_HAT_RIGHT) || x > dead)
		return 4;
	return 0;
}

/* an event's device is a game controller (its own events then name its inputs) */
static bool from_controller(SDL_JoystickID id)
{
	return SDL_GameControllerFromInstanceID(id) != NULL;
}

bool pads_capture(const SDL_Event *event, pad_input *out)
{
	switch (event->type)
	{
	case SDL_JOYBUTTONDOWN:
		if (from_controller(event->jbutton.which))
			return false;
		*out = (pad_input){ PAD_JOY_BUTTON, event->jbutton.button, 0 };
		return true;
	case SDL_JOYHATMOTION:
	{
		const Uint8 value = event->jhat.value;

		if (from_controller(event->jhat.which)
				|| (value != SDL_HAT_UP && value != SDL_HAT_DOWN && value != SDL_HAT_LEFT && value != SDL_HAT_RIGHT))
			return false;
		*out = (pad_input){ PAD_JOY_HAT, event->jhat.hat, value };
		return true;
	}
	case SDL_JOYAXISMOTION:
	{
		SDL_Joystick *const joy = SDL_JoystickFromInstanceID(event->jaxis.which);
		Sint16 rest = 0;

		if (from_controller(event->jaxis.which) || !joy)
			return false;
		if (!SDL_JoystickGetAxisInitialState(joy, event->jaxis.axis, &rest))
			rest = 0;
		if (abs((int)event->jaxis.value - (int)rest) < AXIS_MOVED || abs(event->jaxis.value) < AXIS_HALF)
			return false;    /* (not far enough; or a trigger resting at one end, still near it) */
		*out = (pad_input){ PAD_JOY_AXIS, event->jaxis.axis, event->jaxis.value > 0 ? 1 : -1 };
		return true;
	}
	default:
		return false;
	}
}

int pads_plain_button(const SDL_Event *event)
{
	return event->type == SDL_JOYBUTTONDOWN && !from_controller(event->jbutton.which) ? event->jbutton.button : -1;
}

static const char *hat_name(int dir)
{
	return dir == SDL_HAT_UP ? "up" : dir == SDL_HAT_DOWN ? "down" : dir == SDL_HAT_LEFT ? "left" : "right";
}

void pads_file_name(pad_input input, char *out, size_t size)
{
	if (input.kind == PAD_JOY_BUTTON)
		snprintf(out, size, "joy_button_%d", input.index + 1);
	else if (input.kind == PAD_JOY_AXIS)
		snprintf(out, size, "joy_axis_%d%c", input.index + 1, input.dir > 0 ? '+' : '-');
	else if (input.kind == PAD_JOY_HAT)
		snprintf(out, size, "joy_hat_%d_%s", input.index + 1, hat_name(input.dir));
	else
		snprintf(out, size, "none");
}

void pads_menu_name(pad_input input, char *out, size_t size)
{
	char hat[8];

	if (input.kind == PAD_JOY_BUTTON)
		snprintf(out, size, "BTN %d", input.index + 1);
	else if (input.kind == PAD_JOY_AXIS)
		snprintf(out, size, "AXIS %d%c", input.index + 1, input.dir > 0 ? '+' : '-');
	else if (input.kind == PAD_JOY_HAT)
	{
		snprintf(hat, sizeof(hat), "%s", hat_name(input.dir));
		for (char *c = hat; *c; c++)
			*c = (char)(*c - 'a' + 'A');
		if (input.index == 0)
			snprintf(out, size, "HAT %s", hat);
		else
			snprintf(out, size, "HAT%d %s", input.index + 1, hat);
	}
	else
		snprintf(out, size, "-");
}

bool pads_parse(const char *name, pad_input *out)
{
	int number = 0, used = 0;
	char sign = 0, way[8] = "";

	if (sscanf(name, "joy_button_%d%n", &number, &used) == 1 && !name[used] && number >= 1 && number <= 128)
		*out = (pad_input){ PAD_JOY_BUTTON, number - 1, 0 };
	else if (sscanf(name, "joy_axis_%d%c%n", &number, &sign, &used) == 2 && !name[used] && number >= 1
			&& number <= 32 && (sign == '+' || sign == '-'))
		*out = (pad_input){ PAD_JOY_AXIS, number - 1, sign == '+' ? 1 : -1 };
	else if (sscanf(name, "joy_hat_%d_%7s%n", &number, way, &used) == 2 && !name[used] && number >= 1 && number <= 8)
	{
		const int dir = !SDL_strcasecmp(way, "up") ? SDL_HAT_UP : !SDL_strcasecmp(way, "down") ? SDL_HAT_DOWN
			: !SDL_strcasecmp(way, "left") ? SDL_HAT_LEFT : !SDL_strcasecmp(way, "right") ? SDL_HAT_RIGHT : 0;

		if (!dir)
			return false;
		*out = (pad_input){ PAD_JOY_HAT, number - 1, dir };
	}
	else
		return false;
	return true;
}
