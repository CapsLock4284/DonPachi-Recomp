/* scaler_shot [AREA FRAMES]: the pixel-art scalers (ports/sdl2/scaler.c) over a frame of the game: played to an area
   (the training's way, then the simple player for FRAMES), its picture made larger by each scaler (scaler_NAME.ppm;
   scaler_off.ppm the plain picture), and how long each takes: the average over 200 runs, and the slowest. The
   scalers must leave a picture of one colour as it is (checked: PASS or FAIL) */
#include "common.h"
#include "training.h"
#include "scaler.h"

#include "cave/video.h"

#include <time.h>

static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
static uint32_t scaled[VIDEO_WIDTH * VIDEO_HEIGHT * 16];

static double now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
	const training_setup setup = { argc > 1 ? atoi(argv[1]) : 2, 0, 0, TRAINING_POWER_MAX, false, 0, 0 };
	const long frames = argc > 2 ? atol(argv[2]) : 1800;
	int failures = 0;

	start_game();
	training_prepare(&setup);
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
	}
	video_latch_sprites();
	video_draw(frame);
	video_rotated(frame, picture);
	for (int s = 0; s < SCALERS; s++)
	{
		const int k = scaler_factor(s);
		double total = 0.0, most = 0.0;
		char name[64];
		bool flat = true;

		for (int run = 0; run < 200; run++)
		{
			const double start = now_ms();
			double took;

			scaler_run(s, picture, VIDEO_HEIGHT, VIDEO_WIDTH, scaled);
			took = now_ms() - start;
			total += took;
			if (took > most)
				most = took;
		}
		snprintf(name, sizeof(name), "scaler_%s.ppm", scaler_key(s));
		save_ppm(name, scaled, VIDEO_HEIGHT * k, VIDEO_WIDTH * k);
		printf("%-8s %dx: %d x %d, %.2f ms a frame (the slowest %.2f)\n", scaler_name(s), k, VIDEO_HEIGHT * k,
			VIDEO_WIDTH * k, total / 200.0, most);
		{
			/* one colour stays one colour */
			static uint32_t grey[VIDEO_WIDTH * VIDEO_HEIGHT];

			for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
				grey[i] = 0xFF808080u;
			scaler_run(s, grey, VIDEO_HEIGHT, VIDEO_WIDTH, scaled);
			for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT * k * k; i++)
				flat &= scaled[i] == 0xFF808080u;
			printf("%-8s one colour kept: %s\n", scaler_name(s), flat ? "PASS" : "FAIL");
			failures += !flat;
		}
	}
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
