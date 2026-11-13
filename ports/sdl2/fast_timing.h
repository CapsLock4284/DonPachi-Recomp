/**
 *    Name:        fast_timing.h
 *    Description: the picture with fast timing, as the board shows it: drawn as the program's frame ends, before the
 *                 vblank interrupt has set the layers' scroll and copied the palette for it, so those are put in as
 *                 the interrupt is about to (fast_timing.c)
 *    Version:     0.1.0 (draft)
 *    Created:     13-11-2026
 *
 *    license:BSD-3-Clause
 */
#ifndef FAST_TIMING_H
#define FAST_TIMING_H

/* The layers' scroll and the palette as the coming vblank interrupt sets them (DonPachi's level_1_interrupt), for
   drawing the picture now; fast_timing_end() puts the board's own back. Nothing changes when the program has not
   made its frame ready (a slowed frame: the interrupt then leaves them) */
void fast_timing_begin(void);
void fast_timing_end(void);

#endif
