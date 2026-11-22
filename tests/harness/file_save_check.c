/* file_save_check: the port's files written by file_save's helper (ports/sdl2/file_save.h): a file saved, a newer copy
   of it saved before the first is written (the newer one's bytes in the file), a text file built in memory, and how
   long a save keeps the frame (the copy only, after the first, which starts the helper). Then flushed: each file read back. PASS or FAIL each. Writes
   file_save_check_*.tmp where it runs (deleted after) */
#include "file_save.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void verdict(const char *what, int pass)
{
	printf("%-60s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static int file_is(const char *path, const char *expected)
{
	char read[256] = "";
	FILE *const file = fopen(path, "rb");
	size_t got;

	if (!file)
		return 0;
	got = fread(read, 1, sizeof(read) - 1, file);
	fclose(file);
	read[got] = '\0';
	return !strcmp(read, expected);
}

int main(int argc, char **argv)
{
	file_text text = { 0 };
	Uint64 began, most = 0;
	char big[4096];    /* (the statistics file is under 2 KB) */

	(void)argc;
	(void)argv;
	memset(big, 'x', sizeof(big));
	file_save("file_save_check_big.tmp", big, sizeof(big));    /* (the first: the helper started) */
	for (int i = 0; i < 50; i++)
	{
		began = SDL_GetPerformanceCounter();
		file_save("file_save_check_big.tmp", big, sizeof(big));
		began = SDL_GetPerformanceCounter() - began;
		if (began > most)
			most = began;
	}
	file_save("file_save_check_a.tmp", "first", 5);
	file_save("file_save_check_a.tmp", "second", 6);
	file_text_add(&text, "[stats]\n");
	file_text_add(&text, "%s = %lu\n", "games", 12lu);
	file_text_save(&text, "file_save_check_b.tmp");
	file_save_flush();
	verdict("a file saved, then a newer copy: the newer one's bytes", file_is("file_save_check_a.tmp", "second"));
	verdict("a text file built in memory", file_is("file_save_check_b.tmp", "[stats]\ngames = 12\n"));
	{
		FILE *const file = fopen("file_save_check_big.tmp", "rb");
		long size = 0;

		if (file)
		{
			fseek(file, 0, SEEK_END);
			size = ftell(file);
			fclose(file);
		}
		verdict("4 KB saved 51 times: written whole", size == (long)sizeof(big));
	}
	printf("the longest a save kept the caller: %.3f ms\n", (double)most * 1000.0 / (double)SDL_GetPerformanceFrequency());
	verdict("a save keeps the caller under 1 ms", (double)most * 1000.0 / (double)SDL_GetPerformanceFrequency() < 1.0);
	remove("file_save_check_a.tmp");
	remove("file_save_check_b.tmp");
	remove("file_save_check_big.tmp");
	return failures != 0;
}
