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

/* Called, when set, as the program's vblank interrupt (level 1) returns: its handler has flipped the sprite banks and
   set the layers' scroll and the palette for the frame. MAME draws DonPachi's screen there (90 us after the
   interrupt): a port that draws its picture there, with the sprites latched when VBLANK started, shows what the board
   shows. A port that draws when program_run_frame returns shows the frame a frame sooner (all but the scroll and the
   palette, which the next interrupt sets) */
extern void (*program_after_vblank)(void);

/* The frames so far, and the interrupts the program took */
uint64_t program_frames(void);
uint32_t program_interrupts(void);

#ifdef __cplusplus
}
#endif

#endif
