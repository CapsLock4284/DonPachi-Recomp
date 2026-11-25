/**
 *    Name:        crash_log.h
 *    Description: donpachi.log beside the program: what the program writes to stderr (problems, the 68000 stopping)
 *                 kept in a file on Windows, where a windowed program has no console; and a crash written there as
 *                 it happens (the exception, where in the program, the game's frame, stage and mode), with the
 *                 inputs since the board's reset kept in a file of their own, to play the run again up to the crash
 *    Version:     0.2.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 *    The run's inputs: the game is the same from a reset with the same EEPROM and the same inputs at each frame (the
 *    board's time is the program's own count), so the inputs since the reset are the run. Kept in memory (6 bytes a
 *    frame: about 1.2 MB an hour), and written only when the program stops or crashes, to
 *    donpachi_crash_DATE_TIME.inp beside the program: "DPCINP02", the frames (a 32-bit count, little endian), the
 *    slowdown setting, the game's version (0 the US one, 1 the Japanese one), 1 when the high score table was put
 *    back after the boot, 1 when the VBLANK comes early (program_set_vblank_lead; always 0 in a "DPCINP01" file, from
 *    before 25-11-2026), the EEPROM as the run began (128 bytes, big endian words), the high score
 *    table put back (HISCORE_TABLE_SIZE bytes: hiscore.h), then each frame's IN0, IN1 (as the board reads them: active
 *    low, after the port's autofire, free play, autobomb and AI player) and flags (CRASH_LOG_BIG_HIT), 16 bits each,
 *    little endian. tests/harness/replay_crash plays it again. A run that went back to a save state, had a cheat on,
 *    was a training game or a replay, or had its slowdown setting changed part way is not the same from its inputs:
 *    the log says so. The log of the run before is kept as donpachi_previous.log.
 */
#ifndef CRASH_LOG_H
#define CRASH_LOG_H

#include <stdbool.h>

#define CRASH_LOG_BIG_HIT  1u    /* the frame's flags: the big HIT display on (the port's write after the frame) */

/* From the program's start: stderr to 'path' (Windows; elsewhere stderr stays as it is), the log before kept, and the
   crash handlers ('folder': where the inputs' file goes, ending in its separator) */
void crash_log_start(const char *path, const char *folder);

/* The board was reset (or started): the inputs kept from the next frame on, with the EEPROM as it is then.
   'slowdown' and 'version' as the run has them */
void crash_log_reset(bool slowdown, int version);

/* The high score table was put back after the boot (hiscore_take_put_back): its HISCORE_TABLE_SIZE bytes */
void crash_log_table(const unsigned char *table);

/* The slowdown setting changed: the run's own if no frame has run since the reset, else not replayable */
void crash_log_slowdown(bool slowdown);

/* Before each frame the board runs: its inputs (cave_board.in0, in1 as set) and its flags (CRASH_LOG_BIG_HIT) */
void crash_log_frame(unsigned flags);

/* The run is no longer its inputs alone: 'why' (a save state put back, a cheat on), told in the log at a crash */
void crash_log_not_replayable(const char *why);

/* The program ends as it should: no crash to write at the exit */
void crash_log_finish(void);

#endif
