/* training_check: runs the training's way to places (training.c) and reports where each landed, with a picture
   400 frames after (training_N.ppm).
     training_check AREA PLACE SHIP POWER [AREA PLACE SHIP POWER ...]
       AREA 0-9 (areas 1-10), PLACE 0 stage start / 1 boss / 2 area 10's true boss, SHIP 0-2, POWER 0/1 (max)
   e.g. training_check 0 1 0 1 5 0 2 0 */
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
			atoi(argv[arg + 3]) != 0, false, 0 };
		const clock_t started = clock();
		const uint64_t first = program_frames();
		const char *const problem = training_prepare(&setup);
		static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
		char name[32];

		printf("%s: %s (%llu frames, %.2f s) mode %X stage %u loop %u scroll %d lives %u power %u\n",
			training_name(&setup), problem ? problem : "ok", (unsigned long long)(program_frames() - first),
			(double)(clock() - started) / CLOCKS_PER_SEC, ram_word(0x1018ECu), ram_word(0x101A40u),
			ram_word(0x101A42u), (int16_t)ram_word(0x101A56u), ram_byte(0x101A2Fu), ram_byte(0x102179u));
		for (int k = 0; k < 400; k++)
		{
			cave_board.in0 = 0xFFFFu;
			program_run_frame();
			cave_sound_drop();
		}
		video_latch_sprites();
		video_draw(frame);
		video_rotated(frame, picture);
		snprintf(name, sizeof(name), "training_%d.ppm", shot++);
		save_ppm(name, picture, VIDEO_HEIGHT, VIDEO_WIDTH);
	}
	return 0;
}
