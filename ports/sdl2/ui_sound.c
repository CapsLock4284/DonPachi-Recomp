/**
 *    Name:        ui_sound.c
 *    Description: the port's own sounds, borrowed from the game's (ui_sound.h)
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    The ADPCM decoding after MAME's src/devices/sound/okiadpcm.cpp (BSD-3-Clause, Andrew Gardner, Aaron Giles), as
 *    runtime/src/sound.c has it.
 */
#include "ui_sound.h"

#include "cave/rom_set.h"
#include "cave/sound.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define REGION_SKIP   0x40000u                           /* (rom_set.c's: the region after the NMK112's empty bytes) */
#define CHIP_RATE     (4220000.0 / 2.0 / 132.0)          /* the effects chip's samples a second: 15984.8 */
#define MAX_SAMPLES   (CAVE_SOUND_RATE * 2u)             /* the longest: 2 seconds */

/* Where each sound is in the effects chip's region, its length (nibbles: 2 to a byte) and the volume the game plays
   it at (in 32nds) */
static const struct
{
	uint32_t at, nibbles;
	int volume;
} sources[UI_SOUNDS] = {
	{ 0x00104Cu, 3562u, 6 },     /* the ship select's cursor */
	{ 0x006E2Bu, 12990u, 11 },   /* the ship select's choice */
	{ 0x00878Au, 3008u, 6 },     /* a short one of the ship select's (its ship's name begins with it) */
	{ 0x03313Du, 9570u, 8 },     /* the power-up item's chime */
};

static int16_t *decoded[UI_SOUNDS];
static unsigned lengths[UI_SOUNDS];
static const int16_t *playing;
static unsigned playing_left;

static const int index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static int diff(int step, int nibble)
{
	static int lookup[49 * 16];
	static bool made;

	if (!made)
	{
		for (int s = 0; s <= 48; s++)
		{
			const int value = (int)floor(16.0 * pow(11.0 / 10.0, (double)s));

			for (int n = 0; n < 16; n++)
				lookup[s * 16 + n] = (n & 8 ? -1 : 1)
					* (value * (n >> 2 & 1) + value / 2 * (n >> 1 & 1) + value / 4 * (n & 1) + value / 8);
		}
		made = true;
	}
	return lookup[step * 16 + nibble];
}

/* the sound decoded to 48 kHz, once */
static bool decode(ui_sound sound)
{
	const uint8_t *const region = rom_set.oki[1] ? rom_set.oki[1] + REGION_SKIP : NULL;
	const uint32_t size = DONPACHI_OKI2_SIZE - REGION_SKIP;
	uint32_t nibbles = sources[sound].nibbles;
	float *chip;
	int signal = 0, step = 0;
	unsigned count;

	if (decoded[sound])
		return true;
	if (!region)
		return false;
	if (sources[sound].at + nibbles / 2u > size)
		nibbles = 2u * (size - sources[sound].at);
	chip = malloc(sizeof(float) * (nibbles + 1u));
	if (!chip)
		return false;
	for (uint32_t index = 0; index < nibbles; index++)
	{
		const int nibble = (region[sources[sound].at + index / 2u] >> ((index & 1u) ? 0 : 4)) & 15;

		signal += diff(step, nibble);
		signal = signal > 2047 ? 2047 : signal < -2048 ? -2048 : signal;
		step += index_shift[nibble & 7];
		step = step > 48 ? 48 : step < 0 ? 0 : step;
		chip[index] = (float)(signal * sources[sound].volume) / (2048.0f * 32.0f);
	}
	chip[nibbles] = 0.0f;
	count = (unsigned)(nibbles * (double)CAVE_SOUND_RATE / CHIP_RATE);
	if (count > MAX_SAMPLES)
		count = MAX_SAMPLES;
	decoded[sound] = malloc(sizeof(int16_t) * (count ? count : 1u));
	if (!decoded[sound])
	{
		free(chip);
		return false;
	}
	for (unsigned index = 0; index < count; index++)
	{
		/* (between the chip's samples on either side: a straight line, as the runtime mixes) */
		const double position = index * CHIP_RATE / CAVE_SOUND_RATE;
		const unsigned below = (unsigned)position;
		const double fraction = position - below, a = chip[below], b = chip[below + 1u < nibbles ? below + 1u : below];

		decoded[sound][index] = (int16_t)lrint((a + (b - a) * fraction) * 32767.0);
	}
	lengths[sound] = count;
	free(chip);
	return true;
}

void ui_sound_play(ui_sound sound)
{
	if (sound < 0 || sound >= UI_SOUNDS || !decode(sound))
		return;
	playing = decoded[sound];
	playing_left = lengths[sound];
}

void ui_sound_mix(int16_t *samples, unsigned count)
{
	for (unsigned index = 0; index < count && playing_left; index++, playing_left--)
	{
		const int sum = samples[index] + *playing++;

		samples[index] = (int16_t)(sum > 32767 ? 32767 : sum < -32768 ? -32768 : sum);
	}
}

int ui_sound_playing(void)
{
	return playing_left > 0;
}
