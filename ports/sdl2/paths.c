/**
 *    Name:        paths.c
 *    Description: where the player's files are kept (paths.h)
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "paths.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

static char folder[1024];
static bool found;

/* a file can be made in 'path' (a folder, with its separator) */
static bool can_write(const char *path)
{
	char test[1100];
	FILE *file;

	snprintf(test, sizeof(test), "%s.donpachi_write_test", path);
	if (!(file = fopen(test, "wb")))
		return false;
	fclose(file);
	remove(test);
	return true;
}

const char *paths_folder(void)
{
	if (!found)
	{
		char *const base = SDL_GetBasePath();    /* (the program's folder, with its separator) */

		found = true;
		if (base && can_write(base))
			snprintf(folder, sizeof(folder), "%s", base);
		else
		{
			char *const own = SDL_GetPrefPath("", "DonPachi");    /* (made if it isn't there) */

			snprintf(folder, sizeof(folder), "%s", own ? own : base ? base : "");
			if (own)
				fprintf(stderr, "the program's folder can't be written: the game's files are kept in %s\n", own);
			SDL_free(own);
		}
		SDL_free(base);
	}
	return folder;
}

static bool exists(const char *path)
{
	struct stat info;

	return stat(path, &info) == 0;
}

static bool is_full(const char *path)
{
	return path[0] == '/' || path[0] == '\\' || (path[0] && path[1] == ':');
}

void paths_find(const char *path, char *out, size_t size)
{
	snprintf(out, size, "%s", path);
	if (!path[0] || is_full(path) || exists(path))
		return;
	{
		char *const base = SDL_GetBasePath();
		char tried[1100];

		snprintf(tried, sizeof(tried), "%s%s", paths_folder(), path);
		if (exists(tried))
			snprintf(out, size, "%s", tried);
		else if (base)
		{
			snprintf(tried, sizeof(tried), "%s%s", base, path);
			if (exists(tried))
				snprintf(out, size, "%s", tried);
		}
		SDL_free(base);
	}
}
