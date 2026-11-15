/**
 *    Name:        crash_log.c
 *    Description: donpachi.log, and a crash written into it (crash_log.h)
 *    Version:     0.1.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "crash_log.h"

#include "autobomb.h"
#include "cave/machine.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <stdlib.h>
#endif

/* the game's place as it crashed: the board's frame, the stage, the game's mode, autobomb looking ahead */
static void write_place(void)
{
	fprintf(stderr, "  board frame %llu, stage %u, loop %u, game mode %u, autobomb looking ahead: %s\n",
		(unsigned long long)cave_board.frame, *(const uint16_t *)cave_work_ram_at(0x101A40u, 2),
		*(const uint16_t *)cave_work_ram_at(0x101A42u, 2), *(const uint16_t *)cave_work_ram_at(0x1018ECu, 2),
		autobomb_looking() ? "yes" : "no");
	fflush(stderr);
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
	write_place();
	return EXCEPTION_CONTINUE_SEARCH;    /* (Windows then ends the program as before) */
}

void crash_log_start(const char *path)
{
	time_t now = time(NULL);

	if (freopen(path, "w", stderr))
		setvbuf(stderr, NULL, _IONBF, 0);
	fprintf(stderr, "DonPachi log, %s", ctime(&now));
	SetUnhandledExceptionFilter(crashed);
}

#else

static void crashed(int signal_number)
{
	fprintf(stderr, "crashed: signal %d\n", signal_number);
	write_place();
	signal(signal_number, SIG_DFL);
	raise(signal_number);
}

void crash_log_start(const char *path)
{
	(void)path;
	signal(SIGSEGV, crashed);
	signal(SIGBUS, crashed);
	signal(SIGFPE, crashed);
	signal(SIGILL, crashed);
	signal(SIGABRT, crashed);
}

#endif
