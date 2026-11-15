/**
 *    Name:        crash_log.h
 *    Description: donpachi.log beside the program: what the program writes to stderr (problems, the 68000 stopping)
 *                 kept in a file on Windows, where a windowed program has no console; and a crash written there as
 *                 it happens (the exception, where in the program, the game's frame, stage and mode)
 *    Version:     0.1.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 */
#ifndef CRASH_LOG_H
#define CRASH_LOG_H

/* From the program's start: stderr to 'path' (Windows; elsewhere stderr stays as it is), and the crash handler */
void crash_log_start(const char *path);

#endif
