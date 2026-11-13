/**
 *    Name:        program.h
 *    Description: the decompiled program as a port runs it: a frame at a time
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The program runs on a stack of its own (host/tasks.h). program_run_frame goes on with it until it next
 *    waits for a frame (frame_wait: the profile's FRAME_WAIT, 002EDC in wait_for_frame) or STOPs; VBLANK then
 *    starts, and the port draws the frame, reads its inputs and keeps the time before it runs the next one.
 */
#ifndef CAVE_PROGRAM_H
#define CAVE_PROGRAM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The board and the program as the 68000's reset leaves them, the ROM set loaded (cave/rom_set.h): NULL, or
   what is wrong */
const char *program_start(void);

/* Runs the program to its next frame: VBLANK has started when it returns */
void program_run_frame(void);

/* The board's reset, as its reset button: the next program_run_frame starts the program again from its reset vector,
   with the RAMs, the video and the sound chips cleared and the EEPROM kept (the test mode's settings). The frames go
   on being counted. A player holding the service switch from then on (as the board's: about 140 frames) gets the
   test mode */
void program_reset(void);

/* The arcade's slowdown (a program decompiled with its timing: game/support.h's m68k_block): on, a frame whose work
   takes longer than the board's frame (278,016 68000 cycles) misses its VBLANK and the game waits a frame more, as
   on the board; off, every frame's work fits. On by default. program_timed: the program counts its cycles (false:
   it was decompiled without them, and never slows down). program_slow_frames: the VBLANKs that came while the
   program was still working, since the start */
void program_set_slowdown(bool on);
bool program_slowdown(void);
bool program_timed(void);
uint64_t program_slow_frames(void);
uint64_t program_frame_cycles(void);    /* the 68000 cycles the program counted from its last wait to this one */
/* The frame program_run_frame just ran was a frame of slowdown: its VBLANK came while the program was still working,
   so the work RAM is half way through a frame (the picture is the last one again). What watches the game's memory
   for changes (achievements, statistics, the side panels, the high scores) skips it and looks again at the next */
bool program_frame_lagged(void);

/* Called, when set, at every round of the program's wait for the next frame (with slowdown; with it off, once, as
   the program comes to its wait): the test tools compare the board with MAME's 68000 at the first round of each
   wait the program has made ready (tools/cref/cref_timed --waits) */
extern void (*program_at_wait)(void);

/* Called, when set, as the program's vblank interrupt (level 1) returns: its handler has flipped the sprite banks and
   set the layers' scroll and the palette for the frame. MAME draws DonPachi's screen there (90 us after the
   interrupt): a port that draws its picture there, with the sprites latched when VBLANK started, shows what the board
   shows. A port that draws when program_run_frame returns shows the frame a frame sooner (all but the scroll and the
   palette, which the next interrupt sets) */
extern void (*program_after_vblank)(void);

/* Save states, kept in memory (not in files: what they hold is only good while the program runs): the whole board
   (cave_machine: its RAMs, the video, the sound chips, the EEPROM), the 68000's status, and the program's own stack
   where it waits for its frame. program_state_save takes one between frames (program_run_frame has returned): NULL
   if there is no memory. program_state_load puts one back; the next program_run_frame goes on from it, as the
   frame after the one it was saved at. The port draws the picture again (video_latch_sprites, video_draw) and drops
   the sound it had queued */
typedef struct program_state program_state;

program_state *program_state_save(void);
void program_state_load(const program_state *state);
void program_state_free(program_state *state);

/* The frames so far, and the interrupts the program took */
uint64_t program_frames(void);
uint32_t program_interrupts(void);

#ifdef __cplusplus
}
#endif

#endif
