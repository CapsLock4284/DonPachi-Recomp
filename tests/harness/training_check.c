/* training_check: runs the training's way to places (training.c) and reports where each landed, with a picture
   400 frames after (training_N.ppm).
     training_check AREA PLACE SHIP POWER [AREA PLACE SHIP POWER ...]
       AREA 0-9 (areas 1-10), PLACE 0 stage start / 1 mid-boss / 2 boss / 3 area 10's true boss, SHIP 0-2,
       POWER 0-3 (levels 1-4) / 4 on pace / 5 max
   A picture at the place (3 frames on: training_N_a.ppm) and 400 frames after (training_N_b.ppm); the enemies' bullets and the
   enemies on the screen at the place.
   e.g. training_check 0 1 0 4 5 2 0 5 */
#include "common.h"
#include "training.h"
#include "cave/video.h"

#include <time.h>

int main(int argc, char **argv)
{
	int shot = 0;

	start_game();
	for (int arg = 1; arg + 3 < argc; arg += 4)
	{
		const training_setup setup = { atoi(argv[arg]), atoi(argv[arg + 1]), atoi(argv[arg + 2]),
			atoi(argv[arg + 3]), false, 0 };
		const clock_t started = clock();
		const uint64_t first = program_frames();
		const char *const problem = training_prepare(&setup);
		static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
		char name[32];

		int bullets = 0, enemies = 0;

		for (uint32_t b = 0; b < 75u; b++)
			bullets += ram_word(0x104E94u + 0x40u * b) != 0;
		for (int r = 0; r < 150; r++)
		{
			const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
			const int x = (int16_t)ram_word(at + 8u), y = (int16_t)ram_word(at + 6u);

			enemies += (ram_word(at) & 0xA000u) == 0xA000u && y >= 0 && y < 0x5000 && x >= 0 && x < 0x3C00;
		}
		printf("%s, power %s: %s (%llu frames, %.2f s) mode %X stage %u loop %u scroll %d lives %u power %u/%u max %u"
			" ship %04X bees %u; bullets %d enemies %d\n",
			training_name(&setup), training_power_name(&setup), problem ? problem : "ok",
			(unsigned long long)(program_frames() - first), (double)(clock() - started) / CLOCKS_PER_SEC,
			ram_word(0x1018ECu), ram_word(0x101A40u), ram_word(0x101A42u), (int16_t)ram_word(0x101A56u),
			ram_byte(0x101A2Fu), ram_byte(0x102179u), ram_word(0x102194u), ram_word(0x101A86u), ram_word(0x102158u),
			ram_word(0x10379Au), bullets, enemies);
		for (int k = 0; k < 3; k++)    /* (the sprites shown a frame or two behind) */
		{
			if (getenv("CHAIN"))
				printf("  chain: hits %u timers %u %u values %04X %04X %04X\n", ram_word(0x10174Eu), ram_word(0x101742u),
					ram_word(0x101744u), ram_word(0x10173Au), ram_word(0x10173Eu), ram_word(0x10174Au));
			cave_board.in0 = 0xFFFFu;
			program_run_frame();
			cave_sound_drop();
		}
		video_latch_sprites();
		video_draw(frame);
		video_rotated(frame, picture);
		snprintf(name, sizeof(name), "training_%d_a.ppm", shot);
		save_ppm(name, picture, VIDEO_HEIGHT, VIDEO_WIDTH);
		for (int k = 3; k < 400; k++)
		{
			cave_board.in0 = 0xFFFFu;
			program_run_frame();
			cave_sound_drop();
		}
		video_latch_sprites();
		video_draw(frame);
		video_rotated(frame, picture);
		snprintf(name, sizeof(name), "training_%d_b.ppm", shot++);
		save_ppm(name, picture, VIDEO_HEIGHT, VIDEO_WIDTH);
	}
	return 0;
}
