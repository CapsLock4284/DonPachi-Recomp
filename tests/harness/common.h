/* What the test tools share: the game's memory, the inputs, and pictures saved as PPM (any image viewer opens them;
   convert to PNG with e.g. ImageMagick). The ROM set: rom/donpachi.zip, or the DONPACHI_ROM environment variable. */
#ifndef HARNESS_COMMON_H
#define HARNESS_COMMON_H

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline unsigned ram_word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

static inline unsigned ram_byte(uint32_t address)
{
	return *cave_work_ram_byte(address);
}

/* the ROM set read and the board started; exits on a problem */
static inline void start_game(void)
{
	const char *const rom = getenv("DONPACHI_ROM") ? getenv("DONPACHI_ROM") : "rom/donpachi.zip";
	const char *problem;

	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		fprintf(stderr, "%s\n", problem);
		exit(1);
	}
}

/* a picture of 'w' x 'h' 0x00RRGGBB pixels as a PPM file */
static inline void save_ppm(const char *path, const uint32_t *pixels, int w, int h)
{
	FILE *const file = fopen(path, "wb");

	if (!file)
		return;
	fprintf(file, "P6 %d %d 255\n", w, h);
	for (int i = 0; i < w * h; i++)
	{
		fputc(pixels[i] >> 16 & 255, file);
		fputc(pixels[i] >> 8 & 255, file);
		fputc(pixels[i] & 255, file);
	}
	fclose(file);
}

/* the inputs of a simple player: a coin and Start in the attract mode, the first ship on its select screen, then
   sweeping left and right while firing (tapping, with a held laser now and then); 'shield' keeps the ship's respawn
   shield on (no deaths). Returns player 1's inputs for frame 'frame' (active high: CAVE_UP ... ) */
static inline unsigned autoplayer(long frame, int shield)
{
	static long select_frames;
	const unsigned mode = ram_word(0x1018ECu);
	unsigned held = 0;

	if (mode == 0xBu)
	{
		if (++select_frames > 60 && (select_frames / 8) % 2)
			held = CAVE_BUTTON1;
	}
	else if (mode == 7u)
	{
		const int x = (int16_t)ram_word(0x102158u + 6u) >> 6;
		int target = 40 + (int)(frame / 3 % 160);

		if ((frame / 480) & 1)
			target = 200 - (int)(frame / 3 % 160);
		if (shield)
			*cave_work_ram_byte(0x102196u) = 2;
		if (x < target - 4)
			held |= CAVE_RIGHT;
		else if (x > target + 4)
			held |= CAVE_LEFT;
		held |= (frame / 40) % 3 == 0 ? CAVE_BUTTON1 : ((frame / 2) % 2 ? CAVE_BUTTON1 : 0u);
	}
	else if (frame > 400)
	{
		const long beat = frame % 40;

		if (beat < 5)
			held = CAVE_COIN;
		else if (beat >= 20 && beat < 25)
			held = CAVE_START;
	}
	return held;
}

#endif
