/* botcmp AREA HANDOVER_SCROLL BOT: the training run to AREA's start, then a bot fights to the scroll; prints the
   shootable enemies and the bullets on screen there. BOT 0 strongest, 1 nearest, 2 alternating, 3 sweep */
#include "common.h"
#include "training.h"
#include "cave/video.h"

static int pick(int mode)
{
	int best = -1, most = 0, near = 1 << 30;
	const int ship = (int16_t)ram_word(0x102158u + 8u);

	for (int r = 0; r < 150; r++)
	{
		const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
		const int hp = (int16_t)ram_word(at + 0x18u), x = (int16_t)ram_word(at + 8u), y = (int16_t)ram_word(at + 6u);

		if ((ram_word(at) & 0xA000u) != 0xA000u || hp < 0 || y < 0 || y >= 0x5000 || x < 0 || x >= 0x3C00)
			continue;
		if (mode == 0 ? hp > most : abs(x - ship) < near)
			most = hp, near = abs(x - ship), best = r;
	}
	return best;
}

static unsigned steer(int r)
{
	if (r >= 0)
	{
		const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
		const int target = (int16_t)ram_word(at + 8u), ship = (int16_t)ram_word(0x102158u + 8u);

		if (target > ship + 256)
			return CAVE_RIGHT;
		if (target < ship - 256)
			return CAVE_LEFT;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), 0, 0, TRAINING_POWER_MAX, false, 0 };
	const int to = atoi(argv[2]), bot = atoi(argv[3]);

	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 40000; f++)
	{
		unsigned held = (f / 2) % 2 ? CAVE_BUTTON1 : 0u;

		if ((int16_t)ram_word(0x101A56u) >= to)
			break;
		*cave_work_ram_byte(0x102196u) = 2;
		*cave_work_ram_byte(0x102179u) = 4;
		if (bot == 3)
			held = autoplayer(f, 1);
		else
			held |= steer(pick(bot == 2 ? (int)(f / 90 % 2) : bot));
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
	}
	{
		int enemies = 0, bullets = 0;

		for (int r = 0; r < 150; r++)
		{
			const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
			const int y = (int16_t)ram_word(at + 6u);

			if ((ram_word(at) & 0xA000u) == 0xA000u && y >= 0 && y < 0x5000 && (int16_t)ram_word(at + 8u) >= 0 && (int16_t)ram_word(at + 8u) < 0x3C00)
				enemies++;
		}
		for (int b = 0; b < 75; b++)
			if (ram_word(0x104E94u + 0x40u * (uint32_t)b) & 0x8000u)
				bullets++;
		printf("area %d bot %d: %d enemies, %d bullets\n", setup.area + 1, bot, enemies, bullets);
		if (getenv("DUMP"))
			for (int r = 0; r < 150; r++)
			{
				const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);

				if (ram_word(at))
					printf("  rec %2d: %04X x %d y %d hp %d +2 %04X +4 %04X\n", r, ram_word(at), (int16_t)ram_word(at + 8u), (int16_t)ram_word(at + 6u), (int16_t)ram_word(at + 0x18u), ram_word(at + 2u), ram_word(at + 4u));
			}
	}
	if (getenv("SHOT"))
	{
		static uint32_t fr[VIDEO_WIDTH * VIDEO_HEIGHT], pc[VIDEO_WIDTH * VIDEO_HEIGHT];

		video_latch_sprites();
		video_draw(fr);
		video_rotated(fr, pc);
		save_ppm(getenv("SHOT"), pc, VIDEO_HEIGHT, VIDEO_WIDTH);
	}
	return 0;
}
