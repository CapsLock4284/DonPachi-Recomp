/**
 *    Name:        main.c
 *    Description: DonPachi in a window, on SDL2: the decompiled program, its picture, its sound, the keyboard
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: donpachi [--settings PATH] [--rom PATH] [--scale N] [--no-sound] [--no-free-play] [--eeprom PATH]
 *                    [--arcade-timing] [--cheats FILE] [--cheat NAME ...] [--list-cheats]
 *      --settings PATH the settings file (default: donpachi.ini beside the program; settings.h): the options below
 *                      and the keys and gamepad buttons of both players. Written with the defaults when it is not
 *                      there; the options below go over it for the run
 *      --rom    the ROM set: MAME's donpachi.zip, or a folder holding it or its files (default: rom). Read on the
 *               first run only: its contents are kept in donpachi_usa.dat beside the program, read from then on;
 *               given on the command line, the set is read and extracted again
 *      --scale  the window's size, in times the picture's 240 x 320 (default 3)
 *      --no-sound      plays no sound
 *      --no-free-play  the coins as on the board (5, 6). By default Start drops in a coin first when the game has no
 *                      credit (cave/free_play.h): DonPachi itself has no free play setting
 *      --arcade-timing shows each picture when the board does: as the game's vblank interrupt returns, with the
 *                      sprites latched as VBLANK began (as MAME). Without it (the default) each picture is shown as
 *                      soon as the game's frame is done, a frame sooner: movement and shots on the 2nd picture after
 *                      the controls are read instead of the 3rd, bombs on the 1st instead of the 2nd. The game runs the
 *                      same either way (tools/latency.py measures both)
 *      --eeprom PATH   where the EEPROM is kept: the test mode's settings (default: donpachi.eeprom beside the
 *                      program). 128 bytes, as MAME's nvram/donpachi/eeprom, which can be copied there. A new one
 *                      starts from the ROM set's with the home defaults (eeprom_home_defaults)
 *      --cheat NAME    turns on the cheat of the cheat file whose name holds NAME ("p1 infinite lives"), as many
 *                      times as there are cheats; NAME=VALUE picks a value where the cheat asks for one
 *      --cheats FILE   the cheat file, MAME's format (default: cheats/donpachi.xml)
 *      --list-cheats   lists the cheat file's cheats
 *
 *    Controls (the defaults: donpachi.ini changes them): keys arrows, Z shot, X bomb, C button C, 1 start, 5 coin,
 *          F2 the test mode (held while the game starts, as on the board); player 2 R F D G, A S Q, 2, 6. Gamepads
 *          (the first plugged in is player 1's, the second player 2's; the Xbox layout's names): d-pad or left stick
 *          move, X shot (held: the laser), A bomb, B button C (the board's rapid fire, with "C BUTTON FULL-AUTO",
 *          the home default), Start start; View opens the options menu. Keys and gamepads work together.
 *          F3 resets the game, F4 resets it into the test mode (the service menu: the service switch held for you),
 *          F11 or Alt+Enter full screen.
 *    The options menu (menu.h): the gamepad's View button or Escape. The game is paused while it is open; Quit is
 *          there.
 *    Hitbox: the options menu's HITBOX (or [game] hitbox) shows each ship's hitbox as a white glowing dot (hitbox.h).
 *    Fast forward: held, LB on the gamepad or Tab ([hotkeys] fast_forward; [game] fast_forward_speed, 4 by default).
 *    Main menu: as the program starts (START GAME, TRAINING, SETTINGS, QUIT TO DESKTOP; [game] main_menu), and from
 *          the options' MAIN MENU. The game starts (from its reset) with START GAME.
 *    Training: the main menu's TRAINING (training.h; [training]): an area, its start or its boss (area 10's true
 *          boss too), the ship and power; the game goes straight there, a save state made there, and a miss goes
 *          back to it (retry_on_miss); F5 keeps another place to go back to. No high scores.
 *    Save state: F5 keeps the game as it is now, F8 goes back to it ([hotkeys] save_state, load_state; or the
 *          options menu); one, in memory until the program closes. A game that went back to one keeps no high score.
 *    High scores: kept in donpachi.hiscore beside the program (hiscore.h; the options menu's SAVE HIGH SCORES, or
 *          [game] save_high_scores); not saved in a game that had a cheat on or went back to a save state.
 *    Cheats: the options menu's GAME OPTIONS > CHEATS turns the cheat file's on and off for the run (or --cheat).
 */
#include "cave/cheats.h"
#include "cave/free_play.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/video.h"

#include "achievements.h"
#include "hiscore.h"
#include "rom_drop.h"
#include "hitbox.h"
#include "training.h"
#include "menu.h"
#include "widgets.h"
#include "window_icon.h"
#include "settings.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the settings: donpachi.ini, then the command line (settings.h); the file's own, which the menu changes and saves */
static settings options, file_options;
static char settings_path[1024];
static SDL_Window *window;
static int running = 1;
static bool fast_forwarding;

/* ---- the gamepads ---- */

static SDL_GameController *pads[2];    /* player 1's, player 2's: the first two plugged in */

/* A gamepad plugged in (SDL also tells of those there at the start): the first free player's */
static void pad_added(int device)
{
	SDL_GameController *pad;

	for (int player = 0; player < 2; player++)
		if (pads[player] && SDL_JoystickGetDeviceInstanceID(device)
				== SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[player])))
			return;    /* (already open) */
	for (int player = 0; player < 2; player++)
		if (!pads[player])
		{
			if ((pad = SDL_GameControllerOpen(device)) == NULL)
				fprintf(stderr, "gamepad %d: %s\n", device, SDL_GetError());
			pads[player] = pad;
			return;
		}
}

static void pad_removed(SDL_JoystickID instance)
{
	for (int player = 0; player < 2; player++)
		if (pads[player] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[player])) == instance)
		{
			SDL_GameControllerClose(pads[player]);
			pads[player] = NULL;
		}
}

static uint16_t pad_inputs(int player)
{
	SDL_GameController *const pad = pads[player];
	const int dead = options.stick_dead[player] * 32767 / 100;
	uint16_t pressed = 0;
	int x, y;

	if (!pad)
		return 0;
	for (int action = 0; action < ACTIONS; action++)
		for (int binding = 0; binding < BINDINGS_MAX; binding++)
			if (settings_pad_pressed(pad, options.buttons[player][action][binding]))
				pressed |= settings_action_bits[action];
	if (!options.left_stick[player])
		return pressed;
	x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
	y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
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

/* ---- the hotkeys: what the port does itself ([hotkeys]) ---- */

#define SERVICE_MENU_FRAMES 180    /* the service switch held after the service menu's reset: the game looks at it
                                      between 110 and 140 frames after its start */

static unsigned service_held;      /* frames the service switch is still held for the player */
static bool hotkey_was_down[HOTKEYS];

static void state_action(bool save);
static bool training_active;       /* this game is training's (training.h): a miss goes back to the place */

static bool hotkey_down(int hotkey)
{
	const Uint8 *const keys = SDL_GetKeyboardState(NULL);

	for (int binding = 0; binding < BINDINGS_MAX; binding++)
	{
		const SDL_Scancode key = options.hot_keys[hotkey][binding];

		if (key != SDL_SCANCODE_UNKNOWN && keys[key])
			return true;
		for (int player = 0; player < 2; player++)
			if (pads[player] && settings_pad_pressed(pads[player], options.hot_buttons[hotkey][binding]))
				return true;
	}
	return false;
}

/* A hotkey pressed (not held from before): the reset, or the reset into the test mode (fast forward is held: the
   main loop asks hotkey_down) */
static void hotkeys(void)
{
	for (int hotkey = HOTKEY_RESET; hotkey <= HOTKEY_SERVICE_MENU; hotkey++)
	{
		const bool down = hotkey_down(hotkey);

		if (down && !hotkey_was_down[hotkey])
		{
			program_reset();
			free_play_reset();
			hiscore_reset();
			training_active = false;
			service_held = hotkey == HOTKEY_SERVICE_MENU ? SERVICE_MENU_FRAMES : 0;
		}
		hotkey_was_down[hotkey] = down;
	}
	for (int hotkey = HOTKEY_SAVE_STATE; hotkey <= HOTKEY_LOAD_STATE; hotkey++)
	{
		const bool down = hotkey_down(hotkey);

		if (down && !hotkey_was_down[hotkey])
			state_action(hotkey == HOTKEY_SAVE_STATE);
		hotkey_was_down[hotkey] = down;
	}
}

/* what the player holds on their keys and gamepad: the board's input bits, 1 = held */
static uint16_t held_inputs(int player)
{
	const Uint8 *const keys = SDL_GetKeyboardState(NULL);
	uint16_t pressed = pad_inputs(player);

	for (int action = 0; action < ACTIONS; action++)
		for (int binding = 0; binding < BINDINGS_MAX; binding++)
			if (options.keys[player][action][binding] != SDL_SCANCODE_UNKNOWN
					&& keys[options.keys[player][action][binding]])
				pressed |= settings_action_bits[action];
	return pressed;
}

/* what the game does not see until it is let go: what was held as the menu closed (the A that chose "Resume" is
   no bomb) */
static uint16_t ignored[2];

static void ignore_held_inputs(void)
{
	ignored[0] = held_inputs(0);
	ignored[1] = held_inputs(1);
}

/* The inputs from the keys and the gamepads held: active low */
static void read_keys(void)
{
	uint16_t pressed[2];

	for (int player = 0; player < 2; player++)
	{
		const uint16_t held = held_inputs(player);

		ignored[player] &= held;    /* (let go: seen again) */
		pressed[player] = (uint16_t)(held & ~ignored[player]);
	}
	if (service_held)
	{
		pressed[0] |= CAVE_SERVICE;
		service_held--;
	}
	cave_board.in0 = (uint16_t)~pressed[0];
	cave_board.in1 = (uint16_t)~pressed[1];
}

static void show_problem(const char *problem)
{
	fprintf(stderr, "The game does not start: %s\n", problem);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DonPachi", problem, NULL);
}

/* ---- the game's data: extracted from the ROM set on the first run, read from then on (no .zip needed) ---- */

#define DATA_FILE "donpachi_usa.dat"    /* beside the program: the US set's contents (never in the repository) */

static bool data_extracted;

/* The extracted data beside the program; without it (the first run, or a damaged file), the ROM set ('rom' in the
   settings), then its contents saved as the data for next time. 'rom_given': the command line's --rom, read
   (and extracted) whatever data there is */
static const char *game_data_open(bool rom_given)
{
	char path[1024];
	char *const base = SDL_GetBasePath();    /* (the program's folder, with its separator) */
	const char *data_problem = "--rom given", *rom_problem, *save_problem;

	snprintf(path, sizeof(path), "%s%s", base ? base : "", DATA_FILE);
	SDL_free(base);
	if (!rom_given && (data_problem = rom_set_load_data(path)) == NULL)
		return NULL;
	if ((rom_problem = rom_set_load(options.rom)) != NULL)
	{
		/* no data and no set: a window asks for the set to be dropped onto it */
		bool saved;

		fprintf(stderr, "%s (%s): the ROM set is asked for\n", rom_problem, data_problem);
		if (!rom_drop_screen(path, "NO DONPACHI_USA.DAT, AND NO DONPACHI.ZIP IN THE 'ROM' FOLDER", &saved))
			return "";    /* (the window closed: no game, no message) */
		data_extracted = saved;
		return NULL;
	}
	if ((save_problem = rom_set_save_data(path)) != NULL)
		fprintf(stderr, "the game's data was not saved: %s (the ROM set is read again next time)\n", save_problem);
	else
	{
		data_extracted = true;
		fprintf(stderr, "the game's data extracted from %s into %s\n", options.rom, path);
	}
	return NULL;
}

/* ---- the sound: the board's samples (cave/sound.h), queued to SDL's audio device a frame at a time ---- */

#define AUDIO_AHEAD_MS     50u     /* silence queued at the start: what the device plays from while frames come */
#define AUDIO_MOST_MS      200u    /* past this much queued (the device's clock slower than the frame's), it starts
                                      again from AUDIO_AHEAD_MS: a skip, rarely */

static SDL_AudioDeviceID audio;

static Uint32 audio_bytes(unsigned milliseconds)
{
	return (Uint32)(CAVE_SOUND_RATE * milliseconds / 1000u * sizeof(int16_t));
}

static void audio_open(void)
{
	SDL_AudioSpec want, have;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		fprintf(stderr, "no sound: %s\n", SDL_GetError());
		return;
	}
	SDL_zero(want);
	want.freq = (int)CAVE_SOUND_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 512;
	audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);    /* (SDL converts to what the device takes) */
	if (!audio)
	{
		fprintf(stderr, "no sound: %s\n", SDL_GetError());
		return;
	}
	SDL_PauseAudioDevice(audio, 0);
}

/* silence, so the device has something to play while the frames come */
static void audio_restart(void)
{
	static const int16_t silence[CAVE_SOUND_RATE * AUDIO_AHEAD_MS / 1000u];

	SDL_ClearQueuedAudio(audio);
	SDL_QueueAudio(audio, silence, (Uint32)sizeof(silence));
}

/* the frame's samples to the device */
static void audio_queue(void)
{
	int16_t samples[2048];
	unsigned count;

	if (!audio)
	{
		cave_sound_drop();
		return;
	}
	if (SDL_GetQueuedAudioSize(audio) > audio_bytes(AUDIO_MOST_MS))
		audio_restart();
	while ((count = cave_sound_take(samples, 2048u)) > 0)
	{
		if (options.volume < 100)
			for (unsigned index = 0; index < count; index++)
				samples[index] = (int16_t)(samples[index] * options.volume / 100);
		SDL_QueueAudio(audio, samples, (Uint32)(count * sizeof(int16_t)));
	}
}

/* ---- the EEPROM's file: loaded before the first frame, saved soon after the game changes it, and at the end ---- */

#define EEPROM_CHECK_FRAMES 60u    /* how often it is checked for changes: about a second */

static char eeprom_path[1024];
static uint16_t eeprom_saved[CAVE_EEPROM_WORDS];

/* The home defaults, for a new EEPROM: "C BUTTON FULL-AUTO" (test mode, configuration item 3: the board's own rapid
   fire on button C). Words 1 and 4 as the test mode writes them when that is the only setting changed from the ROM
   set's EEPROM: word 4's bit 2 is the setting (the game reads it at 034488 into $101287); word 1 goes from FFFB to
   FFFF (the game does not check it) */
static void eeprom_keep(bool always);

static void eeprom_home_defaults(void)
{
	cave_board.eeprom.words[1] = 0xFFFFu;
	cave_board.eeprom.words[4] |= 0x0004u;
}

static void eeprom_open(const char *path)
{
	if (path)
		snprintf(eeprom_path, sizeof(eeprom_path), "%s", path);
	else
	{
		char *const base = SDL_GetBasePath();    /* (the program's folder, with its separator) */

		snprintf(eeprom_path, sizeof(eeprom_path), "%sdonpachi.eeprom", base ? base : "");
		SDL_free(base);
	}
	switch (cave_eeprom_load(eeprom_path))
	{
	case 1:
		break;    /* (the player's own settings) */
	case 0:
		eeprom_home_defaults();
		eeprom_keep(true);
		break;
	default:
		fprintf(stderr, "%s is not an EEPROM of 128 bytes: the ROM set's is used (and saved over it)\n", eeprom_path);
		eeprom_home_defaults();
		break;
	}
	memcpy(eeprom_saved, cave_board.eeprom.words, sizeof(eeprom_saved));
}

static void eeprom_keep(bool always)
{
	const char *problem;

	if (!always && !memcmp(eeprom_saved, cave_board.eeprom.words, sizeof(eeprom_saved)))
		return;
	if ((problem = cave_eeprom_save(eeprom_path)) != NULL)
		fprintf(stderr, "%s: %s\n", eeprom_path, problem);
	else
		memcpy(eeprom_saved, cave_board.eeprom.words, sizeof(eeprom_saved));
}

/* ---- the picture ---- */

static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
static SDL_Renderer *renderer;
static SDL_Texture *texture;

/* the picture from the board's memories, with the sprites latched last */
static void draw_picture(void)
{
	video_draw(frame);
	video_rotated(frame, picture);
}

/* The picture with the ships' hitboxes over it, when they are shown (hitbox.h): the boxes of the frame whose
   sprites the picture has, a frame back (two with arcade timing) */
static const uint32_t *with_hitboxes(const uint32_t *pixels)
{
	static uint32_t marked[VIDEO_WIDTH * VIDEO_HEIGHT];

	if (!options.hitbox && !(training_active && options.training_hitbox))
		return pixels;
	memcpy(marked, pixels, sizeof(marked));
	hitbox_draw(marked, options.arcade_timing ? 2 : 1);
	return marked;
}

/* ---- the picture in the window ([video]: scaling, smoothing, scanlines, rotation) ---- */

/* the side panels beside the picture (widgets.h): when they are on, and the picture is not turned */
static bool panels_shown(void)
{
	return options.panels && options.rotate == ROTATE_NONE;
}

static SDL_Texture *panel_textures[2];
static uint32_t panel_pixels[2][WIDGET_W * WIDGET_H];

/* the picture's size on the screen, in the game's pixels: 240 x 320, or 320 x 240 turned for a monitor on its side;
   with the side panels, the panels' and their gaps' width too */
static int shown_width(void)
{
	if (panels_shown())
		return VIDEO_HEIGHT + 2 * (WIDGET_W + WIDGET_GAP);
	return options.rotate == ROTATE_NONE ? VIDEO_HEIGHT : VIDEO_WIDTH;
}

static int shown_height(void)
{
	return options.rotate == ROTATE_NONE ? VIDEO_WIDTH : VIDEO_HEIGHT;
}

/* a point of the unturned picture's rectangle, turned about the centre (cx, cy) as the picture is */
static void turn(float x, float y, float cx, float cy, float *out_x, float *out_y)
{
	if (options.rotate == ROTATE_RIGHT)
	{
		*out_x = cx - (y - cy);
		*out_y = cy + (x - cx);
	}
	else if (options.rotate == ROTATE_LEFT)
	{
		*out_x = cx + (y - cy);
		*out_y = cy - (x - cx);
	}
	else
	{
		*out_x = x;
		*out_y = y;
	}
}

static void show_picture(const uint32_t *pixels)
{
	static const Uint8 darkness[4] = { 0, 70, 120, 170 };    /* (the scanlines' dark half: off, light, medium, heavy) */
	int out_w, out_h;
	float scale_x, scale_y, w, h, x, y, cx, cy;
	SDL_FRect unturned;

	SDL_UpdateTexture(texture, NULL, pixels, VIDEO_HEIGHT * (int)sizeof(uint32_t));
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
	scale_x = (float)out_w / (float)shown_width();
	scale_y = (float)out_h / (float)shown_height();
	if (options.scaling != VIDEO_STRETCH)
	{
		scale_x = scale_y = scale_x < scale_y ? scale_x : scale_y;
		if (options.scaling == VIDEO_INTEGER && scale_x >= 1.0f)
			scale_x = scale_y = (float)(int)scale_x;
	}
	w = (float)shown_width() * scale_x;
	h = (float)shown_height() * scale_y;
	x = ((float)out_w - w) / 2.0f;
	y = ((float)out_h - h) / 2.0f;
	if (options.scaling == VIDEO_INTEGER)
	{
		x = (float)(int)x;    /* (whole pixels: every game pixel the same size) */
		y = (float)(int)y;
	}
	if (panels_shown())
	{
		/* the panels at the sides, the picture between them: all at the picture's scale */
		const SDL_FRect left = { x, y, WIDGET_W * scale_x, WIDGET_H * scale_y };
		const SDL_FRect right = { x + w - WIDGET_W * scale_x, y, WIDGET_W * scale_x, WIDGET_H * scale_y };

		widgets_draw(panel_pixels[0], panel_pixels[1]);
		for (int side = 0; side < 2; side++)
		{
			SDL_UpdateTexture(panel_textures[side], NULL, panel_pixels[side], WIDGET_W * (int)sizeof(uint32_t));
			SDL_SetTextureScaleMode(panel_textures[side], options.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
			SDL_RenderCopyF(renderer, panel_textures[side], NULL, side ? &right : &left);
		}
		x += (WIDGET_W + WIDGET_GAP) * scale_x;
		w = VIDEO_HEIGHT * scale_x;
	}
	cx = x + w / 2.0f;
	cy = y + h / 2.0f;
	/* the texture's rectangle before it is turned: 240 x 320 in its own shape */
	unturned.w = options.rotate == ROTATE_NONE ? w : h;
	unturned.h = options.rotate == ROTATE_NONE ? h : w;
	unturned.x = cx - unturned.w / 2.0f;
	unturned.y = cy - unturned.h / 2.0f;
	SDL_RenderCopyExF(renderer, texture, NULL, &unturned,
		options.rotate == ROTATE_RIGHT ? 90.0 : options.rotate == ROTATE_LEFT ? 270.0 : 0.0, NULL, SDL_FLIP_NONE);

	/* the scanlines: the arcade monitor's lines are the board's rows, the picture's columns (the monitor stands on
	   its side in the cabinet): the dark half of each, turned with the picture; none when a line is under 2 pixels */
	if (options.scanlines > 0 && unturned.w / VIDEO_HEIGHT >= 2.0f)
	{
		static SDL_FRect lines[VIDEO_HEIGHT];
		const float band = unturned.w / VIDEO_HEIGHT;

		for (int line = 0; line < VIDEO_HEIGHT; line++)
		{
			const float x0 = unturned.x + band * (float)line + band * 0.5f, x1 = unturned.x + band * (float)(line + 1);
			float ax, ay, bx, by;

			turn(x0, unturned.y, cx, cy, &ax, &ay);
			turn(x1, unturned.y + unturned.h, cx, cy, &bx, &by);
			lines[line].x = ax < bx ? ax : bx;
			lines[line].y = ay < by ? ay : by;
			lines[line].w = ax < bx ? bx - ax : ax - bx;
			lines[line].h = ay < by ? by - ay : ay - by;
		}
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, darkness[options.scanlines & 3]);
		SDL_RenderFillRectsF(renderer, lines, VIDEO_HEIGHT);
	}
	SDL_RenderPresent(renderer);
}

/* ---- the big HIT display: the game's own hidden option, set without its stick command ---- */

/* $101738 (a byte): bit 0 player 1's, bit 1 player 2's, set by Up + Left then A + Start during a game (Down + Right then A +
   Start clears it; the game clears both as a game starts): the GP window's HIT count in the large font all the time,
   and the best chain shown between chains (func_006D06). Cosmetic only. With the option on the bits are kept set
   while a game is played; turned off, they are cleared once, and the command works as on the board again */
#define BIG_HIT_FLAGS 0x101738u

static bool big_hit_set;

static void big_hit_frame(void)
{
	uint8_t *const flags = cave_work_ram_byte(BIG_HIT_FLAGS);    /* (a byte) */

	if (options.big_hit && *(const uint16_t *)cave_work_ram_at(0x1018ECu, 2) == 7u)
	{
		*flags |= 3u;
		big_hit_set = true;
	}
	else if (!options.big_hit && big_hit_set)
	{
		*flags &= (uint8_t)~3u;
		big_hit_set = false;
	}
}

/* ---- what the options menu does (menu.h's menu_host) ---- */

static void menu_reset(bool service_menu)
{
	program_reset();
	free_play_reset();
	hiscore_reset();
	training_active = false;
	service_held = service_menu ? SERVICE_MENU_FRAMES : 0;
}

/* full screen as the settings have it: borderless (a window over the desktop) or exclusive */
static Uint32 fullscreen_flag(void)
{
	return options.exclusive ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
}

static void set_fullscreen(void)
{
	const Uint32 now = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;

	if (now == fullscreen_flag())
		return;
	if (now)
		SDL_SetWindowFullscreen(window, 0);    /* (from one kind to the other: out of it first) */
	if (options.exclusive)
	{
		/* the desktop's own mode: no change of resolution or refresh rate */
		SDL_DisplayMode desktop;
		const int display = SDL_GetWindowDisplayIndex(window);

		if (display >= 0 && SDL_GetDesktopDisplayMode(display, &desktop) == 0)
			SDL_SetWindowDisplayMode(window, &desktop);
	}
	SDL_SetWindowFullscreen(window, fullscreen_flag());
}

static void menu_apply_window(void)
{
	if (options.fullscreen)
		set_fullscreen();
	else
	{
		SDL_SetWindowFullscreen(window, 0);
		SDL_SetWindowSize(window, shown_width() * options.scale, shown_height() * options.scale);
	}
}

/* the filter and vsync, as the settings have them */
static void menu_apply_video(void)
{
	SDL_SetTextureScaleMode(texture, options.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	if (SDL_RenderSetVSync(renderer, options.vsync ? 1 : 0) != 0)
		fprintf(stderr, "vsync: %s\n", SDL_GetError());
}

static void menu_apply_timing(void)
{
	program_after_vblank = options.arcade_timing ? draw_picture : NULL;
	if (options.arcade_timing)
		draw_picture();    /* (the picture until the next interrupt draws it) */
}

/* ---- the save state (cave/program.h): one, in memory, until the program closes ---- */

static program_state *save_state;
static void *save_state_panels;    /* (the side panels' run, as it was: widgets.h) */
static bool state_loaded;          /* this game went back to a save state, or had a cheat on: its scores are not kept
                                      (hiscore.h) */

static void state_put_back(const program_state *state, const void *panels);

static void state_action(bool save)
{
	if (save)
	{
		program_state *const state = program_state_save();
		void *const panels = widgets_state_save();

		if (!state || !panels)
		{
			program_state_free(state);
			free(panels);
			menu_notice("NO MEMORY FOR A SAVE STATE");
			return;
		}
		program_state_free(save_state);
		free(save_state_panels);
		save_state = state;
		save_state_panels = panels;
		menu_notice("STATE SAVED");
		return;
	}
	if (!save_state)
	{
		menu_notice("NO SAVE STATE YET");
		return;
	}
	state_put_back(save_state, save_state_panels);
	menu_notice("STATE LOADED");
}

/* The options' MAIN MENU: the game reset and paused, the main menu up (menu.c), until START GAME */
static void menu_main_menu(void)
{
	menu_reset(false);
	widgets_reset();
	state_loaded = false;
	memset(picture, 0, sizeof(picture));
	if (audio)
		audio_restart();
}

/* ---- training (training.h): the game to a place of the player's choosing, a save state there ---- */

/* A state put back: the game, the side panels' run, the picture and sound (no high score from the game after) */
static void state_put_back(const program_state *state, const void *panels)
{
	program_state_load(state);
	widgets_state_load(panels);
	free_play_reset();
	hiscore_state_loaded();
	video_latch_sprites();    /* (the sprites the state's picture had) */
	draw_picture();
	if (audio)
		audio_restart();
	state_loaded = true;
}

static program_state *training_place;    /* the training game's place as it began (RETRY TRAINING: F5 does not
                                            change it, as it does the retry on a miss's) */
static void *training_place_panels;
static bool ship_was_alive;
static int retry_in;               /* frames to the retry, after a miss */
static unsigned training_stage;    /* the stage the training game is in: when it changes (past the area's BONUS
                                      screen), the training game is over */

static bool in_training(void)
{
	return training_active;
}

static bool menu_training(void)
{
	const training_setup setup = { options.training_area, options.training_point, options.training_ship,
		options.training_power, options.training_max_bombs, options.training_difficulty };
	const char *problem;

	hiscore_reset();
	free_play_reset();
	service_held = 0;
	if ((problem = training_prepare(&setup)) != NULL)
	{
		fprintf(stderr, "training: %s\n", problem);
		menu_reset(false);    /* (the main menu's START GAME: from the board's reset) */
		return false;
	}
	hiscore_frame(options.save_high_scores, false);    /* (the saved table back, the HUD's HIGH with it) */
	training_hud();
	widgets_reset();
	widgets_begin_game();
	program_state_free(save_state);
	free(save_state_panels);
	save_state = program_state_save();
	save_state_panels = widgets_state_save();
	state_loaded = true;    /* (no high score from training) */
	program_state_free(training_place);
	free(training_place_panels);
	training_place = program_state_save();
	training_place_panels = widgets_state_save();
	training_active = true;
	ship_was_alive = true;
	retry_in = 0;
	training_stage = *(const uint16_t *)cave_work_ram_at(0x101A40u, 2);
	video_latch_sprites();
	draw_picture();
	if (audio)
		audio_restart();
	{
		char text[48];

		snprintf(text, sizeof(text), "TRAINING: %s", training_name(&setup));
		menu_notice(text);
	}
	return true;
}

/* The options' RETRY TRAINING: the training game's place again */
static void menu_retry_training(void)
{
	if (!training_active || !training_place)
		return;
	state_put_back(training_place, training_place_panels);
	retry_in = 0;
	ship_was_alive = true;
	menu_notice("RETRY");
}

/* After each frame of a training game: a miss (the ship down) goes back to the place a moment later; the area
   cleared (its BONUS screen over: the game moves on to the next stage) or the game over, back to the training page */
static bool training_frame(void)
{
	const bool alive = (*(const uint16_t *)cave_work_ram_at(0x102158u, 2) & 0x8000u) != 0;
	const unsigned mode = *(const uint16_t *)cave_work_ram_at(0x1018ECu, 2);

	if (!training_active)
		return false;
	if (retry_in == 0 && (*(const uint16_t *)cave_work_ram_at(0x101A40u, 2) != training_stage || mode != 7u))
	{
		menu_main_menu();    /* (the game reset: the training page next) */
		menu_open_training();
		return true;
	}
	if (!options.training_retry)
		return false;
	if (retry_in > 0)
	{
		if (--retry_in == 0)
		{
			state_action(false);
			menu_notice("RETRY");
			ship_was_alive = true;
		}
		return false;
	}
	if (ship_was_alive && !alive && mode == 7u)
		retry_in = 45;    /* (the ship's explosion seen first) */
	ship_was_alive = alive;
	return false;
}

static void menu_quit(void)
{
	running = 0;
}

#define CHEATS_MAX 16

int main(int argc, char **argv)
{
	const char *cheat_names[CHEATS_MAX];
	bool rom_given = false;
	int cheat_count = 0, list_cheats = 0;
	bool settings_written;
	const char *problem;
	Uint64 next, period;

	/* the settings file, then the command line over it */
	settings_defaults(&options);
	settings_path[0] = '\0';
	for (int index = 1; index + 1 < argc; index++)
		if (!strcmp(argv[index], "--settings"))
			snprintf(settings_path, sizeof(settings_path), "%s", argv[index + 1]);
	if (!settings_path[0])
	{
		char *const base = SDL_GetBasePath();    /* (the program's folder, with its separator) */

		snprintf(settings_path, sizeof(settings_path), "%sdonpachi.ini", base ? base : "");
		SDL_free(base);
	}
	problem = settings_load(&options, settings_path, &settings_written);
	file_options = options;    /* (before the command line's options) */
	if (problem)
	{
		char message[1400];

		snprintf(message, sizeof(message), "%s: %s.\nThose settings keep their defaults.", settings_path, problem);
		fprintf(stderr, "%s\n", message);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "DonPachi settings", message, NULL);
	}
	for (int index = 1; index < argc; index++)
	{
		const char *const value = index + 1 < argc ? argv[index + 1] : NULL;

		if (!strcmp(argv[index], "--settings") && value)
			index++;
		else if (!strcmp(argv[index], "--rom") && value)
		{
			snprintf(options.rom, sizeof(options.rom), "%s", argv[++index]);
			rom_given = true;    /* (the set read again, and extracted again) */
		}
		else if (!strcmp(argv[index], "--cheat") && value && cheat_count < CHEATS_MAX)
			cheat_names[cheat_count++] = argv[++index];
		else if (!strcmp(argv[index], "--cheats") && value)
			snprintf(options.cheats, sizeof(options.cheats), "%s", argv[++index]);
		else if (!strcmp(argv[index], "--list-cheats"))
			list_cheats = 1;
		else if (!strcmp(argv[index], "--no-sound"))
			options.sound = false;
		else if (!strcmp(argv[index], "--no-free-play"))
			options.free_play = false;
		else if (!strcmp(argv[index], "--arcade-timing"))
			options.arcade_timing = true;
		else if (!strcmp(argv[index], "--eeprom") && value)
			snprintf(options.eeprom, sizeof(options.eeprom), "%s", argv[++index]);
		else if (!strcmp(argv[index], "--scale") && value)
			options.scale = atoi(argv[++index]);
	}
	if (options.scale < 1)
		options.scale = 1;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0)
	{
		fprintf(stderr, "SDL: %s\n", SDL_GetError());
		return 1;
	}
	if ((problem = game_data_open(rom_given)) != NULL || (problem = program_start()) != NULL)
	{
		if (problem[0])
			show_problem(problem);
		SDL_Quit();
		return 1;
	}
	if (list_cheats)
	{
		printf("the cheats of %s:\n", options.cheats);
		cheats_list(options.cheats);
		SDL_Quit();
		return 0;
	}
	eeprom_open(options.eeprom[0] ? options.eeprom : NULL);
	if ((problem = hiscore_open(NULL)) != NULL)
		fprintf(stderr, "donpachi.hiscore: %s\n", problem);
	{
		char path[1024];
		char *const base = SDL_GetBasePath();    /* (the program's folder, with its separator) */

		snprintf(path, sizeof(path), "%sdonpachi.achievements", base ? base : "");
		SDL_free(base);
		achievements_load(path);
	}
	cheats_load(options.cheats);    /* (for the menu's cheats page; none, without the file) */
	for (int cheat = 0; cheat < cheat_count; cheat++)
		if ((problem = cheats_enable(options.cheats, cheat_names[cheat])) != NULL)
		{
			show_problem(problem);
			SDL_Quit();
			return 1;
		}
	window = SDL_CreateWindow("DonPachi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		shown_width() * options.scale, shown_height() * options.scale,
		SDL_WINDOW_RESIZABLE | (options.fullscreen && !options.exclusive ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	if (window)
		window_icon_set(window);
	if (window && options.fullscreen && options.exclusive)
		set_fullscreen();    /* (exclusive: at the desktop's mode, not the window's size) */
	renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
	if (!renderer)
		renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
	texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		VIDEO_HEIGHT, VIDEO_WIDTH) : NULL;
	for (int side = 0; side < 2 && texture; side++)
		if (!(panel_textures[side] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
				WIDGET_W, WIDGET_H)))
			texture = NULL;
	if (!texture)
	{
		show_problem(SDL_GetError());
		SDL_Quit();
		return 1;
	}
	menu_apply_video();
	if (options.arcade_timing)
		program_after_vblank = draw_picture;
	{
		menu_host menu = { 0 };

		menu.options = &options;
		menu.file = &file_options;
		menu.path = settings_path;
		menu.pads = pads;
		menu.reset = menu_reset;
		menu.apply_window = menu_apply_window;
		menu.apply_video = menu_apply_video;
		menu.apply_timing = menu_apply_timing;
		menu.quit = menu_quit;
		menu.state = state_action;
		menu.main_menu = menu_main_menu;
		menu.training = menu_training;
		menu.in_training = in_training;
		menu.retry_training = menu_retry_training;
		menu_init(&menu);
		if (options.main_menu)
			menu_open_front();
		if (data_extracted)
			menu_notice("ROM DATA EXTRACTED");    /* (the .zip not needed from now on) */
	}
	if (options.sound)
		audio_open();
	if (audio)
		audio_restart();
	SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

	period = SDL_GetPerformanceFrequency() * CAVE_FRAME_USEC / 1000000u;
	next = SDL_GetPerformanceCounter() + period;
	while (running)
	{
		SDL_Event event;

		if (program_frames() % EEPROM_CHECK_FRAMES == 0)
			eeprom_keep(false);

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				running = 0;
			else if (event.type == SDL_CONTROLLERDEVICEADDED)
				pad_added(event.cdevice.which);
			else if (event.type == SDL_CONTROLLERDEVICEREMOVED)
				pad_removed(event.cdevice.which);
			else if (event.type == SDL_KEYDOWN && !event.key.repeat
					&& (event.key.keysym.sym == SDLK_F11
						|| (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))))
			{
				options.fullscreen = !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
				if (options.fullscreen)
					set_fullscreen();
				else
					SDL_SetWindowFullscreen(window, 0);
			}
			else if (menu_is_open())
				menu_event(&event);
			else if (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
				menu_open();
			else if (event.type == SDL_KEYDOWN && !event.key.repeat)
			{
				const SDL_Keycode key = event.key.keysym.sym;

				if (key == SDLK_ESCAPE)
					menu_open();
			}
		}
		if (menu_just_closed())
		{
			ignore_held_inputs();
			if (audio)
				audio_restart();
		}
		if (!running)
			break;
		if (menu_is_open())
		{
			/* (the game paused: the menu over its last picture, at the board's pace) */
			static uint32_t menu_picture[VIDEO_WIDTH * VIDEO_HEIGHT];

			menu_tick();
			menu_draw(picture, menu_picture);
			show_picture(menu_picture);
			goto pace;
		}
		hotkeys();
		{
			/* fast forward, while its key or button is held: fast_forward_speed of the game's frames in this one,
			   the last one shown, no sound */
			const bool fast = hotkey_down(HOTKEY_FAST_FORWARD);
			const int frames = fast ? options.fast_forward_speed : 1;

			if (fast != fast_forwarding)
			{
				fast_forwarding = fast;
				SDL_SetWindowTitle(window, fast ? "DonPachi (fast forward)" : "DonPachi");
				if (!fast && audio)
					audio_restart();
			}
			{
				const widgets_status status = { options.arcade_timing, options.free_play, cheats_any_on(),
					fast ? frames : 0 };

				widgets_set_status(&status);
			}
			for (int frame = 0; frame < frames; frame++)
			{
				read_keys();
				if (options.free_play)
					free_play_inputs();
				program_run_frame();
				big_hit_frame();
				cheats_apply();
				if (fast)
					cave_sound_drop();
				else
					audio_queue();
				video_latch_sprites();    /* (VBLANK: the 013 takes the sprites) */
				hitbox_record();
				widgets_frame();
				if (cheats_any_on())
					state_loaded = true;
				hiscore_frame(options.save_high_scores, !state_loaded && !training_active);
				if (training_frame())
					break;    /* (the training page is up: the game paused) */
				/* (a new game: its scores are kept again) */
				if (state_loaded && !cheats_any_on() && *(const uint16_t *)cave_work_ram_at(0x1018ECu, 2) == 0xBu)
				{
					state_loaded = false;
					hiscore_state_loaded();    /* (the table as the file has it: none of that game's scores) */
				}
				/* achievements: the arcade game only (not training, cheats or save states) */
				achievements_frame(!state_loaded && !training_active && !cheats_any_on());
				/* (shown on the side panel, never over the game: without the panels, only on the achievements page) */
				if (!widgets_toast_busy())
				{
					const char *const earned = achievements_next_shown();

					if (earned && options.panels)
						widgets_toast(earned, achievements_waiting());
				}
			}
		}
		if (!options.arcade_timing)
			draw_picture();       /* (--arcade-timing drew it as the vblank interrupt returned: program_after_vblank) */
		show_picture(menu_with_notice(with_hitboxes(picture)));

	pace:
		/* (the board's 57.55 frames a second: SDL_Delay may sleep a whole tick of the system's timer longer than
		   asked, up to 15 ms on Windows, so it sleeps only to 2 ms before the frame's time, and the rest is waited
		   out on the counter) */
		{
			const Uint64 frequency = SDL_GetPerformanceFrequency(), margin = frequency * 2u / 1000u;
			Uint64 now = SDL_GetPerformanceCounter();

			if (now + margin < next)
				SDL_Delay((Uint32)((next - now - margin) * 1000u / frequency));
			while ((now = SDL_GetPerformanceCounter()) < next)
				;
			next = (now > next + 4u * period) ? now + period : next + period;
		}
	}
	eeprom_keep(true);
	if (audio)
		SDL_CloseAudioDevice(audio);
	SDL_Quit();
	return 0;
}
