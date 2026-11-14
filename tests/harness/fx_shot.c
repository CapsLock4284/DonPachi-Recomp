/* fx_shot AREA FRAME [FRAME...]: the simple player (shielded) from AREA's start, a picture at each FRAME (fx_FRAME.ppm)
   with the explosions (104AD4) boxed red and the effects (106914) green; HITS=1: the enemies' hit boxes too, from
   their hit records (yellow, the bosses' magenta) */
#include "common.h"
#include "training.h"
#include "cave/video.h"
static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], pic[VIDEO_WIDTH * VIDEO_HEIGHT];
static void box(int x, int y, uint32_t c)
{
	for (int d = -8; d <= 8; d++)
	{
		int pts[4][2] = { { x + d, y - 8 }, { x + d, y + 8 }, { x - 8, y + d }, { x + 8, y + d } };
		for (int k = 0; k < 4; k++)
			if (pts[k][0] >= 0 && pts[k][0] < VIDEO_HEIGHT && pts[k][1] >= 0 && pts[k][1] < VIDEO_WIDTH)
				pic[pts[k][1] * VIDEO_HEIGHT + pts[k][0]] = c;
	}
}
int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), 0, 0, TRAINING_POWER_MAX, false, 0 };
	int next = 2;
	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 12000 && next < argc; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		program_run_frame();
		cave_sound_drop();
		if (f == atol(argv[next]))
		{
			char name[64];
			video_latch_sprites(); video_draw(frame); video_rotated(frame, pic);
			for (int i = 0; i < 30; i++) { uint32_t at = 0x104AD4u + 0x20u * i; if (ram_word(at) & 0x8000u) box((int16_t)ram_word(at + 8) / 64, 320 - (int16_t)ram_word(at + 6) / 64, 0xFF0000u); }
			for (int i = 0; i < 40; i++) { uint32_t at = 0x106914u + 0x20u * i; if (ram_word(at) & 0x8000u) { box((int16_t)ram_word(at + 8) / 64, 320 - (int16_t)ram_word(at + 6) / 64, 0x00FF00u); printf("B kind %u\n", ram_word(at) & 15); } }
			if (getenv("HITS"))
				for (int r = 0; r < 150; r++)
				{
					const uint32_t at = r < 100 ? 0x103814u + 0x20u * r : 0x104494u + 0x20u * (r - 100);
					if ((ram_word(at) & 0xA000u) != 0xA000u) continue;
					const int x = (int16_t)ram_word(at + 8) / 64, y = 320 - (int16_t)ram_word(at + 6) / 64;
					const int up = ram_word(at + 0x10) / 64, down = ram_word(at + 0x12) / 64, left = ram_word(at + 0x14) / 64, right = ram_word(at + 0x16) / 64;
					for (int px = x - left; px <= x + right; px++) for (int py = y - up; py <= y + down; py++)
						if ((px == x - left || px == x + right || py == y - up || py == y + down) && px >= 0 && px < VIDEO_HEIGHT && py >= 0 && py < VIDEO_WIDTH)
							pic[py * VIDEO_HEIGHT + px] = r < 100 ? 0xFFFF00u : 0xFF00FFu;
				}
			snprintf(name, sizeof(name), "fx_%ld.ppm", f);
			save_ppm(name, pic, VIDEO_HEIGHT, VIDEO_WIDTH);
			next++;
		}
	}
	return 0;
}
