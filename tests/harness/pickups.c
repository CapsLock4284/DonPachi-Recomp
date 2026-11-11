/* pickups AREA: the training run to AREA's start at the lowest power (MAX=1: at MAX), then a bot that fights and
   chases the items to the area's boss; logs each power change (the power 0x102178, the half step 0x102194, the MAX
   flags) with the scroll. SPRITES=1 also logs each item as it appears and goes (the item records from 0x106114:
   sprite 0361 a power-up, 0379 a bomb); DUMP=1 each new item object's words. The counts behind ON PACE */
#include "common.h"
#include "training.h"

static bool on_screen_record(uint32_t at)
{
	const int hp = (int16_t)ram_word(at + 0x18u), x = (int16_t)ram_word(at + 8u), y = (int16_t)ram_word(at + 6u);

	return (ram_word(at) & 0xA000u) == 0xA000u && hp >= 0 && y >= 0 && y < 0x5000 && x >= 0 && x < 0x3C00;
}

int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), 0, 0, getenv("MAX") ? TRAINING_POWER_MAX : 0, false, 0 };
	const bool dump = getenv("DUMP") != NULL;
	static const int boss[5] = { 555, 576, 629, 888, 539 };
	unsigned seen[64] = { 0 }, power = 99, half = 99, flags = 0;

	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 40000; f++)
	{
		const int scroll = (int16_t)ram_word(0x101A56u);
		const int sx = (int16_t)ram_word(0x102158u + 8u), sy = (int16_t)ram_word(0x102158u + 6u);
		unsigned move = 0;
		int tx = -1, ty = -1, best = -1;

		if (scroll >= boss[setup.area % 5] || ram_word(0x1018ECu) != 7u)
			break;
		*cave_work_ram_byte(0x102196u) = 2;
		/* chase the nearest item record (0x106114..., 0x20 each) */
		for (int i = 0; i < 16; i++)
		{
			const uint32_t at = 0x106114u + 0x20u * (uint32_t)i;
			const int x = (int16_t)ram_word(at + 8u), y = (int16_t)ram_word(at + 6u);
			const int d = abs(x - sx) + abs(y - sy);

			if ((ram_word(at) & 0x8000u) && y >= 0 && y < 0x5000 && x >= 0 && x < 0x3C00 && (best < 0 || d < best))
				best = d, tx = x, ty = y;
		}
		if (best < 0)
		{
			int most = 0;

			for (int r = 0; r < 150; r++)
			{
				const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);

				if (on_screen_record(at) && (int16_t)ram_word(at + 0x18u) > most)
					most = (int16_t)ram_word(at + 0x18u), tx = (int16_t)ram_word(at + 8u);
			}
		}
		if (tx >= 0 && tx > sx + 256)
			move |= CAVE_RIGHT;
		if (tx >= 0 && tx < sx - 256)
			move |= CAVE_LEFT;
		if (ty >= 0 && ty > sy + 256)
			move |= CAVE_DOWN;
		if (ty >= 0 && ty < sy - 256)
			move |= CAVE_UP;
		cave_board.in0 = (uint16_t)~(((f / 2) % 2 ? CAVE_BUTTON1 : 0u) | move);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		if (ram_word(0x102178u) != power || ram_word(0x102194u) != half || (ram_word(0x101A86u) << 4 ^ (ram_word(0x102158u) & 0x400u)) != flags)
		{
			power = ram_word(0x102178u), half = ram_word(0x102194u), flags = (ram_word(0x101A86u) << 4 ^ (ram_word(0x102158u) & 0x400u));
			printf("scroll %4d: power %u half %u maxflag %u player %04X\n", scroll, power, half, ram_word(0x101A86u), ram_word(0x102158u));
			for (uint32_t a = 0x100000u; a < 0x110000u; a += 2u)
				if (ram_word(a) == 0x3A93u || ram_word(a) == 0x332Eu)
					printf("   %06X: %04X (record? %04X %04X %04X %04X)\n", a, ram_word(a), ram_word(a - 4u), ram_word(a - 2u), ram_word(a + 2u), ram_word(a + 4u));
		}
		if (getenv("SFX"))
		{
			static bool was[4];
			static uint32_t last_sample[4], last_base[4];

			for (int v = 0; v < 4; v++)
			{
				const cave_oki_voice *const voice = &cave_board.sound.oki[1].voice[v];

				if (voice->playing && (!was[v] || voice->sample < last_sample[v] || voice->base != last_base[v]))
					printf("scroll %4d: sound voice %d bank %02X base %05X %u nibbles volume %d\n", scroll, v,
						cave_board.sound.bank[4 + ((voice->base >> 16) & 3u)], voice->base, voice->count, voice->volume);
				was[v] = voice->playing, last_sample[v] = voice->sample, last_base[v] = voice->base;
			}
		}
		if (getenv("SPRITES"))
		{
			static uint16_t was[0x200];

			for (int r = 0; r < 0x200; r++)
			{
				const uint32_t at = 0x106114u + 0x20u * (uint32_t)r; if (r >= 16) break;
				const uint16_t sp = (ram_word(at) & 0x8000u) ? ram_word(at + 4u) : 0u;

				if (!sp && was[r])
					printf("scroll %4d: record %06X gone (was %04X)\n", scroll, at, was[r]);
				if (sp && !was[r])
					printf("scroll %4d: record %06X sprite %04X x %04X y %04X\n", scroll, at, sp, ram_word(at + 8u), ram_word(at + 6u));
				was[r] = sp;
			}
		}
		for (int i = 0; i < 64; i++)
		{
			const uint32_t o = 0x100102u + 0x40u * (uint32_t)i;
			const bool item = (ram_word(o) & 0x8000u) && (ram_word(o + 0xCu) >> 8) == 0x16u;

			if (item && !seen[i] && dump)
			{
				const uint32_t a6 = (uint32_t)ram_word(o + 6u) << 16 | ram_word(o + 8u);

				printf("scroll %4d: item a6 %06X:", scroll, a6);
				for (int w = 0; w < 16; w++)
					printf(" %04X", ram_word(a6 + 2u * (uint32_t)w));
				printf(" ship %04X %04X\n", sx, sy);
			}
			seen[i] = item;
		}
	}
	return 0;
}
