/* layer_shot AREA FRAMES [FILTER]: GRAPHICS > BY LAYER (ports/sdl2/layer_filter.c) beside the filter over the whole
   picture: training's way to an area's start (PLACE=1 its mid-boss, 2 its boss), then the simple player for FRAMES
   frames; the picture made larger with FILTER (scaler.h's key: mmpx, the default, scale2x or scale3x) both ways,
   layer_whole.ppm and layer_by_layer.ppm, and how long each took a frame (over 200 frames more). Checks too that the
   planes put together at the game's size are video_draw's picture, every frame of the 200: PASS or FAIL */
#include "common.h"
#include "training.h"
#include "scaler.h"
#include "layer_filter.h"
#include "cave/sound.h"
#include "cave/video.h"

#include <time.h>

static double now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
	const training_setup setup = { argc > 1 ? atoi(argv[1]) : 0, getenv("PLACE") ? atoi(getenv("PLACE")) : 0, 0,
		TRAINING_POWER_MAX, false, 0 };
	const long frames = argc > 2 ? atol(argv[2]) : 1800;
	int filter = SCALER_MMPX, factor, differ = 0;
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	static uint32_t plain_frame[VIDEO_WIDTH * VIDEO_HEIGHT], plain[VIDEO_WIDTH * VIDEO_HEIGHT];
	uint32_t *whole, *layered;
	double whole_ms = 0, layered_ms = 0;

	for (int f = 0; argc > 3 && f < SCALERS; f++)
		if (!strcmp(argv[3], scaler_key(f)))
			filter = f;
	factor = scaler_factor(filter);
	whole = malloc((size_t)VIDEO_WIDTH * VIDEO_HEIGHT * (size_t)(factor * factor) * sizeof(uint32_t));
	layered = malloc((size_t)VIDEO_WIDTH * VIDEO_HEIGHT * (size_t)(factor * factor) * sizeof(uint32_t));
	start_game();
	training_prepare(&setup);
	for (long f = 0; f < frames + 200; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		video_latch_sprites();
		if (f < frames)
			continue;
		video_draw(plain_frame);
		video_rotated(plain_frame, plain);
		{
			double t0 = now_ms(), t1, t2;

			scaler_run(filter, plain, VIDEO_HEIGHT, VIDEO_WIDTH, whole);
			t1 = now_ms();
			layer_filter_draw(frame, picture);
			if (!layer_filter_scale(filter, picture, picture, layered))
			{
				printf("layer_filter_scale refused (%s)\n", scaler_key(filter));
				return 1;
			}
			t2 = now_ms();
			whole_ms += t1 - t0;
			layered_ms += t2 - t1;
		}
		differ += memcmp(plain, picture, sizeof(plain)) != 0;
		if (f == frames)
		{
			save_ppm("layer_whole.ppm", whole, VIDEO_HEIGHT * factor, VIDEO_WIDTH * factor);
			save_ppm("layer_by_layer.ppm", layered, VIDEO_HEIGHT * factor, VIDEO_WIDTH * factor);
		}
	}
	printf("%s: the whole picture %.2f ms a frame, by layer %.2f ms (the planes drawn, put together and scaled)\n",
		scaler_key(filter), whole_ms / 200.0, layered_ms / 200.0);
	printf("%-60s %s\n", "the planes put together are video_draw's picture (200 frames)", differ ? "FAIL" : "PASS");
	scaler_quit();
	return differ != 0;
}
