/**
 *    Name:        crash_log.c
 *    Description: donpachi.log, a crash written into it, and the run's inputs kept for it (crash_log.h)
 *    Version:     0.2.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "crash_log.h"

#include "autobomb.h"
#include "hiscore.h"
#include "cave/machine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

#define MOST_FRAMES (8u * 3600u * 58u)    /* the inputs kept for at most 8 hours from a reset (10 MB) */
#define WORDS 3u                          /* a frame's: IN0, IN1, flags */

static char folder[1024];
static bool finished, written, ran;    /* (ran: the board has run a frame: a stop before is no crash) */

static struct
{
	uint16_t *frames;           /* IN0, IN1, flags of each frame since the reset */
	uint32_t count, room;
	uint16_t eeprom[CAVE_EEPROM_WORDS];
	bool eeprom_taken;          /* (the EEPROM as the first frame after the reset began) */
	uint8_t slowdown, version, has_table;
	uint8_t table[HISCORE_TABLE_SIZE];    /* the high score table the port put back after the boot */
	char not_replayable[96];    /* why the run is not its inputs alone, or "" */
} run;

void crash_log_reset(bool slowdown, int version)
{
	run.has_table = 0;
	run.count = 0;
	run.eeprom_taken = false;
	run.slowdown = slowdown;
	run.version = (uint8_t)version;
	run.not_replayable[0] = '\0';
}

void crash_log_table(const unsigned char *table)
{
	if (!table)
		return;
	memcpy(run.table, table, sizeof(run.table));
	run.has_table = 1;
}

void crash_log_slowdown(bool slowdown)
{
	if (run.count == 0)
		run.slowdown = slowdown;    /* (changed before the run's first frame: the run's own) */
	else if (run.slowdown != slowdown)
		crash_log_not_replayable("the slowdown setting changed");
}

void crash_log_frame(unsigned flags)
{
	ran = true;
	if (!run.eeprom_taken)
	{
		memcpy(run.eeprom, cave_board.eeprom.words, sizeof(run.eeprom));
		run.eeprom_taken = true;
	}
	if (run.count >= MOST_FRAMES)
	{
		crash_log_not_replayable("more than 8 hours from the reset");
		return;
	}
	if (run.count >= run.room)
	{
		const uint32_t room = run.room ? run.room * 2u : 65536u;
		uint16_t *const frames = realloc(run.frames, (size_t)room * WORDS * sizeof(uint16_t));

		if (!frames)
		{
			crash_log_not_replayable("no memory for the inputs");
			return;
		}
		run.frames = frames;
		run.room = room;
	}
	run.frames[run.count * WORDS] = cave_board.in0;
	run.frames[run.count * WORDS + 1u] = cave_board.in1;
	run.frames[run.count * WORDS + 2u] = (uint16_t)flags;
	run.count++;
}

void crash_log_not_replayable(const char *why)
{
	if (!run.not_replayable[0])
		snprintf(run.not_replayable, sizeof(run.not_replayable), "%s", why);
}

void crash_log_finish(void)
{
	finished = true;
}

static void put32(FILE *file, uint32_t value)
{
	for (int byte = 0; byte < 4; byte++)
		fputc((int)(value >> (8 * byte) & 0xFFu), file);
}

/* The game's place, the run's inputs in their file and where in the log; then the player told (once) */
static void write_crash(void)
{
	char path[1200], name[64];
	const time_t now = time(NULL);
	FILE *file;

	if (written)
		return;
	written = true;
	fprintf(stderr, "  board frame %llu, %lu frames since the reset, stage %u, loop %u, game mode %u, "
		"autobomb looking ahead: %s\n", (unsigned long long)cave_board.frame, (unsigned long)run.count,
		*(const uint16_t *)cave_work_ram_at(0x101A40u, 2), *(const uint16_t *)cave_work_ram_at(0x101A42u, 2),
		*(const uint16_t *)cave_work_ram_at(0x1018ECu, 2), autobomb_looking() ? "yes" : "no");
	strftime(name, sizeof(name), "donpachi_crash_%Y%m%d_%H%M%S.inp", localtime(&now));
	snprintf(path, sizeof(path), "%s%s", folder, name);
	if (run.eeprom_taken && (file = fopen(path, "wb")) != NULL)
	{
		fwrite("DPCINP01", 1, 8, file);
		put32(file, run.count);
		fputc(run.slowdown, file);
		fputc(run.version, file);
		fputc(run.has_table, file);
		fputc(0, file);
		for (unsigned word = 0; word < CAVE_EEPROM_WORDS; word++)
		{
			fputc(run.eeprom[word] >> 8, file);    /* (big endian) */
			fputc(run.eeprom[word] & 0xFF, file);
		}
		fwrite(run.table, 1, sizeof(run.table), file);
		for (uint32_t index = 0; index < run.count * WORDS; index++)
		{
			fputc(run.frames[index] & 0xFF, file);
			fputc(run.frames[index] >> 8, file);
		}
		fclose(file);
		fprintf(stderr, "  the inputs since the reset: %s%s%s%s\n", name,
			run.not_replayable[0] ? " (not replayable: " : "", run.not_replayable,
			run.not_replayable[0] ? ")" : "");
	}
	else
		name[0] = '\0';
	fflush(stderr);
#ifdef _WIN32
	{
		char text[600];

		if (name[0])
			snprintf(text, sizeof(text), "DonPachi stopped.\n\nWhat happened is in donpachi.log, and the game's "
				"inputs up to it in %s, beside the program. Sending both lets the problem be played again and found.",
				name);
		else
			snprintf(text, sizeof(text), "DonPachi stopped.\n\nWhat happened is in donpachi.log, beside the program. "
				"Sending it helps the problem be found.");
		MessageBoxA(NULL, text, "DonPachi", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
	}
#endif
}

/* At the exit: a stop of the program's own (the 68000 stopped: exit(3)) is a crash too */
static void at_exit(void)
{
	if (!finished && ran)
	{
		fputs("stopped\n", stderr);
		write_crash();
	}
}

#ifdef _WIN32

static LONG WINAPI crashed(EXCEPTION_POINTERS *info)
{
	const void *const at = info->ExceptionRecord->ExceptionAddress;
	HMODULE module = NULL;

	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)at, &module);
	fprintf(stderr, "crashed: exception %08lX at %p (the program's +%llX)\n",
		(unsigned long)info->ExceptionRecord->ExceptionCode, at,
		module ? (unsigned long long)((const char *)at - (const char *)module) : 0ull);
	write_crash();
	return EXCEPTION_CONTINUE_SEARCH;    /* (Windows then ends the program as before) */
}

void crash_log_start(const char *path, const char *log_folder)
{
	time_t now = time(NULL);
	char previous[1200];

	snprintf(folder, sizeof(folder), "%s", log_folder);
	snprintf(previous, sizeof(previous), "%sdonpachi_previous.log", log_folder);
	MoveFileExA(path, previous, MOVEFILE_REPLACE_EXISTING);    /* (the run before's log kept) */
	if (freopen(path, "w", stderr))
		setvbuf(stderr, NULL, _IONBF, 0);
	fprintf(stderr, "DonPachi log, %s", ctime(&now));
	SetUnhandledExceptionFilter(crashed);
	atexit(at_exit);
}

#else

static void crashed(int signal_number)
{
	fprintf(stderr, "crashed: signal %d\n", signal_number);
	write_crash();
	signal(signal_number, SIG_DFL);
	raise(signal_number);
}

void crash_log_start(const char *path, const char *log_folder)
{
	(void)path;
	snprintf(folder, sizeof(folder), "%s", log_folder);
	signal(SIGSEGV, crashed);
	signal(SIGBUS, crashed);
	signal(SIGFPE, crashed);
	signal(SIGILL, crashed);
	signal(SIGABRT, crashed);
	atexit(at_exit);
}

#endif
