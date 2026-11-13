/**
 *    Name:        main.c
 *    Description: DonPachi in a window, on SDL2: the decompiled program, its picture, its sound, the keyboard
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: donpachi [--settings PATH] [--rom PATH] [--scale N] [--no-sound] [--no-free-play] [--eeprom PATH]
 *                    [--arcade-timing] [--cheats FILE] [--cheat NAME ...] [--list-cheats]
 *      --settings PATH the settings file (default: donpachi.ini beside the program (paths.h); settings.h): the options below
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
 *    Save states: F5 keeps the game as it is now, F8 goes back to it ([hotkeys] save_state, load_state; or the
 *          options menu), in the chosen slot of 5 (F6 / F7, the options' STATE SLOT), in memory until the program
 *          closes. A game that went back to one keeps no high score.
 *    High scores: kept in donpachi.hiscore beside the program (hiscore.h; the options menu's SAVE HIGH SCORES, or
 *          [game] save_high_scores); not saved in a game that had a cheat on or went back to a save state.
 *    Cheats: the options menu's GAME OPTIONS > CHEATS turns them on and off for the run (or --cheat): the port's own
 *    (own_cheats.c: AI PLAYER 2, player 2 flown by the computer (ai_player.c); lives, bombs, invincibility, MAX power,
 *    an endless chain), then the cheat file's, if it is there. GAME OPTIONS > DEBUG TOOLS: the developers' debug mode
 *    and RAM editor, still in the program (own_cheats.c), cheats as well.
 */
#include "cave/cheats.h"
#include "own_cheats.h"
#include "crt.h"
#include "ai_player.h"
#include "game_mode.h"
#include "fast_timing.h"
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
#include "replay.h"
#include "stats.h"
#include "ui_sound.h"
#include "menu.h"
#include "widgets.h"
#include "window_icon.h"
#include "settings.h"
#include "paths.h"

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
static void state_slot_hotkey(int step);
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

static bool big_hit_set;    /* (the big HIT display's flags set by the port, to clear when the option goes off) */

/* The board's power-on self-test, after each reset: the program clears and checks its work RAM, sets up the video
   chips and adds up the whole program ROM byte by byte against its checksum (reset, 03425A), about 247 frames of a
   black screen (4.3 seconds) with its interrupts off. The port checked the ROM set as it loaded it, so the test can't
   fail: its frames are run unseen, at once (all of them, as the board runs them: the game the same, replays too).
   Over once the vblank interrupt counts its first frame (frame_counter, $101260) */
#define BOOT_FRAMES_MOST 400

static bool booting(void)
{
	return program_frames() < BOOT_FRAMES_MOST && *(const uint16_t *)cave_work_ram_at(0x101260u, 2) == 0u;
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
			replay_keep(false);    /* (a game reset part way: not kept) */
			program_reset();
			replay_reset();
			free_play_reset();
			hiscore_reset();
			ai_player_reset();
			big_hit_set = false;
			training_active = false;
			service_held = hotkey == HOTKEY_SERVICE_MENU ? SERVICE_MENU_FRAMES : 0;
		}
		hotkey_was_down[hotkey] = down;
	}
	for (int hotkey = HOTKEY_SAVE_STATE; hotkey <= HOTKEY_SLOT_NEXT; hotkey++)
	{
		const bool down = hotkey_down(hotkey);

		if (down && !hotkey_was_down[hotkey])
		{
			if (hotkey == HOTKEY_SLOT_PREVIOUS || hotkey == HOTKEY_SLOT_NEXT)
				state_slot_hotkey(hotkey == HOTKEY_SLOT_NEXT ? 1 : -1);
			else
				state_action(hotkey == HOTKEY_SAVE_STATE);
		}
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
	const char *data_problem = "--rom given", *rom_problem, *save_problem;

	snprintf(path, sizeof(path), "%s%s", paths_folder(), DATA_FILE);
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
		ui_sound_mix(samples, count);    /* (the port's own: the menus', the achievement pop-up's) */
		if (options.volume < 100)
			for (unsigned index = 0; index < count; index++)
				samples[index] = (int16_t)(samples[index] * options.volume / 100);
		SDL_QueueAudio(audio, samples, (Uint32)(count * sizeof(int16_t)));
	}
}

/* the menu's frame, the game paused: its sounds (ui_sound.h) queued, with silence between them */
static void audio_menu_frame(void)
{
	static uint64_t made;    /* (the output's samples, kept exact from frame to frame) */
	int16_t samples[2048] = { 0 };
	unsigned count;

	if (!audio)
		return;
	made += (uint64_t)CAVE_SOUND_RATE * CAVE_FRAME_USEC;
	count = (unsigned)(made / 1000000u);
	made %= 1000000u;
	if (count > 2048u)
		count = 2048u;
	if (SDL_GetQueuedAudioSize(audio) > audio_bytes(AUDIO_MOST_MS))
		audio_restart();
	ui_sound_mix(samples, count);
	if (options.volume < 100)
		for (unsigned index = 0; index < count; index++)
			samples[index] = (int16_t)(samples[index] * options.volume / 100);
	SDL_QueueAudio(audio, samples, (Uint32)(count * sizeof(int16_t)));
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
		snprintf(eeprom_path, sizeof(eeprom_path), "%sdonpachi.eeprom", paths_folder());
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

/* ---- the picture in the window ([video]: scaling, smoothing, scanlines or the CRT look, rotation) ---- */

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
		{
			/* (the menu up: its chosen entry explained on the right panel) */
			const char *title, *text;

			if (menu_help(&title, &text))
				widgets_draw_help(panel_pixels[1], title, text);
		}
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
	/* the arcade monitor's look (crt.c), when it is on and the renderer can: its own lines, not the scanlines */
	if (options.crt != CRT_OFF && crt_draw(renderer, texture, pixels, unturned,
			options.rotate == ROTATE_RIGHT ? 90 : options.rotate == ROTATE_LEFT ? 270 : 0, options.crt))
	{
		SDL_RenderPresent(renderer);
		return;
	}
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
   and the best chain shown between chains (draw_chain_hud). Cosmetic only. With the option on the bits are kept set
   while a game is played; turned off, they are cleared once, and the command works as on the board again */
#define BIG_HIT_FLAGS 0x101738u

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
	if (replay_playing())
		replay_stop();    /* (the player's own EEPROM back) */
	replay_keep(false);    /* (one waiting was answered before this: see the menu's MAIN MENU) */
	program_reset();
	replay_reset();
	free_play_reset();
	hiscore_reset();
	ai_player_reset();
	big_hit_set = false;
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

/* As the vblank interrupt returns: the debug tools (own_cheats.h), and the picture drawn there with arcade timing */
static void after_vblank(void)
{
	own_cheats_after_vblank();
	if (options.arcade_timing)
		draw_picture();
}

static void menu_apply_timing(void)
{
	if (options.arcade_timing)
		draw_picture();    /* (the picture until the next interrupt draws it) */
}

/* ---- the save states (cave/program.h): STATE_SLOTS of them, in memory, until the program closes ---- */

static program_state *slot_states[STATE_SLOTS];
static void *slot_panels[STATE_SLOTS];    /* (the side panels' run, as it was: widgets.h) */
static int state_slot;             /* the slot save and load use */
#define save_state slot_states[state_slot]
#define save_state_panels slot_panels[state_slot]
static bool state_loaded;          /* this game went back to a save state, or had a cheat on: its scores are not kept
                                      (hiscore.h) */

static void state_put_back(const program_state *state, const void *panels);
static bool slot_is_place[STATE_SLOTS];    /* the slot's state is the training's place (F5 not used since): a retry
                                              from it is a replay's start again */
#define save_state_is_place slot_is_place[state_slot]

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
		save_state_is_place = false;
		{
			char text[32];

			snprintf(text, sizeof(text), "STATE SAVED (SLOT %d)", state_slot + 1);
			menu_notice(text);
		}
		return;
	}
	if (!save_state)
	{
		char text[32];

		snprintf(text, sizeof(text), "SLOT %d IS EMPTY", state_slot + 1);
		menu_notice(text);
		return;
	}
	state_put_back(save_state, save_state_panels);
	{
		char text[32];

		snprintf(text, sizeof(text), "STATE LOADED (SLOT %d)", state_slot + 1);
		menu_notice(text);
	}
}

/* The save states' slot, moved by 'step' (0: as it is): the one now (the menu's STATE SLOT, F6 / F7) */
static int state_slot_step(int step)
{
	state_slot = ((state_slot + step) % STATE_SLOTS + STATE_SLOTS) % STATE_SLOTS;
	return state_slot;
}

static bool state_slot_used(int slot)
{
	return slot >= 0 && slot < STATE_SLOTS && slot_states[slot] != NULL;
}

/* F6 / F7: the slot before or after, told on the screen */
static void state_slot_hotkey(int step)
{
	char text[32];

	state_slot_step(step);
	snprintf(text, sizeof(text), "STATE SLOT %d%s", state_slot + 1, save_state ? "" : " (EMPTY)");
	menu_notice(text);
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
	ai_player_reset();
	replay_taint();    /* (not a replay: the inputs before it are not this game's) */
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
		options.training_power, options.training_max_bombs, options.training_difficulty, options.training_percent };
	const char *problem;

	hiscore_reset();
	free_play_reset();
	service_held = 0;
	replay_taint();    /* (training: run to the place by the port, not a replay) */
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
	save_state_is_place = true;
	replay_begin_training(&setup);    /* (each attempt from the place recorded, to keep as a replay) */
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
	replay_training_retry();
	retry_in = 0;
	ship_was_alive = true;
	menu_notice("RETRY");
}

/* The options' SAVE REPLAY in a training game */
static void menu_save_training_replay(void)
{
	menu_notice(replay_save_training() ? "REPLAY SAVED" : "NO REPLAY TO SAVE");
}

/* After each frame of a training game: a miss (the ship down) goes back to the place a moment later; the area
   cleared (its BONUS screen over: the game moves on to the next stage) or the game over, back to the training page */
static bool training_frame(void)
{
	const bool alive = (*(const uint16_t *)cave_work_ram_at(0x102158u, 2) & 0x8000u) != 0;
	const unsigned mode = game_mode_now();    /* (7: a game played, by one player or both) */

	if (!training_active)
		return false;
	if (retry_in == 0 && (*(const uint16_t *)cave_work_ram_at(0x101A40u, 2) != training_stage || mode != 7u))
	{
		/* (the attempt's replay kept or not first, if it is one: then the training page) */
		replay_hold_training();
		if (replay_waiting())
			menu_ask_save_replay(MENU_AFTER_TRAINING_PAGE);
		else
		{
			menu_main_menu();    /* (the game reset: the training page next) */
			menu_open_training();
		}
		return true;
	}
	if (!options.training_retry)
		return false;
	if (retry_in > 0)
	{
		if (--retry_in == 0)
		{
			state_action(false);
			if (save_state_is_place)
				replay_training_retry();    /* (from the place again: a new attempt) */
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

/* ---- replays (replay.h) ---- */

static bool replay_paused;         /* playback paused: Space or A; Right or the d-pad's right a frame on */
static bool replay_step;

/* The main menu's REPLAYS: the replay played from its game's start (the frames before it run at once, unseen) */
static bool menu_play_replay(const char *file)
{
	const char *problem;

	hiscore_reset();
	if ((problem = replay_play(file)) != NULL)
	{
		fprintf(stderr, "replay %s: %s\n", file, problem);
		menu_main_menu();    /* (the board back to its own start) */
		menu_notice("REPLAY NOT PLAYABLE");
		return false;
	}
	free_play_reset();
	widgets_reset();
	if (replay_playing_training())
		widgets_begin_game();    /* (the panels on the game at once, as for training) */
	state_loaded = true;    /* (no scores, achievements or statistics from it) */
	training_active = false;
	big_hit_set = false;
	replay_paused = replay_step = false;
	video_latch_sprites();
	draw_picture();
	if (audio)
		audio_restart();
	menu_notice("REPLAY  (SPACE: PAUSE)");
	return true;
}

static bool in_replay(void)
{
	return replay_playing();
}

/* The menu's answer to "SAVE THE REPLAY?" */
static void menu_keep_replay(bool keep)
{
	if (replay_keep(keep))
		menu_notice("REPLAY SAVED");
}

/* A frame of the replay being played: false at its end (the replays page up again) */
static bool replay_frame(bool fast)
{
	if (!replay_next(NULL))
	{
		menu_main_menu();    /* (the player's EEPROM back, the board reset) */
		menu_open_replays();
		return false;
	}
	program_run_frame();
	if (!replay_after_playback_frame())
		menu_notice("REPLAY OUT OF STEP");
	if (fast)
		cave_sound_drop();
	else
		audio_queue();
	video_latch_sprites();    /* (VBLANK: the 013 takes the sprites) */
	hitbox_record();
	widgets_frame();
	widgets_take_defeats();
	return true;
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
		snprintf(settings_path, sizeof(settings_path), "%sdonpachi.ini", paths_folder());
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
	{
		/* (the ROM set and the cheat file as the settings name them: from the working folder, else beside the
		   program, as a game started from a desktop's file manager or a launcher may not be started in its folder) */
		char found[sizeof(options.rom)];

		paths_find(options.rom, found, sizeof(found));
		snprintf(options.rom, sizeof(options.rom), "%s", found);
		paths_find(options.cheats, found, sizeof(found));
		snprintf(options.cheats, sizeof(options.cheats), "%s", found);
	}
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

		snprintf(path, sizeof(path), "%sdonpachi.achievements", paths_folder());
		achievements_load(path);
		snprintf(path, sizeof(path), "%sdonpachi.stats", paths_folder());
		stats_load(path);
		snprintf(path, sizeof(path), "%sreplays/", paths_folder());
		replay_folder(path);
	}
	own_cheats_add();               /* (the port's own: there without the cheat file) */
	cheats_load(options.cheats);    /* (for the menu's cheats page: the own ones, then the file's if it is there) */
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
	program_after_vblank = after_vblank;
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
		menu.state_slot = state_slot_step;
		menu.state_used = state_slot_used;
		menu.main_menu = menu_main_menu;
		menu.training = menu_training;
		menu.in_training = in_training;
		menu.retry_training = menu_retry_training;
		menu.play_replay = menu_play_replay;
		menu.in_replay = in_replay;
		menu.keep_replay = menu_keep_replay;
		menu.replay_in_game = replay_in_game;
		menu.hold_replay = replay_hold_unfinished;
		menu.save_training_replay = menu_save_training_replay;
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

	program_set_slowdown(options.slowdown);    /* (the arcade's slowdown, as the settings have it) */
	replay_reset();    /* (the board as it starts: recorded from here) */
	period = SDL_GetPerformanceFrequency() * CAVE_FRAME_USEC / 1000000u;
	next = SDL_GetPerformanceCounter() + period;
	while (running)
	{
		SDL_Event event;

		if (program_frames() % EEPROM_CHECK_FRAMES == 0 && !replay_playing())
			eeprom_keep(false);    /* (a replay's EEPROM is its own: not saved) */

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_RENDER_TARGETS_RESET || event.type == SDL_RENDER_DEVICE_RESET)
				crt_reset();    /* (its textures lost: made again) */
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
			else if (replay_playing() && ((event.type == SDL_KEYDOWN && !event.key.repeat
					&& event.key.keysym.sym == SDLK_SPACE)
					|| (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_A)))
			{
				replay_paused = !replay_paused;    /* (a replay: pause, or on again) */
				menu_notice(replay_paused ? "PAUSED  (RIGHT: A FRAME)" : "PLAYING");
				if (!replay_paused && audio)
					audio_restart();
			}
			else if (replay_playing() && ((event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RIGHT
					|| event.key.keysym.sym == SDLK_PERIOD))
					|| (event.type == SDL_CONTROLLERBUTTONDOWN
						&& event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)))
			{
				if (!replay_paused)
					menu_notice("PAUSED  (RIGHT: A FRAME)");
				replay_paused = true;    /* (a frame on, paused: key repeat steps on while it is held) */
				replay_step = true;
			}
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
			audio_menu_frame();
			menu_draw(picture, menu_picture);
			show_picture(menu_picture);
			goto pace;
		}
		if (!replay_playing())
			hotkeys();    /* (a replay: no reset or save states) */
		if (!replay_playing() && program_slowdown() != options.slowdown)
		{
			/* the slowdown changed in the menu: the frames fall another way from here, so no replay of this run */
			program_set_slowdown(options.slowdown);
			replay_taint();
		}
		{
			/* fast forward, while its key or button is held: fast_forward_speed of the game's frames in this one,
			   the last one shown, no sound */
			const bool fast = hotkey_down(HOTKEY_FAST_FORWARD), boot = booting();
			const int frames = boot ? BOOT_FRAMES_MOST : fast ? options.fast_forward_speed : 1;

			if (fast != fast_forwarding)
			{
				fast_forwarding = fast;
				SDL_SetWindowTitle(window, fast ? "DonPachi (fast forward)" : "DonPachi");
				if (!fast && audio)
					audio_restart();
			}
			{
				const widgets_status status = { options.arcade_timing, options.free_play, cheats_any_on(),
					fast ? options.fast_forward_speed : 0, program_slowdown() };

				widgets_set_status(&status);
			}
			for (int frame = 0; frame < frames; frame++)
			{
				if (boot && !booting())
					break;    /* (the self-test over: the game's first picture from the next frame on) */
				if (replay_playing())
				{
					if (replay_paused && !replay_step)
					{
						audio_menu_frame();    /* (silence meanwhile) */
						break;
					}
					replay_step = false;
					if (!replay_frame(fast || replay_paused))
						break;    /* (its end: the replays page is up) */
					if (replay_paused)
						break;    /* (one frame: stepped) */
					continue;
				}
				read_keys();
				if (own_cheats_ai_on())
					cave_board.in1 = (uint16_t)~ai_player_inputs();    /* (player 2: the computer) */
				if (options.free_play)
					free_play_inputs();
				replay_input(cave_board.in0, cave_board.in1, options.big_hit ? REPLAY_BIG_HIT : 0u);
				program_run_frame();
				big_hit_frame();
				cheats_apply();
				if (cheats_any_on())
					replay_taint();
				if (fast || boot)
					cave_sound_drop();
				else
					audio_queue();
				video_latch_sprites();    /* (VBLANK: the 013 takes the sprites) */
				hitbox_record();
				widgets_frame();
				if (cheats_any_on())
					state_loaded = true;
				hiscore_frame(options.save_high_scores, !state_loaded && !training_active);
				if (hiscore_take_put_back())
				{
					replay_patch(0x101846u, 0x5Au);    /* (the table) */
					replay_patch(0x101782u, 4u);       /* (the HUD's HIGH) */
				}
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
				stats_frame(!state_loaded && !training_active && !cheats_any_on(), widgets_take_defeats());
				/* (shown on the side panel, never over the game: without the panels, only on the achievements page) */
				if (!widgets_toast_busy())
				{
					const char *const earned = achievements_next_shown();

					if (earned && options.panels)
					{
						widgets_toast(earned, achievements_waiting());
						ui_sound_play(UI_SOUND_ACHIEVEMENT);
					}
				}
				/* a game over: its replay kept or not, as the player says (the game paused meanwhile) */
				if (replay_after_frame())
				{
					menu_ask_save_replay(MENU_AFTER_RESUME);
					break;
				}
			}
		}
		if (!options.arcade_timing)
		{
			/* (--arcade-timing drew it as the vblank interrupt returned: program_after_vblank. Here the scroll and
			   the palette as the coming interrupt sets them: fast_timing.h) */
			fast_timing_begin();
			draw_picture();
			fast_timing_end();
		}
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
	if (replay_playing())
		replay_stop();
	eeprom_keep(true);
	stats_save();
	if (audio)
		SDL_CloseAudioDevice(audio);
	SDL_Quit();
	return 0;
}
