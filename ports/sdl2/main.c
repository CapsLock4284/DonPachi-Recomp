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
 *      --rom    the ROM set: MAME's donpachi.zip, or a folder holding it or its files (default: rom)
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
 *          the home default), Start start; View is kept for the options menu. Keys and gamepads work together.
 *          F3 resets the game, F4 resets it into the test mode (the service menu: the service switch held for you),
 *          F11 or Alt+Enter full screen, Escape quits.
 *    Debug keys (DONPACHI_DEBUG_KEYS, below): V autofire for player 1 (W for player 2), Tab turns
 *          fast forward on and off.
 */
#include "cave/cheats.h"
#include "cave/free_play.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/video.h"

#include "settings.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the settings: donpachi.ini, then the command line (settings.h) */
static settings options;

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

/* A hotkey pressed (not held from before): the reset, or the reset into the test mode */
static void hotkeys(void)
{
	for (int hotkey = 0; hotkey < HOTKEYS; hotkey++)
	{
		const bool down = hotkey_down(hotkey);

		if (down && !hotkey_was_down[hotkey])
		{
			program_reset();
			free_play_reset();
			service_held = hotkey == HOTKEY_SERVICE_MENU ? SERVICE_MENU_FRAMES : 0;
		}
		hotkey_was_down[hotkey] = down;
	}
}

/* The inputs from the keys and the gamepads held: active low */
static void read_keys(void)
{
	const Uint8 *const keys = SDL_GetKeyboardState(NULL);
	uint16_t pressed[2];

	for (int player = 0; player < 2; player++)
	{
		pressed[player] = pad_inputs(player);
		for (int action = 0; action < ACTIONS; action++)
			for (int binding = 0; binding < BINDINGS_MAX; binding++)
				if (options.keys[player][action][binding] != SDL_SCANCODE_UNKNOWN
						&& keys[options.keys[player][action][binding]])
					pressed[player] |= settings_action_bits[action];
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

/* ==== DEBUG KEYS: begin ===============================================================================================
   Autofire and fast forward, for testing only. Neither is the arcade's: the game's own rapid fire setting (the test
   mode's) is what the finished port uses. To take them out, set DONPACHI_DEBUG_KEYS to 0, or delete everything between
   "DEBUG KEYS: begin" and "DEBUG KEYS: end" here and in main(): nothing else uses them.
     autofire:     while V (player 1) or W (player 2) is held, the shot button is pressed DEBUG_AUTOFIRE_ON frames, then
                   released DEBUG_AUTOFIRE_OFF frames (taps, not a hold: holding the shot gives the laser). C and Q
                   stay button 3, the game's own rapid fire when the test mode sets "C BUTTON FULL-AUTO".
     fast forward: Tab turns it on and off (the window's title says when it is on); while on, the game runs as fast as
                   it can, drawn one frame in DEBUG_FAST_DRAW */
#ifndef DONPACHI_DEBUG_KEYS
#define DONPACHI_DEBUG_KEYS 1
#endif
#if DONPACHI_DEBUG_KEYS
#define DEBUG_AUTOFIRE_ON   2
#define DEBUG_AUTOFIRE_OFF  2
#define DEBUG_FAST_DRAW     8

static const SDL_Scancode debug_autofire_keys[2] = { SDL_SCANCODE_V, SDL_SCANCODE_W };
static unsigned debug_autofire_frame[2];

/* The inputs (active low) with the autofire keys' taps */
static void debug_autofire(void)
{
	const Uint8 *const keys = SDL_GetKeyboardState(NULL);
	uint16_t *const ports[2] = { &cave_board.in0, &cave_board.in1 };

	for (int player = 0; player < 2; player++)
	{
		if (!keys[debug_autofire_keys[player]])
		{
			debug_autofire_frame[player] = 0;
			continue;
		}
		if (debug_autofire_frame[player] < DEBUG_AUTOFIRE_ON)
			*ports[player] &= (uint16_t)~CAVE_BUTTON1;
		debug_autofire_frame[player] = (debug_autofire_frame[player] + 1u) % (DEBUG_AUTOFIRE_ON + DEBUG_AUTOFIRE_OFF);
	}
}

static int debug_fast_forward;

/* Tab pressed: fast forward on, or off again */
static void debug_toggle_fast_forward(SDL_Window *window)
{
	debug_fast_forward = !debug_fast_forward;
	if (!debug_fast_forward && audio)
		audio_restart();
	SDL_SetWindowTitle(window, debug_fast_forward ? "DonPachi (fast forward)" : "DonPachi");
}
#endif
/* ==== DEBUG KEYS: end ================================================================================================= */

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

static void show_picture(void)
{
	SDL_UpdateTexture(texture, NULL, picture, VIDEO_HEIGHT * (int)sizeof(uint32_t));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

#define CHEATS_MAX 16

int main(int argc, char **argv)
{
	const char *cheat_names[CHEATS_MAX];
	char settings_path[1024];
	int cheat_count = 0, list_cheats = 0;
	bool settings_written;
	const char *problem;
	SDL_Window *window;
	Uint64 next, period;
	int running = 1;
#if DONPACHI_DEBUG_KEYS    /* ---- DEBUG KEYS: begin ---- */
	unsigned debug_fast_frames = 0;
#endif                     /* ---- DEBUG KEYS: end ---- */

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
	if ((problem = settings_load(&options, settings_path, &settings_written)) != NULL)
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
			snprintf(options.rom, sizeof(options.rom), "%s", argv[++index]);
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
	if ((problem = rom_set_load(options.rom)) != NULL || (problem = program_start()) != NULL)
	{
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
	for (int cheat = 0; cheat < cheat_count; cheat++)
		if ((problem = cheats_enable(options.cheats, cheat_names[cheat])) != NULL)
		{
			show_problem(problem);
			SDL_Quit();
			return 1;
		}
	window = SDL_CreateWindow("DonPachi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		VIDEO_HEIGHT * options.scale, VIDEO_WIDTH * options.scale,
		SDL_WINDOW_RESIZABLE | (options.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
	if (!renderer)
		renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
	texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		VIDEO_HEIGHT, VIDEO_WIDTH) : NULL;
	if (!texture)
	{
		show_problem(SDL_GetError());
		SDL_Quit();
		return 1;
	}
	SDL_RenderSetLogicalSize(renderer, VIDEO_HEIGHT, VIDEO_WIDTH);
	if (options.arcade_timing)
		program_after_vblank = draw_picture;
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
			else if (event.type == SDL_KEYDOWN && !event.key.repeat)
			{
				const SDL_Keycode key = event.key.keysym.sym;

				if (key == SDLK_ESCAPE)
					running = 0;
#if DONPACHI_DEBUG_KEYS    /* ---- DEBUG KEYS: begin ---- */
				else if (key == SDLK_TAB)
					debug_toggle_fast_forward(window);
#endif                     /* ---- DEBUG KEYS: end ---- */
				else if (key == SDLK_F11 || (key == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)))
					SDL_SetWindowFullscreen(window,
						(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
			}
		}
		hotkeys();
		read_keys();
		if (options.free_play)
			free_play_inputs();
#if DONPACHI_DEBUG_KEYS    /* ---- DEBUG KEYS: begin ---- */
		debug_autofire();
		if (debug_fast_forward)
		{
			program_run_frame();
			cheats_apply();
			cave_sound_drop();    /* (no sound while fast forwarding) */
			video_latch_sprites();
			if (++debug_fast_frames % DEBUG_FAST_DRAW == 0)
			{
				if (!options.arcade_timing)
					draw_picture();
				show_picture();
			}
			next = SDL_GetPerformanceCounter() + period;    /* (normal speed again from when it is turned off) */
			continue;
		}
#endif                     /* ---- DEBUG KEYS: end ---- */
		program_run_frame();
		cheats_apply();
		audio_queue();
		video_latch_sprites();    /* (VBLANK: the 013 takes the sprites) */
		if (!options.arcade_timing)
			draw_picture();       /* (--arcade-timing drew it as the vblank interrupt returned: program_after_vblank) */
		show_picture();

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
