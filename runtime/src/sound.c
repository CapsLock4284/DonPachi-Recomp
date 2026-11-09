/**
 *    Name:        sound.c
 *    Description: DonPachi's sound (cave/sound.h): the two OKI M6295s, their ADPCM, the NMK112's banks, and the mix
 *                 the ports play
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After MAME's src/devices/sound/okim6295.cpp (BSD-3-Clause, Mirko Buffoni, Aaron Giles): write(), read(),
 *    generate_adpcm(), s_volume_table; src/devices/sound/okiadpcm.cpp (BSD-3-Clause, Andrew Gardner, Aaron Giles):
 *    oki_adpcm_state's clock(), reset(), compute_tables(); src/devices/machine/nmk112.cpp (BSD-3-Clause, Alex W.
 *    Jackson): okibank_w() and its banks, with device_generic_cart_interface::map_non_power_of_two()'s way of
 *    folding the entries past the ROM onto it.
 */
#include "cave/sound.h"
#include "cave/machine.h"
#include "cave/rom_set.h"

#include <math.h>
#include <string.h>

#define FRAME_USEC      CAVE_FRAME_USEC     /* the board's frame: 17376 us (MAME's timer) */
#define OKI_DIVISOR     132u                /* pin 7 high */
#define SAMPLES_MAX     4096u               /* a frame's samples of one chip (#1 makes 278) */
#define OUTPUT_SIZE     32768u              /* the output's ring: a power of 2, about 0.7 s */

static const uint32_t oki_clock[2] = { 4220000u / 4u, 4220000u / 2u };
static const double oki_gain[2] = { 1.60, 1.0 };

/* the ROM regions as cave.cpp has them now ("oki1": atdp.u33; "oki2": atdp.u32 then atdp.u33): rom_set.c keeps
   them after 0x40000 empty bytes, the NMK112's old layout */
#define REGION_SKIP     0x40000u
static const uint32_t region_size[2] = { DONPACHI_OKI1_SIZE - REGION_SKIP, DONPACHI_OKI2_SIZE - REGION_SKIP };

/* MAME's s_volume_table, in 32nds: 0, -3.2, -6, -9.2, -12, -14.5, -18, -20.5, -24 dB, then silent */
static const int volume_table[16] = { 0x20, 0x16, 0x10, 0x0b, 0x08, 0x06, 0x04, 0x03, 0x02, 0, 0, 0, 0, 0, 0, 0 };
static const int index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static int diff_lookup[49 * 16];
static bool tables_made;

static int16_t output[OUTPUT_SIZE];
static uint32_t output_read, output_write;     /* (free running: the ring's places are these & (OUTPUT_SIZE - 1)) */
static uint64_t output_clock;                  /* (what is left over of the output's samples from the last frame) */

/* the resampling, per chip: where the next output sample falls among the chip's samples of this frame (-1: the
   last one of the frame before, 'previous') */
static double resample_position[2] = { -1.0, -1.0 };
static int previous_sample[2];

static void make_tables(void)
{
	static const int nbl2bit[16][4] = {
		{ 1, 0, 0, 0 }, { 1, 0, 0, 1 }, { 1, 0, 1, 0 }, { 1, 0, 1, 1 },
		{ 1, 1, 0, 0 }, { 1, 1, 0, 1 }, { 1, 1, 1, 0 }, { 1, 1, 1, 1 },
		{ -1, 0, 0, 0 }, { -1, 0, 0, 1 }, { -1, 0, 1, 0 }, { -1, 0, 1, 1 },
		{ -1, 1, 0, 0 }, { -1, 1, 0, 1 }, { -1, 1, 1, 0 }, { -1, 1, 1, 1 }
	};

	for (int step = 0; step <= 48; step++)
	{
		const int stepval = (int)floor(16.0 * pow(11.0 / 10.0, (double)step));

		for (int nib = 0; nib < 16; nib++)
			diff_lookup[step * 16 + nib] = nbl2bit[nib][0] *
				(stepval * nbl2bit[nib][1] + stepval / 2 * nbl2bit[nib][2] + stepval / 4 * nbl2bit[nib][3] + stepval / 8);
	}
	tables_made = true;
}

/* ---- the NMK112 ---- */

/* the page of the ROM a bank register's value selects: the ROM's pages, rounded up to a power of 2, are the
   entries; an entry past the ROM folds back onto it (its highest bit dropped) */
static uint32_t page_of(int chip, uint8_t value)
{
	const uint32_t pages = region_size[chip] >> 16;
	uint32_t mask = pages - 1u;

	while (mask & (mask + 1u))
		mask |= mask >> 1;
	uint32_t page = value & mask;
	while (page >= pages)
	{
		uint32_t high = mask + 1u;

		while (!(page & high))
			high >>= 1;
		page &= ~high;
	}
	return page;
}

/* a byte of the chip's 256 KB: four banks of 64 KB; on #1 (paged) the sample table's four quarters (0x000-0x3FF)
   come with the banks too, each from its own bank's page */
static uint8_t oki_byte(int chip, uint32_t address)
{
	const uint8_t *const region = rom_set.oki[chip] + REGION_SKIP;
	const unsigned bank = (address >> 16) & 3u;

	address &= 0x3FFFFu;
	if (chip == 1 && address < 0x400u)
		return region[(page_of(chip, cave_board.sound.bank[4 + (address >> 8)]) << 16) | address];
	return region[(page_of(chip, cave_board.sound.bank[chip * 4 + bank]) << 16) | (address & 0xFFFFu)];
}

void cave_sound_reset(void)
{
	memset(&cave_board.sound, 0, sizeof(cave_board.sound));
	cave_board.sound.oki[0].command = -1;
	cave_board.sound.oki[1].command = -1;
}

void cave_sound_bank(int which, uint8_t data)
{
	cave_board.sound.bank[which & 7] = data;
}

/* ---- the M6295s ---- */

uint8_t cave_sound_status(int chip)
{
	uint8_t result = 0xF0u;

	for (int voice = 0; voice < 4; voice++)
		if (cave_board.sound.oki[chip].voice[voice].playing)
			result |= (uint8_t)(1u << voice);
	return result;
}

void cave_sound_command(int chip, uint8_t data)
{
	cave_oki *const oki = &cave_board.sound.oki[chip];

	if (oki->command != -1)
	{
		/* a play command's second byte: the voices (bits 4-7) and the volume */
		unsigned mask = data >> 4;

		for (int number = 0; number < 4; number++, mask >>= 1)
			if (mask & 1u)
			{
				cave_oki_voice *const voice = &oki->voice[number];
				const uint32_t base = (uint32_t)oki->command * 8u;
				uint32_t start, stop;

				if (voice->playing)
					continue;
				start = (uint32_t)oki_byte(chip, base) << 16 | (uint32_t)oki_byte(chip, base + 1u) << 8
					| oki_byte(chip, base + 2u);
				stop = (uint32_t)oki_byte(chip, base + 3u) << 16 | (uint32_t)oki_byte(chip, base + 4u) << 8
					| oki_byte(chip, base + 5u);
				start &= 0x3FFFFu;
				stop &= 0x3FFFFu;
				if (start < stop)
				{
					voice->playing = true;
					voice->base = start;
					voice->sample = 0;
					voice->count = 2u * (stop - start + 1u);
					voice->signal = 0;
					voice->step = 0;
					voice->volume = volume_table[data & 0x0Fu];
				}
			}
		oki->command = -1;
	}
	else if (data & 0x80u)
		oki->command = data & 0x7F;
	else
	{
		/* stop the voices of bits 3-6 */
		unsigned mask = data >> 3;

		for (int number = 0; number < 4; number++, mask >>= 1)
			if (mask & 1u)
				oki->voice[number].playing = false;
	}
}

/* the chip's next sample, its voices' sum: 2048 * 32 to a voice at full volume */
static int oki_sample(int chip)
{
	cave_oki *const oki = &cave_board.sound.oki[chip];
	int sum = 0;

	for (int number = 0; number < 4; number++)
	{
		cave_oki_voice *const voice = &oki->voice[number];
		int nibble;

		if (!voice->playing)
			continue;
		nibble = (oki_byte(chip, voice->base + voice->sample / 2u) >> (((voice->sample & 1u) << 2) ^ 4u)) & 15;
		voice->signal += diff_lookup[voice->step * 16 + nibble];
		if (voice->signal > 2047)
			voice->signal = 2047;
		else if (voice->signal < -2048)
			voice->signal = -2048;
		voice->step += index_shift[nibble & 7];
		if (voice->step > 48)
			voice->step = 48;
		else if (voice->step < 0)
			voice->step = 0;
		sum += voice->signal * voice->volume;
		if (++voice->sample >= voice->count)
			voice->playing = false;
	}
	return sum;
}

/* ---- the frame ---- */

static void output_put(int16_t sample)
{
	if (output_write - output_read >= OUTPUT_SIZE)
		output_read++;    /* (full: the oldest goes) */
	output[output_write++ & (OUTPUT_SIZE - 1u)] = sample;
}

void cave_sound_frame(void)
{
	static int samples[2][SAMPLES_MAX];
	unsigned made[2], outputs;

	if (!tables_made)
		make_tables();
	if (!rom_set.oki[0] || !rom_set.oki[1])
		return;
	for (int chip = 0; chip < 2; chip++)
	{
		/* the chip's samples in the frame: its clock over 132, kept exact from frame to frame */
		cave_oki *const oki = &cave_board.sound.oki[chip];
		const uint64_t per_sample = (uint64_t)OKI_DIVISOR * 1000000u;

		oki->clocks += (uint64_t)oki_clock[chip] * FRAME_USEC;
		made[chip] = (unsigned)(oki->clocks / per_sample);
		oki->clocks %= per_sample;
		if (made[chip] > SAMPLES_MAX)
			made[chip] = SAMPLES_MAX;
		for (unsigned index = 0; index < made[chip]; index++)
			samples[chip][index] = oki_sample(chip);
	}

	output_clock += (uint64_t)CAVE_SOUND_RATE * FRAME_USEC;
	outputs = (unsigned)(output_clock / 1000000u);
	output_clock %= 1000000u;
	for (unsigned index = 0; index < outputs; index++)
	{
		double mixed = 0.0;

		for (int chip = 0; chip < 2; chip++)
		{
			/* (between the chip's samples on either side: a straight line) */
			const double position = resample_position[chip];
			const int below = (int)floor(position);
			const double fraction = position - below;
			const int last = (int)made[chip] - 1;
			const int a = below < 0 ? previous_sample[chip] : samples[chip][below > last ? last : below];
			const int b = below + 1 < 0 ? previous_sample[chip] : samples[chip][below + 1 > last ? last : below + 1];

			mixed += oki_gain[chip] * (a + (b - a) * fraction) / (2048.0 * 32.0);
			resample_position[chip] += (double)oki_clock[chip] / OKI_DIVISOR / CAVE_SOUND_RATE;
		}
		if (mixed > 1.0)
			mixed = 1.0;
		else if (mixed < -1.0)
			mixed = -1.0;
		output_put((int16_t)lrint(mixed * 32767.0));
	}
	for (int chip = 0; chip < 2; chip++)
	{
		if (made[chip])
			previous_sample[chip] = samples[chip][made[chip] - 1u];
		resample_position[chip] -= made[chip];
		if (resample_position[chip] < -1.0 || resample_position[chip] > 1.0)
			resample_position[chip] = -1.0;    /* (never, but kept from wandering) */
	}
}

unsigned cave_sound_take(int16_t *out, unsigned most)
{
	unsigned count = 0;

	while (count < most && output_read != output_write)
		out[count++] = output[output_read++ & (OUTPUT_SIZE - 1u)];
	return count;
}

void cave_sound_drop(void)
{
	output_read = output_write;
}
