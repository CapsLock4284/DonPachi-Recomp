/**
 *    Name:        file_save.c
 *    Description: the port's files written by a helper thread (file_save.h)
 *    Version:     0.1.0 (draft)
 *    Created:     21-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "file_save.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the file written now, as before (the helper's own work, and FILE_SAVE_SYNC's) */
static void write_now(const char *path, const void *data, size_t size)
{
	FILE *const file = fopen(path, "wb");

	if (!file)
	{
		fprintf(stderr, "%s: could not be written\n", path);
		return;
	}
	if (fwrite(data, 1, size, file) != size)
		fprintf(stderr, "%s: could not be written\n", path);
	fclose(file);
}

#ifdef FILE_SAVE_SYNC

void file_save(const char *path, const void *data, size_t size)
{
	write_now(path, data, size);
}

void file_save_flush(void)
{
}

#else

#include <SDL.h>

#define WAITING_MOST 8    /* files waiting at once (the port has 5) */

typedef struct
{
	char path[1024];
	void *data;
	size_t size;
} waiting_file;

static waiting_file waiting[WAITING_MOST];
static int waiting_count;
static SDL_mutex *lock;
static SDL_cond *wake, *idle;
static SDL_Thread *helper;
static int busy;           /* the helper is writing one */
static int stopping;

static int helper_main(void *unused)
{
	(void)unused;
	SDL_LockMutex(lock);
	for (;;)
	{
		waiting_file file;

		while (waiting_count == 0 && !stopping)
			SDL_CondWait(wake, lock);
		if (waiting_count == 0)
			break;    /* (stopping, nothing left) */
		file = waiting[0];
		memmove(waiting, waiting + 1, sizeof(waiting[0]) * (size_t)(waiting_count - 1));
		waiting_count--;
		busy = 1;
		SDL_UnlockMutex(lock);
		write_now(file.path, file.data, file.size);
		free(file.data);
		SDL_LockMutex(lock);
		busy = 0;
		SDL_CondBroadcast(idle);
	}
	SDL_CondBroadcast(idle);
	SDL_UnlockMutex(lock);
	return 0;
}

static int started(void)
{
	if (helper)
		return 1;
	if (!lock && !(lock = SDL_CreateMutex()))
		return 0;
	if (!wake && !(wake = SDL_CreateCond()))
		return 0;
	if (!idle && !(idle = SDL_CreateCond()))
		return 0;
	stopping = 0;
	helper = SDL_CreateThread(helper_main, "file_save", NULL);
	return helper != NULL;
}

void file_save(const char *path, const void *data, size_t size)
{
	void *copy;

	if (!started() || !(copy = malloc(size ? size : 1)))
	{
		write_now(path, data, size);    /* (no helper or no memory: at once, as before) */
		return;
	}
	memcpy(copy, data, size);
	SDL_LockMutex(lock);
	{
		int at;

		/* (a newer copy of a file still waiting takes its place) */
		for (at = 0; at < waiting_count; at++)
			if (!strcmp(waiting[at].path, path))
				break;
		if (at < waiting_count)
			free(waiting[at].data);
		else if (waiting_count < WAITING_MOST)
			at = waiting_count++;
		else
		{
			SDL_UnlockMutex(lock);
			write_now(path, data, size);    /* (too many waiting: at once) */
			free(copy);
			return;
		}
		snprintf(waiting[at].path, sizeof(waiting[at].path), "%s", path);
		waiting[at].data = copy;
		waiting[at].size = size;
	}
	SDL_CondSignal(wake);
	SDL_UnlockMutex(lock);
}

void file_save_flush(void)
{
	if (!helper)
		return;
	SDL_LockMutex(lock);
	stopping = 1;
	SDL_CondSignal(wake);
	SDL_UnlockMutex(lock);
	SDL_WaitThread(helper, NULL);    /* (it writes what is left, then ends) */
	helper = NULL;
}

#endif

void file_text_add(file_text *text, const char *format, ...)
{
	va_list args;
	int length;

	va_start(args, format);
	length = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (length < 0)
		return;
	if (text->size + (size_t)length + 1u > text->room)
	{
		size_t room = text->room ? text->room : 1024u;
		char *grown;

		while (text->size + (size_t)length + 1u > room)
			room *= 2u;
		if (!(grown = realloc(text->data, room)))
			return;
		text->data = grown;
		text->room = room;
	}
	va_start(args, format);
	vsnprintf(text->data + text->size, text->room - text->size, format, args);
	va_end(args);
	text->size += (size_t)length;
}

void file_text_save(file_text *text, const char *path)
{
	if (text->data)
		file_save(path, text->data, text->size);
	free(text->data);
	text->data = NULL;
	text->size = text->room = 0;
}
