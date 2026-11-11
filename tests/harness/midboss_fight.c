/* midboss_fight AREA: the training's mid-boss place, then a bot firing at the strongest enemy (shielded): the
   mid-boss objects' hit records 0-5 (hit points) every 30 frames, and when each goes */
#include "common.h"
#include "training.h"

static const unsigned midboss_type[5] = { 0x42u, 0x13u, 0x21u, 0x1Eu, 0x8Bu };

static unsigned steer(void)
{
	int most = 0, target = -1;
	const int ship = (int16_t)ram_word(0x102158u + 8u);

	for (int r = 0; r < 150; r++)
	{
		const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
		const int hp = (int16_t)ram_word(at + 0x18u), x = (int16_t)ram_word(at + 8u), y = (int16_t)ram_word(at + 6u);

		if ((ram_word(at) & 0xA000u) == 0xA000u && hp > most && hp < 32767 && y >= 0 && y < 0x5000 && x >= 0 && x < 0x3C00)
			most = hp, target = x;
	}
	if (target > ship + 256)
		return CAVE_RIGHT;
	if (target >= 0 && target < ship - 256)
		return CAVE_LEFT;
	return 0;
}

int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), TRAINING_MIDBOSS, 0, TRAINING_POWER_MAX, false, 0 };
	const unsigned type = midboss_type[setup.area % 5];
	bool was[64] = { false };

	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 3000; f++)
	{
		*cave_work_ram_byte(0x102196u) = 2;
		cave_board.in0 = (uint16_t)~(((f / 2) % 2 ? CAVE_BUTTON1 : 0u) | steer());
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		for (int i = 0; i < 64; i++)
		{
			const uint32_t o = 0x100102u + 0x40u * (uint32_t)i;
			const bool here = (ram_word(o) & 0x8000u) && (ram_word(o + 0xCu) >> 8) == type;
			const uint32_t hits = (uint32_t)ram_word(o + 6u) << 16 | ram_word(o + 8u);

			if (here && (!was[i] || f % 30 == 0))
			{
				printf("frame %4ld scroll %d slot %d:", f, (int16_t)ram_word(0x101A56u), i);
				for (int k = 0; k < 6; k++)
					printf(" [%04X %d]", ram_word(hits + 0x20u * (uint32_t)k), (int16_t)ram_word(hits + 0x20u * (uint32_t)k + 0x18u));
				printf(" obj+30..3F: %04X %04X %04X %04X %04X %04X %04X %04X score %04X%04X\n", ram_word(o + 0x30u), ram_word(o + 0x32u),
					ram_word(o + 0x34u), ram_word(o + 0x36u), ram_word(o + 0x38u), ram_word(o + 0x3Au), ram_word(o + 0x3Cu),
					ram_word(o + 0x3Eu), ram_word(0x10177Au), ram_word(0x10177Cu));
			}
			if (!here && was[i])
				printf("frame %4ld slot %d: gone, score %04X%04X\n", f, i, ram_word(0x10177Au), ram_word(0x10177Cu));
			if (here && getenv("RESERVE") && f == atol(getenv("RESERVE")))
			{
				printf("object %06X words:", o);
				for (int k = 0; k < 32; k++)
					printf(" %04X", ram_word(o + 2u * (uint32_t)k));
				printf("\nrecords %06X:", hits);
				for (int k = 0; k < 0x60; k++)
					printf("%s%04X", k % 16 ? " " : "\n  ", ram_word(hits + 2u * (uint32_t)k));
				printf("\n");
			}
			was[i] = here;
		}
	}
	return 0;
}
