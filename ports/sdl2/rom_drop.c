/**
 *    Name:        rom_drop.c
 *    Description: the first run's window when there is no game data and no ROM set (rom_drop.h)
 *    Version:     0.1.0 (draft)
 *    Created:     10-11-2026
 *
 *    license:BSD-3-Clause
 *    The font is this program's own: 5 x 7 capitals, digits and a few signs.
 */
#include "rom_drop.h"

#include "cave/rom_set.h"

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_W    640
#define WINDOW_H    400

#define BACKGROUND  0x0C0C1Cu
#define GOLD        0xF0C040u
#define WHITE       0xF0F0F0u
#define GREY        0x9090A8u
#define RED         0xFF4848u
#define GREEN       0x60E070u

typedef struct
{
	char ch;
	const char *rows[7];
} glyph;

static const glyph font[] = {
	{ 'A', { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
	{ 'B', { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." } },
	{ 'C', { ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###." } },
	{ 'D', { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." } },
	{ 'E', { "#####", "#....", "#....", "####.", "#....", "#....", "#####" } },
	{ 'F', { "#####", "#....", "#....", "####.", "#....", "#....", "#...." } },
	{ 'G', { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####" } },
	{ 'H', { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } },
	{ 'I', { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." } },
	{ 'J', { "..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.." } },
	{ 'K', { "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#" } },
	{ 'L', { "#....", "#....", "#....", "#....", "#....", "#....", "#####" } },
	{ 'M', { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" } },
	{ 'N', { "#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#" } },
	{ 'O', { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
	{ 'P', { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." } },
	{ 'Q', { ".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#" } },
	{ 'R', { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" } },
	{ 'S', { ".####", "#....", "#....", ".###.", "....#", "....#", "####." } },
	{ 'T', { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." } },
	{ 'U', { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." } },
	{ 'V', { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." } },
	{ 'W', { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#." } },
	{ 'X', { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" } },
	{ 'Y', { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." } },
	{ 'Z', { "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####" } },
	{ '0', { ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###." } },
	{ '1', { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." } },
	{ '2', { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" } },
	{ '3', { "#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###." } },
	{ '4', { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." } },
	{ '5', { "#####", "#....", "####.", "....#", "....#", "#...#", ".###." } },
	{ '6', { "..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###." } },
	{ '7', { "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..." } },
	{ '8', { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." } },
	{ '9', { ".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.." } },
	{ '.', { ".....", ".....", ".....", ".....", ".....", ".##..", ".##.." } },
	{ ',', { ".....", ".....", ".....", ".....", ".##..", "..#..", ".#..." } },
	{ ':', { ".....", ".##..", ".##..", ".....", ".##..", ".##..", "....." } },
	{ '\'', { "..#..", "..#..", ".#...", ".....", ".....", ".....", "....." } },
	{ '-', { ".....", ".....", ".....", "#####", ".....", ".....", "....." } },
	{ '(', { "...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#." } },
	{ ')', { ".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..." } },
	{ '/', { "....#", "....#", "...#.", "..#..", ".#...", "#....", "#...." } },
	{ '!', { "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.." } },
	{ '?', { ".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.." } },
	{ '_', { ".....", ".....", ".....", ".....", ".....", ".....", "#####" } },
};

static const glyph *glyph_of(char ch)
{
	ch = (char)toupper((unsigned char)ch);
	for (size_t index = 0; index < sizeof(font) / sizeof(font[0]); index++)
		if (font[index].ch == ch)
			return &font[index];
	return NULL;    /* (a space, or a sign it has not: left blank) */
}

static void set_colour(SDL_Renderer *renderer, uint32_t colour)
{
	SDL_SetRenderDrawColor(renderer, colour >> 16 & 255u, colour >> 8 & 255u, colour & 255u, 255);
}

/* 'text' centred at 'y', each of its pixels 'scale' x 'scale' (a character 6 pixels wide, with its space) */
static void text_centred(SDL_Renderer *renderer, int y, const char *text, int scale, uint32_t colour)
{
	const int width = (int)strlen(text) * 6 * scale - scale;
	int x = (WINDOW_W - width) / 2;

	set_colour(renderer, colour);
	for (const char *at = text; *at; at++, x += 6 * scale)
	{
		const glyph *const shape = glyph_of(*at);

		if (!shape)
			continue;
		for (int row = 0; row < 7; row++)
			for (int column = 0; column < 5; column++)
				if (shape->rows[row][column] == '#')
				{
					const SDL_Rect pixel = { x + column * scale, y + row * scale, scale, scale };

					SDL_RenderFillRect(renderer, &pixel);
				}
	}
}

/* 'text' cut to what fits the window at 'scale', with "..." where it is cut */
static void text_fitted(SDL_Renderer *renderer, int y, const char *text, int scale, uint32_t colour)
{
	const size_t most = (size_t)((WINDOW_W - 32) / (6 * scale));
	char line[160];

	if (strlen(text) <= most || most < 4u || most >= sizeof(line))
	{
		text_centred(renderer, y, text, scale, colour);
		return;
	}
	memcpy(line, text, most - 3u);
	memcpy(line + most - 3u, "...", 4u);
	text_centred(renderer, y, line, scale, colour);
}

static void draw(SDL_Renderer *renderer, const char *status, uint32_t status_colour, const char *why)
{
	set_colour(renderer, BACKGROUND);
	SDL_RenderClear(renderer);
	text_centred(renderer, 70 + 4, "DONPACHI", 8, 0x301000u);    /* (its shadow) */
	text_centred(renderer, 70, "DONPACHI", 8, GOLD);
	text_centred(renderer, 170, "DROP THE ARCADE ROM SET HERE", 3, WHITE);
	text_centred(renderer, 210, "DONPACHI.ZIP: MAME'S SET, US VERSION", 2, GREY);
	text_centred(renderer, 232, "THE GAME TAKES ITS DATA FROM IT, ONCE.", 2, GREY);
	if (status)
		text_fitted(renderer, 290, status, 2, status_colour);
	else if (why)
		text_fitted(renderer, 360, why, 1, 0x606078u);
	SDL_RenderPresent(renderer);
}

bool rom_drop_screen(const char *data_path, const char *why, bool *saved)
{
	SDL_Window *const window = SDL_CreateWindow("DonPachi: the ROM set", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, 0);
	SDL_Renderer *renderer;
	char status[200] = "";
	uint32_t status_colour = WHITE;
	bool loaded = false, running = true;

	*saved = false;
	if (!window)
		return false;
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer)
	{
		SDL_DestroyWindow(window);
		return false;
	}
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	while (running && !loaded)
	{
		SDL_Event event;

		draw(renderer, status[0] ? status : NULL, status_colour, why);
		if (!SDL_WaitEventTimeout(&event, 250))
			continue;
		if (event.type == SDL_QUIT
				|| (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
				|| (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
			running = false;
		else if (event.type == SDL_DROPFILE)
		{
			const char *problem;

			draw(renderer, "READING...", WHITE, NULL);
			if ((problem = rom_set_load(event.drop.file)) == NULL)
			{
				const char *const save_problem = rom_set_save_data(data_path);

				if (save_problem)
					fprintf(stderr, "the game's data was not saved: %s\n", save_problem);
				*saved = save_problem == NULL;
				loaded = true;
				draw(renderer, "FOUND IT! STARTING...", GREEN, NULL);
				SDL_Delay(700);
			}
			else
			{
				fprintf(stderr, "%s: %s\n", event.drop.file, problem);
				/* the files the set needs are matched by their CRC32: another game's, or another version's */
				snprintf(status, sizeof(status), "%s", strstr(problem, " has no ")
					? "NOT THE DONPACHI SET (US VERSION). TRY ANOTHER." : "THAT ISN'T A .ZIP OR A FOLDER OF THE SET.");
				status_colour = RED;
			}
			SDL_free(event.drop.file);
		}
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	return loaded;
}
