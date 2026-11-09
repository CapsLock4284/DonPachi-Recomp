/**
 *    Name:        sound.h
 *    Description: DonPachi's sound: two OKI M6295 ADPCM chips (the music on #0, the effects on #1) behind an NMK112,
 *                 which banks the sample ROMs in 64 KB pages; the 68000 drives all three itself
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After MAME's src/devices/sound/okim6295.cpp (BSD-3-Clause, Mirko Buffoni, Aaron Giles),
 *    src/devices/sound/okiadpcm.cpp (BSD-3-Clause, Andrew Gardner, Aaron Giles), src/devices/machine/nmk112.cpp
 *    (BSD-3-Clause, Alex W. Jackson) and the donpachi machine of src/mame/cave/cave.cpp (BSD-3-Clause, Luca Elia
 *    and others): #0 at 4.220 MHz / 4, #1 at 4.220 MHz / 2, both with pin 7 high (a sample every 132 clocks), #0
 *    mixed at 1.60 and #1 at 1.0; the NMK112 pages #1's sample table, not #0's.
 *
 *    The board's frame makes the sound: cave_vblank_start() runs the chips through the frame just ended (so the
 *    program's commands take effect at the frame's end, and the voices it asks about are as they were at its start).
 *    The decompiled program has no time inside a frame; the reference (tools/cref) runs the same way, so the two
 *    still compare.
 */
#ifndef CAVE_SOUND_H
#define CAVE_SOUND_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAVE_SOUND_RATE     48000u    /* what the ports play: 16-bit mono */

typedef struct
{
	bool playing;
	uint32_t base;        /* the sample's start in the chip's 256 KB */
	uint32_t sample;      /* the nibble it is at */
	uint32_t count;       /* its nibbles */
	int volume;           /* 0..32 (MAME's table, in 32nds) */
	int signal, step;     /* the ADPCM decoder */
} cave_oki_voice;

typedef struct
{
	cave_oki_voice voice[4];
	int command;          /* the sample of a play command waiting for its second byte, or -1 */
	uint64_t clocks;      /* (what is left over of the chip's clocks from the last frame, for its next sample) */
} cave_oki;

typedef struct
{
	cave_oki oki[2];
	uint8_t bank[8];      /* the NMK112: chip #0's 4 banks of 64 KB, then #1's */
} cave_sound;

/* The program's bus: the M6295s' status and commands (chip 0 or 1), the NMK112's bank registers (0-7) */
uint8_t cave_sound_status(int chip);
void cave_sound_command(int chip, uint8_t data);
void cave_sound_bank(int which, uint8_t data);

/* The chips after the board's reset: silent, no command waiting */
void cave_sound_reset(void);

/* The chips through one frame of the board (cave_vblank_start() calls it): the frame's samples go to the output */
void cave_sound_frame(void);

/* The samples made so far and not yet taken, at most 'most' of them into 'out': how many. The output keeps about
   0.7 s; what a port does not take in time is dropped, the oldest first */
unsigned cave_sound_take(int16_t *out, unsigned most);

/* Drops the samples not yet taken (a port that skips ahead: its fast forward) */
void cave_sound_drop(void);

#ifdef __cplusplus
}
#endif

#endif
