/**
 *    Name:        pads.h
 *    Description: the players' gamepads and joysticks: those SDL knows (its game controllers, the Xbox layout's names,
 *                 more of them from gamecontrollerdb.txt beside the program) and any other with buttons and a stick
 *                 or a d-pad, read as it is (its buttons, axes and hats by number)
 *    Version:     0.1.0 (draft)
 *    Created:     23-11-2026
 *
 *    license:BSD-3-Clause
 *    A device SDL has no layout for (an arcade stick's encoder, an older DirectInput pad, a clone) sends no game
 *    controller events, so before this it was never opened. Now every device is opened as it is plugged in
 *    (SDL_JOYDEVICEADDED, which comes for all of them): as a game controller when SDL knows it, as a plain joystick
 *    when not. A plain one's inputs are bound as "joy_button_N", "joy_axis_N+" / "joy_axis_N-" and "joy_hat_N_up"
 *    (N from 1, as Windows' game controller settings number them) and shown as BTN N, AXIS N+, HAT UP. Until a
 *    player binds them, its first hat and its first two axes move, and buttons 1, 2, 3 are shot, bomb and C, the
 *    last one start and the one before it coin (an action keeps its default until it is given a plain one).
 *    The log (stderr: the console, or donpachi.log on Windows) names each device found and how it is read.
 */
#ifndef DONPACHI_PADS_H
#define DONPACHI_PADS_H

#include "settings.h"

#include <SDL.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	SDL_GameController *pad;    /* SDL knows it: its Xbox layout */
	SDL_Joystick *joy;          /* always (the controller's own joystick when it is one) */
	SDL_JoystickID id;
} pad_device;

/* gamecontrollerdb.txt in 'folder', when it is there: SDL's layouts for more devices (before SDL's devices are
   looked at: call it before SDL_Init's events are read) */
void pads_load_mappings(const char *folder);

/* SDL_JOYDEVICEADDED: the device opened for the first player without one (true), or left (a third device, one
   already open, or one with no stick, d-pad or buttons to speak of: a keyboard's or headset's controls) */
bool pads_added(pad_device players[2], int device_index);
void pads_removed(pad_device players[2], SDL_JoystickID id);

/* Every device plugged in now, as if it had just come (those open already left as they are; one unplugged closed).
   After a window that reads SDL's events itself (the ROM drop window: rom_drop.c), which takes the devices'
   SDL_JOYDEVICEADDED with the rest: a gamepad plugged in before the first run was otherwise not seen until it was
   plugged in again */
void pads_scan(pad_device players[2]);

/* a binding held on the device (a trigger, or an axis, counts past half way) */
bool pads_pressed(const pad_device *device, pad_input input);

/* the board's inputs (cave/machine.h's bits) a player's device holds, by their bindings; 'left_stick': its stick
   moves too ('dead_percent' of the way before it counts) */
uint16_t pads_board_inputs(const pad_device *device, const pad_input bindings[ACTIONS][BINDINGS_MAX],
	bool left_stick, int dead_percent);

/* for the menus: where the device's d-pad or stick points (0 none, 1 up, 2 down, 3 left, 4 right) */
int pads_direction(const pad_device *device, int dead);

/* An event of a plain joystick (not a game controller) as a binding: a button pressed, a hat pushed one way, an
   axis moved most of the way from where it rests. False for anything else */
bool pads_capture(const SDL_Event *event, pad_input *out);

/* A plain joystick's button pressed (SDL_JOYBUTTONDOWN of one SDL has no layout for): its button, or -1 */
int pads_plain_button(const SDL_Event *event);

/* the plain kinds' names: in the file ("joy_button_3"), on the menu ("BTN 3"); parsed back (false: not one) */
void pads_file_name(pad_input input, char *out, size_t size);
void pads_menu_name(pad_input input, char *out, size_t size);
bool pads_parse(const char *name, pad_input *out);

#endif
