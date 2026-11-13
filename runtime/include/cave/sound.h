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
 *    The chips run as the 68000's time goes: given its clock (cave_sound_clock: the port's program counts its cycles),
 *    the chips are brought up to the moment before each command, bank change or status read, as MAME's stream
 *    update does, so a command takes effect where it falls in the frame; cave_vblank_start() runs them to the
 *    frame's end. Without the clock (tools/cref) all of the frame's samples are made at its end, so the commands take
 *    effect from its start (up to a frame early: the port's way before 0.7.0). The status the program reads (only for
 *    the test mode's sound test, and as it waits for its stop command at the start) is the chips' at that moment.
 *
 *    The output as MAME makes it by default (src/emu/sound.cpp, resampler.cpp, audio_effects/filter.cpp): each
 *    chip's samples to 48 kHz by its "LoFi" resampler (a 4-point interpolation, after Yamaha's), mixed at their
 *    gains, through the speaker's high-pass filter at 20 Hz, then to 16 bits.
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

#define CAVE_SOUND_FRAME_MOST 320u    /* a frame's samples of one chip, at most (#1 makes 278) */

typedef struct
{
	cave_oki_voice voice[4];
	int command;          /* the sample of a play command waiting for its second byte, or -1 */
	uint64_t clocks;      /* (what is left over of the chip's clocks from the last frame, for its next sample) */
	uint32_t due;         /* the frame's samples */
	uint32_t made;        /* those made so far */
	int32_t samples[CAVE_SOUND_FRAME_MOST];
} cave_oki;

typedef struct
{
	cave_oki oki[2];
	uint8_t bank[8];      /* the NMK112: chip #0's 4 banks of 64 KB, then #1's */
	bool frame_begun;     /* (the frame's samples counted: 'due') */
	uint64_t frame_began; /* the 68000's time as the frame began (cave_sound_clock's cycles) */
} cave_sound;

/* The program's bus: the M6295s' status and commands (chip 0 or 1), the NMK112's bank registers (0-7) */
uint8_t cave_sound_status(int chip);
void cave_sound_command(int chip, uint8_t data);
void cave_sound_bank(int which, uint8_t data);

/* The chips after the board's reset: silent, no command waiting */
void cave_sound_reset(void);

/* The chips to the end of the board's frame (cave_vblank_start() calls it): the frame's samples go to the output */
void cave_sound_frame(void);

/* The 68000's time, for where the program's commands fall in a frame: 'cycles_now' gives its cycles so far, a frame
   being 'frame_cycles' of them. NULL (as it starts): every command from its frame's start */
void cave_sound_clock(uint64_t (*cycles_now)(void), uint64_t frame_cycles);

/* The samples made so far and not yet taken, at most 'most' of them into 'out': how many. The output keeps about
   0.7 s; what a port does not take in time is dropped, the oldest first */
unsigned cave_sound_take(int16_t *out, unsigned most);

/* Drops the samples not yet taken (a port that skips ahead: its fast forward) */
void cave_sound_drop(void);

#ifdef __cplusplus
}
#endif

#endif
