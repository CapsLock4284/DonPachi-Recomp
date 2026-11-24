/* pads_check: the gamepads and joysticks (ports/sdl2/pads.h) on SDL's virtual joysticks, no device needed:
     one SDL has no layout for (2 axes, a hat, 12 buttons) is opened as a plain joystick for player 1: its hat and
       stick move, buttons 1, 2, 3 are shot, bomb and C, the 12th start, the 11th coin (the defaults)
     its button, hat and axis bound as the controls page binds them (pads_capture on SDL's events): the bound button
       is shot, the default one no longer; a hat and an axis bound to bomb and start; an axis moved a little not
       taken (a trigger resting at one end is measured from there: SDL's first value, which a virtual device
       can't give)
     the names in the file and on the menu, read back the same
     a device with nothing to move a ship (3 buttons, no axis or hat) left alone, the next one player 2's
     one with a layout in a gamecontrollerdb.txt (written here) opened as a game controller, its buttons by name
     a device plugged in while another window took SDL's events (the ROM drop window): found by pads_scan
     a device unplugged: its player's place free again
   PASS or FAIL each. Writes pads_check.txt where it runs */
#include "pads.h"
#include "cave/machine.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-70s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static pad_device players[2];

/* the events as main.c takes them; 'capture': a binding caught from them, as the controls page catches one */
static bool events(pad_input *capture)
{
	SDL_Event event;
	bool caught = false;

	SDL_JoystickUpdate();
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_JOYDEVICEADDED)
			pads_added(players, event.jdevice.which);
		else if (event.type == SDL_JOYDEVICEREMOVED)
			pads_removed(players, event.jdevice.which);
		else if (capture && !caught && pads_capture(&event, capture))
			caught = true;
	}
	return caught;
}

static int attach(int axes, int buttons, int hats)
{
	const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, axes, buttons, hats);

	events(NULL);
	return index;
}

static SDL_Joystick *virtual_of(int player)
{
	return players[player].joy;
}

int main(void)
{
	pad_input bindings[ACTIONS][BINDINGS_MAX];
	pad_input caught;
	char name[32], shown[32];
	SDL_Joystick *joy;

	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
	{
		printf("SDL: %s\n", SDL_GetError());
		return 1;
	}
	memset(bindings, 0, sizeof(bindings));

	/* a plain joystick */
	attach(2, 12, 1);
	joy = virtual_of(0);
	verdict("a device with no layout: player 1's, a plain joystick", joy != NULL && players[0].pad == NULL);
	if (!joy)
		return 1;
	SDL_JoystickSetVirtualButton(joy, 0, 1);
	SDL_JoystickSetVirtualHat(joy, 0, SDL_HAT_UP);
	SDL_JoystickSetVirtualAxis(joy, 0, -32767);
	events(NULL);
	verdict("its button 1 shot, hat up up, stick left left (the defaults)",
		pads_board_inputs(&players[0], (const pad_input (*)[BINDINGS_MAX])bindings, true, 40)
			== (CAVE_BUTTON1 | CAVE_UP | CAVE_LEFT));
	SDL_JoystickSetVirtualButton(joy, 0, 0);
	SDL_JoystickSetVirtualHat(joy, 0, SDL_HAT_CENTERED);
	SDL_JoystickSetVirtualAxis(joy, 0, 0);
	SDL_JoystickSetVirtualButton(joy, 11, 1);
	SDL_JoystickSetVirtualButton(joy, 10, 1);
	SDL_JoystickSetVirtualButton(joy, 2, 1);
	events(NULL);
	verdict("its 12th button start, the 11th coin, the 3rd C",
		pads_board_inputs(&players[0], (const pad_input (*)[BINDINGS_MAX])bindings, true, 40)
			== (CAVE_START | CAVE_COIN | CAVE_BUTTON3));
	SDL_JoystickSetVirtualButton(joy, 11, 0);
	SDL_JoystickSetVirtualButton(joy, 10, 0);
	SDL_JoystickSetVirtualButton(joy, 2, 0);
	events(NULL);

	/* bound as the controls page binds */
	SDL_JoystickSetVirtualButton(joy, 6, 1);
	verdict("button 7 pressed: caught as BTN 7", events(&caught) && caught.kind == PAD_JOY_BUTTON && caught.index == 6);
	bindings[ACTION_SHOT][0] = caught;
	verdict("bound to shot: shot",
		pads_board_inputs(&players[0], (const pad_input (*)[BINDINGS_MAX])bindings, true, 40) == CAVE_BUTTON1);
	SDL_JoystickSetVirtualButton(joy, 6, 0);
	SDL_JoystickSetVirtualButton(joy, 0, 1);
	events(NULL);
	verdict("button 1 then: no longer shot (the default given up)",
		pads_board_inputs(&players[0], (const pad_input (*)[BINDINGS_MAX])bindings, true, 40) == 0);
	SDL_JoystickSetVirtualButton(joy, 0, 0);
	events(NULL);
	SDL_JoystickSetVirtualHat(joy, 0, SDL_HAT_LEFT);
	verdict("hat left: caught as HAT LEFT", events(&caught) && caught.kind == PAD_JOY_HAT && caught.dir == SDL_HAT_LEFT);
	bindings[ACTION_BOMB][0] = caught;
	SDL_JoystickSetVirtualHat(joy, 0, SDL_HAT_CENTERED);
	events(NULL);
	SDL_JoystickSetVirtualAxis(joy, 1, 32767);
	verdict("axis 2 down: caught as AXIS 2+", events(&caught) && caught.kind == PAD_JOY_AXIS && caught.index == 1
		&& caught.dir == 1);
	bindings[ACTION_START][0] = caught;
	verdict("held: start (and down, the stick)",
		pads_board_inputs(&players[0], (const pad_input (*)[BINDINGS_MAX])bindings, true, 40) == (CAVE_START | CAVE_DOWN));
	SDL_JoystickSetVirtualAxis(joy, 1, 0);
	SDL_JoystickSetVirtualHat(joy, 0, SDL_HAT_LEFT);
	events(NULL);
	verdict("hat left: bomb, and left",
		pads_board_inputs(&players[0], (const pad_input (*)[BINDINGS_MAX])bindings, true, 40) == (CAVE_BUTTON2 | CAVE_LEFT));
	SDL_JoystickSetVirtualHat(joy, 0, SDL_HAT_CENTERED);
	events(NULL);

	/* the names */
	{
		static const pad_input samples[] = { { PAD_JOY_BUTTON, 6, 0 }, { PAD_JOY_AXIS, 1, -1 }, { PAD_JOY_HAT, 1, SDL_HAT_DOWN } };
		static const char *const files[] = { "joy_button_7", "joy_axis_2-", "joy_hat_2_down" };
		static const char *const menus[] = { "BTN 7", "AXIS 2-", "HAT2 DOWN" };
		bool same = true;

		for (int i = 0; i < 3; i++)
		{
			pad_input back;

			pads_file_name(samples[i], name, sizeof(name));
			pads_menu_name(samples[i], shown, sizeof(shown));
			same &= !strcmp(name, files[i]) && !strcmp(shown, menus[i]) && pads_parse(name, &back)
				&& back.kind == samples[i].kind && back.index == samples[i].index && back.dir == samples[i].dir;
		}
		verdict("the names in the file and on the menu, read back the same", same && !pads_parse("joy_button_0", &caught)
			&& !pads_parse("joy_hat_1_sideways", &caught));
	}

	/* a trigger resting at one end */
	{
		const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 3, 4, 0);    /* (another device) */
		SDL_Joystick *const trigger = SDL_JoystickOpen(index);

		events(NULL);
		pads_removed(players, SDL_JoystickInstanceID(trigger));    /* (not a player's: its events only) */
		SDL_JoystickSetVirtualAxis(trigger, 2, 12000);
		verdict("an axis moved a third of the way: not caught", !events(&caught));
		SDL_JoystickSetVirtualAxis(trigger, 2, 32767);
		verdict("pressed all the way: caught as AXIS 3+", events(&caught) && caught.kind == PAD_JOY_AXIS
			&& caught.index == 2 && caught.dir == 1);
		SDL_JoystickClose(trigger);
		SDL_JoystickDetachVirtual(index);
		events(NULL);
	}

	/* a device with nothing to move a ship, then a game controller from gamecontrollerdb.txt */
	{
		int junk = attach(0, 3, 0), known;
		char guid[64];
		FILE *file;

		verdict("3 buttons, no axis or hat: left alone (player 2's place still free)", players[1].joy == NULL);
		known = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 8, 0);
		SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(known), guid, sizeof(guid));
		SDL_JoystickDetachVirtual(known);
		events(NULL);
		file = fopen("pads_check.txt", "w");
		fprintf(file, "%s,Test Pad,a:b0,b:b1,x:b2,y:b3,back:b4,start:b5,leftshoulder:b6,rightshoulder:b7,"
			"leftx:a0,lefty:a1,platform:%s,\n", guid, SDL_GetPlatform());
		fclose(file);
		verdict("a layout read from a file", SDL_GameControllerAddMappingsFromFile("pads_check.txt") == 1);
		attach(2, 8, 0);
		verdict("a device with that layout: player 2's, a game controller", players[1].pad != NULL);
		if (players[1].pad)
		{
			SDL_Joystick *const pad_joy = players[1].joy;

			SDL_JoystickSetVirtualButton(pad_joy, 2, 1);
			events(NULL);
			bindings[ACTION_SHOT][1] = (pad_input){ PAD_BUTTON, SDL_CONTROLLER_BUTTON_X, 0 };
			verdict("its button X shot, as its layout names it",
				pads_board_inputs(&players[1], (const pad_input (*)[BINDINGS_MAX])bindings, true, 40) == CAVE_BUTTON1);
			SDL_JoystickSetVirtualButton(pad_joy, 2, 0);
			events(NULL);
		}
		(void)junk;
	}

	/* unplugged */
	{
		const SDL_JoystickID id = players[0].id;

		for (int index = SDL_NumJoysticks() - 1; index >= 0; index--)
			if (SDL_JoystickGetDeviceInstanceID(index) == id)
				SDL_JoystickDetachVirtual(index);
		events(NULL);
		verdict("player 1's unplugged: its place free", players[0].joy == NULL && players[1].joy != NULL);
	}
	/* a device plugged in while another window read SDL's events (the ROM drop window): found by pads_scan */
	{
		SDL_Event event;
		const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 6, 1);
		const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);

		while (SDL_PollEvent(&event))
			;    /* (its SDL_JOYDEVICEADDED taken by the other window) */
		verdict("plugged in while another window took the events: not seen yet", players[0].joy == NULL);
		pads_scan(players);
		verdict("then found by pads_scan: player 1's", players[0].joy != NULL && players[0].id == id);
	}
	remove("pads_check.txt");
	SDL_Quit();
	printf(failures ? "%d FAILED\n" : "all PASS\n", failures);
	return failures != 0;
}
