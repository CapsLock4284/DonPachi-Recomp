/**
 *    Name:        main.c
 *    Description: DonPachi in a window, on SDL2: the decompiled program, its picture, the keyboard (no sound yet)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: donpachi [--rom PATH] [--scale N] [--cheat NAME ...]
 *      --rom    the ROM set: MAME's donpachi.zip, or a folder holding it or its files (default: rom)
 *      --scale  the window's size, in times the picture's 240 x 320 (default 3)
 *      --cheat NAME    turns on the cheat of the cheat file whose name holds NAME ("p1 infinite lives"), as many
 *                      times as there are cheats; NAME=VALUE picks a value where the cheat asks for one
 *      --cheats FILE   the cheat file, MAME's format (default: cheats/donpachi.xml)
 *      --list-cheats   lists the cheat file's cheats
 *
 *    Keys: arrows move, Z shot, X bomb, C button 3, 1 start, 5 coin; player 2: R F D G, A S Q, 2, 6;
 *          F2 the test mode, F11 or Alt+Enter full screen, Escape quits.
 */
#include "cave/cheats.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/video.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	SDL_Scancode key;
	int player;
	uint16_t bit;
} key_binding;

static const key_binding bindings[] = {
	{ SDL_SCANCODE_UP, 0, CAVE_UP }, { SDL_SCANCODE_DOWN, 0, CAVE_DOWN },
	{ SDL_SCANCODE_LEFT, 0, CAVE_LEFT }, { SDL_SCANCODE_RIGHT, 0, CAVE_RIGHT },
	{ SDL_SCANCODE_Z, 0, CAVE_BUTTON1 }, { SDL_SCANCODE_X, 0, CAVE_BUTTON2 }, { SDL_SCANCODE_C, 0, CAVE_BUTTON3 },
	{ SDL_SCANCODE_1, 0, CAVE_START }, { SDL_SCANCODE_5, 0, CAVE_COIN }, { SDL_SCANCODE_F2, 0, CAVE_SERVICE },
	{ SDL_SCANCODE_R, 1, CAVE_UP }, { SDL_SCANCODE_F, 1, CAVE_DOWN },
	{ SDL_SCANCODE_D, 1, CAVE_LEFT }, { SDL_SCANCODE_G, 1, CAVE_RIGHT },
	{ SDL_SCANCODE_A, 1, CAVE_BUTTON1 }, { SDL_SCANCODE_S, 1, CAVE_BUTTON2 }, { SDL_SCANCODE_Q, 1, CAVE_BUTTON3 },
	{ SDL_SCANCODE_2, 1, CAVE_START }, { SDL_SCANCODE_6, 1, CAVE_COIN },
};

/* The inputs from the keys held: active low */
static void read_keys(void)
{
	const Uint8 *const keys = SDL_GetKeyboardState(NULL);
	uint16_t pressed[2] = { 0, 0 };

	for (size_t index = 0; index < sizeof(bindings) / sizeof(bindings[0]); index++)
		if (keys[bindings[index].key])
			pressed[bindings[index].player] |= bindings[index].bit;
	cave_board.in0 = (uint16_t)~pressed[0];
	cave_board.in1 = (uint16_t)~pressed[1];
}

static void show_problem(const char *problem)
{
	fprintf(stderr, "The game does not start: %s\n", problem);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DonPachi", problem, NULL);
}

#define CHEATS_MAX 16

int main(int argc, char **argv)
{
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	const char *rom = "rom";
	const char *cheat_file = "cheats/donpachi.xml", *cheat_names[CHEATS_MAX];
	int cheat_count = 0, list_cheats = 0;
	int scale = 3;
	const char *problem;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	Uint64 next, period;
	int running = 1;

	for (int index = 1; index < argc; index++)
	{
		if (!strcmp(argv[index], "--rom") && index + 1 < argc)
			rom = argv[++index];
		else if (!strcmp(argv[index], "--cheat") && index + 1 < argc && cheat_count < CHEATS_MAX)
			cheat_names[cheat_count++] = argv[++index];
		else if (!strcmp(argv[index], "--cheats") && index + 1 < argc)
			cheat_file = argv[++index];
		else if (!strcmp(argv[index], "--list-cheats"))
			list_cheats = 1;
		else if (!strcmp(argv[index], "--scale") && index + 1 < argc)
			scale = atoi(argv[++index]);
	}
	if (scale < 1)
		scale = 1;
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
	{
		fprintf(stderr, "SDL: %s\n", SDL_GetError());
		return 1;
	}
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		show_problem(problem);
		SDL_Quit();
		return 1;
	}
	if (list_cheats)
	{
		printf("the cheats of %s:\n", cheat_file);
		cheats_list(cheat_file);
		SDL_Quit();
		return 0;
	}
	for (int cheat = 0; cheat < cheat_count; cheat++)
		if ((problem = cheats_enable(cheat_file, cheat_names[cheat])) != NULL)
		{
			show_problem(problem);
			SDL_Quit();
			return 1;
		}
	window = SDL_CreateWindow("DonPachi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		VIDEO_HEIGHT * scale, VIDEO_WIDTH * scale, SDL_WINDOW_RESIZABLE);
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
	SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

	period = SDL_GetPerformanceFrequency() * CAVE_FRAME_USEC / 1000000u;
	next = SDL_GetPerformanceCounter() + period;
	while (running)
	{
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				running = 0;
			else if (event.type == SDL_KEYDOWN && !event.key.repeat)
			{
				const SDL_Keycode key = event.key.keysym.sym;

				if (key == SDLK_ESCAPE)
					running = 0;
				else if (key == SDLK_F11 || (key == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)))
					SDL_SetWindowFullscreen(window,
						(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
			}
		}
		read_keys();
		program_run_frame();
		cheats_apply();
		video_latch_sprites();
		video_draw(frame);
		video_rotated(frame, picture);
		SDL_UpdateTexture(texture, NULL, picture, VIDEO_HEIGHT * (int)sizeof(uint32_t));
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);

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
	SDL_Quit();
	return 0;
}
