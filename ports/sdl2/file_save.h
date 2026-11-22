/**
 *    Name:        file_save.h
 *    Description: the port's files (statistics, achievements, high scores, the stages' bests) written by a helper
 *                 thread, so the game's frame never waits for the disk
 *    Version:     0.1.0 (draft)
 *    Created:     21-11-2026
 *
 *    license:BSD-3-Clause
 *    Found in the owner's FPS log: the statistics saved each minute of play took 12-18 ms of the frame on his PC, once
 *    120 ms (the file's write waiting for Windows: a virus scanner, a drive waking), a hitch each time. Now the file's
 *    bytes are copied and the frame goes on; the helper writes them. A newer copy for the same file replaces one still
 *    waiting. FILE_SAVE_SYNC defined (the test tools without SDL): written at once, as before.
 */
#ifndef FILE_SAVE_H
#define FILE_SAVE_H

#include <stddef.h>

/* 'size' bytes of 'data' for the file 'path' (both copied): written soon */
void file_save(const char *path, const void *data, size_t size);

/* Every file still waiting written now (the program ending), and the helper stopped */
void file_save_flush(void);

/* A text file built in memory, then given to file_save */
typedef struct
{
	char *data;
	size_t size, room;
} file_text;

void file_text_add(file_text *text, const char *format, ...);

/* Saved (file_save) and its memory let go */
void file_text_save(file_text *text, const char *path);

#endif
