/* midboss_find AREA [FRAMES]: the training's mid-boss place, then no input (shielded): each object type as it
   appears, and every 60 frames the objects whose hit records (+6) hold large hit points: the mid-boss */
#include "common.h"
#include "training.h"

int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), getenv("START") ? TRAINING_START : TRAINING_MIDBOSS, 0, TRAINING_POWER_MAX, false, 0 };
	const long frames = argc > 2 ? atol(argv[2]) : 900;
	unsigned seen_type[64];

	start_game();
	training_prepare(&setup);
	for (int i = 0; i < 64; i++)
		seen_type[i] = ram_word(0x100102u + 0x40u * (uint32_t)i) & 0x8000u ? ram_word(0x100102u + 0x40u * (uint32_t)i + 0xCu) >> 8 : 0xFFFFu;
	for (long f = 0; f < frames; f++)
	{
		*cave_work_ram_byte(0x102196u) = 2;
		cave_board.in0 = 0xFFFFu;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		for (int i = 0; i < 64; i++)
		{
			const uint32_t o = 0x100102u + 0x40u * (uint32_t)i;
			const unsigned type = ram_word(o) & 0x8000u ? ram_word(o + 0xCu) >> 8 : 0xFFFFu;

			if (type != seen_type[i] && type != 0xFFFFu)
				printf("frame %4ld scroll %d: slot %2d new type %02X\n", f, (int16_t)ram_word(0x101A56u), i, type);
			seen_type[i] = type;
		}
		if (f % 60 == 0)
			for (int i = 0; i < 64; i++)
			{
				const uint32_t o = 0x100102u + 0x40u * (uint32_t)i;
				const uint32_t hits = (uint32_t)ram_word(o + 6u) << 16 | ram_word(o + 8u);
				char line[512];
				int n = 0, big = 0;

				if (!(ram_word(o) & 0x8000u) || hits < 0x103814u || hits >= 0x105000u)
					continue;
				for (int k = 0; k < 6; k++)
				{
					const uint32_t r = hits + 0x20u * (uint32_t)k;
					const int hp = (int16_t)ram_word(r + 0x18u);

					n += snprintf(line + n, sizeof(line) - (size_t)n, " [%04X %d @%04X,%04X]", ram_word(r), hp,
						ram_word(r + 8u), ram_word(r + 6u));
					if (k == 0 && (ram_word(r) & 0xA000u) == 0xA000u && hp >= 1000 && hp < 32767)
						big = 1;
				}
				if (big)
					printf("frame %4ld scroll %d: slot %2d type %02X hits %06X:%s\n", f, (int16_t)ram_word(0x101A56u), i,
						ram_word(o + 0xCu) >> 8, hits, line);
			}
	}
	return 0;
}
