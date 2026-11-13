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
 *    folding the entries past the ROM onto it. The output after src/emu/resampler.cpp (BSD-3-Clause, Olivier
 *    Galibert): audio_resampler_lofi, its interpolation table and its delay, src/emu/audio_effects/filter.cpp
 *    (BSD-3-Clause, Olivier Galibert): the high-pass filter, and src/emu/sound.cpp's mix: MAME's defaults.
 */
#include "cave/sound.h"
#include "cave/machine.h"
#include "cave/rom_set.h"

#include <math.h>
#include <string.h>

#define FRAME_USEC      CAVE_FRAME_USEC     /* the board's frame: 17376 us (MAME's timer) */
#define OKI_DIVISOR     132u                /* pin 7 high */
#define OUTPUT_SIZE     32768u              /* the output's ring: a power of 2, about 0.7 s */
#define HIGH_PASS_HZ    20.0                /* the speaker's filter (audio_effect_filter: on, at 20 Hz, Q 0.7071) */
#define HIGH_PASS_Q     0.7071067f
#define HISTORY         4                   /* the resampler's samples from the frame before */

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

/* the resampling, per chip: where the next output sample falls among the chip's samples of this frame (MAME's
   LoFi takes the 4 before it: the frame before's last ones, 'history'), and its interpolation table */
static double resample_position[2];
static float history[2][HISTORY];
static float lofi_table[2][0x1001];

/* the high-pass filter (a biquad, in floats as MAME's): its coefficients and the last values in and out */
static float hp_b0, hp_b1, hp_b2, hp_a1, hp_a2;
static float hp_x[3], hp_y[3];

/* the 68000's time (cave_sound_clock) */
static uint64_t (*clock_now)(void);
static uint64_t clock_frame;

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

	/* audio_resampler_lofi's interpolation table: f0(t) = (t - t^3) / 6, f1(t) = t + (t^2 - t^3) / 2, f1 past the
	   middle made so that the four add up to 1 */
	for (int i = 1; i != 4096; i++)
	{
		const float p = (float)i / 4096.0f;

		lofi_table[0][i] = (p - p * p * p) / 6;
	}
	for (int i = 1; i != 2049; i++)
	{
		const float p = (float)i / 4096.0f;

		lofi_table[1][i] = p + (p * p - p * p * p) / 2;
	}
	for (int i = 2049; i != 4096; i++)
		lofi_table[1][i] = 1.0f + lofi_table[0][i] + lofi_table[0][4096 - i] - lofi_table[1][4096 - i];
	lofi_table[0][0] = 0.0f;
	lofi_table[0][0x1000] = 0.0f;
	lofi_table[1][0] = 0.0f;
	lofi_table[1][0x1000] = 1.0f;

	/* audio_effect_filter::build_highpass */
	{
		const float rate = (float)CAVE_SOUND_RATE;
		const float k = tanf(3.14159265358979f * (float)HIGH_PASS_HZ / rate), k2 = k * k, q = HIGH_PASS_Q;
		const float d = k2 * q + k + q;

		hp_b0 = q / d;
		hp_b1 = -2 * q / d;
		hp_b2 = hp_b0;
		hp_a1 = 2 * q * (k2 - 1) / d;
		hp_a2 = (k2 * q - k + q) / d;
	}
	tables_made = true;
}

void cave_sound_clock(uint64_t (*cycles_now)(void), uint64_t frame_cycles)
{
	clock_now = cycles_now;
	clock_frame = frame_cycles;
}

/* the frame's samples counted (the chip's clock over 132, kept exact from frame to frame), none made yet */
static void begin_frame(void)
{
	const uint64_t per_sample = (uint64_t)OKI_DIVISOR * 1000000u;

	for (int chip = 0; chip < 2; chip++)
	{
		cave_oki *const oki = &cave_board.sound.oki[chip];

		oki->clocks += (uint64_t)oki_clock[chip] * FRAME_USEC;
		oki->due = (uint32_t)(oki->clocks / per_sample);
		oki->clocks %= per_sample;
		if (oki->due > CAVE_SOUND_FRAME_MOST)
			oki->due = CAVE_SOUND_FRAME_MOST;
		oki->made = 0;
	}
	cave_board.sound.frame_began = clock_now ? clock_now() : 0u;
	cave_board.sound.frame_begun = true;
}

static int oki_sample(int chip);

/* the chips' samples made up to now (their share of the frame's, by the 68000's time), or to the frame's end */
static void catch_up(bool to_the_end)
{
	uint64_t elapsed = 0;

	if (!rom_set.oki[0] || !rom_set.oki[1])
		return;
	if (!tables_made)
		make_tables();
	if (!cave_board.sound.frame_begun)
		begin_frame();
	if (!to_the_end)
	{
		uint64_t now;

		if (!clock_now || !clock_frame)
			return;    /* (no time inside the frame: all of it at the end) */
		now = clock_now();
		elapsed = now > cave_board.sound.frame_began ? now - cave_board.sound.frame_began : 0u;
	}
	for (int chip = 0; chip < 2; chip++)
	{
		cave_oki *const oki = &cave_board.sound.oki[chip];
		const uint32_t want = to_the_end || elapsed >= clock_frame ? oki->due
			: (uint32_t)((uint64_t)oki->due * elapsed / clock_frame);

		while (oki->made < want)
			oki->samples[oki->made++] = oki_sample(chip);
	}
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
	catch_up(false);
	cave_board.sound.bank[which & 7] = data;
}

/* ---- the M6295s ---- */

uint8_t cave_sound_status(int chip)
{
	uint8_t result = 0xF0u;

	catch_up(false);    /* (as okim6295_device::read: the stream brought up to now) */
	for (int voice = 0; voice < 4; voice++)
		if (cave_board.sound.oki[chip].voice[voice].playing)
			result |= (uint8_t)(1u << voice);
	return result;
}

void cave_sound_command(int chip, uint8_t data)
{
	cave_oki *const oki = &cave_board.sound.oki[chip];

	catch_up(false);
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

/* MAME's LoFi resampler: the chip's sample at 'position' (in its samples of this frame, from 0), 4 samples behind
   it (audio_resampler_lofi: "ssample -= 4"), between the 2nd and 3rd of the 4 before it */
static float lofi(int chip, double position)
{
	const cave_oki *const oki = &cave_board.sound.oki[chip];
	const int below = (int)floor(position);
	const unsigned phase = (unsigned)((position - below) * 4096.0) & 0xFFFu;
	float s[4];

	for (int k = 0; k < 4; k++)
	{
		const int index = below - 4 + k;

		s[k] = index < 0 ? history[chip][HISTORY + index]
			: (float)oki->samples[index < (int)oki->made ? index : (int)oki->made - 1] / (2048.0f * 32.0f);
	}
	return -s[0] * lofi_table[0][0x1000 - phase] + s[1] * lofi_table[1][0x1000 - phase] + s[2] * lofi_table[1][phase]
		- s[3] * lofi_table[0][phase];
}

void cave_sound_frame(void)
{
	unsigned outputs;

	if (!rom_set.oki[0] || !rom_set.oki[1])
		return;
	catch_up(true);

	output_clock += (uint64_t)CAVE_SOUND_RATE * FRAME_USEC;
	outputs = (unsigned)(output_clock / 1000000u);
	output_clock %= 1000000u;
	for (unsigned index = 0; index < outputs; index++)
	{
		float mixed = 0.0f;
		int value;

		for (int chip = 0; chip < 2; chip++)
		{
			if (cave_board.sound.oki[chip].made)
				mixed += (float)oki_gain[chip] * lofi(chip, resample_position[chip]);
			resample_position[chip] += (double)oki_clock[chip] / OKI_DIVISOR / CAVE_SOUND_RATE;
		}
		/* the speaker's high-pass (filter.h's history and apply) */
		hp_x[2] = hp_x[1];
		hp_x[1] = hp_x[0];
		hp_x[0] = mixed;
		mixed = hp_b0 * hp_x[0] + hp_b1 * hp_x[1] + hp_b2 * hp_x[2] - hp_a1 * hp_y[0] - hp_a2 * hp_y[1];
		hp_y[2] = hp_y[1];
		hp_y[1] = hp_y[0];
		hp_y[0] = mixed;
		/* (to 16 bits as sound.cpp: the int cut toward 0, then clamped) */
		value = (int)(mixed * 32768.0f);
		output_put((int16_t)(value < -32768 ? -32768 : value > 32767 ? 32767 : value));
	}
	for (int chip = 0; chip < 2; chip++)
	{
		cave_oki *const oki = &cave_board.sound.oki[chip];
		float last[HISTORY];

		/* the last 4 kept for the next frame's first outputs */
		for (int k = 0; k < HISTORY; k++)
		{
			const int index = (int)oki->made - HISTORY + k;

			last[k] = index < 0 ? history[chip][HISTORY + index] : (float)oki->samples[index] / (2048.0f * 32.0f);
		}
		memcpy(history[chip], last, sizeof(last));
		resample_position[chip] -= oki->made;
		if (resample_position[chip] < 0.0 || resample_position[chip] > 2.0)
			resample_position[chip] = 0.0;    /* (never, but kept from wandering) */
	}
	begin_frame();
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
