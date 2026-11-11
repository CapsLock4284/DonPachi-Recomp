/* panels_shot: a picture of the game as the port shows it on a wide screen: the left panel, the game, the right
   panel (panels.ppm, 480 x 320). Training's way to an area's start (PLACE=1: its mid-boss, 2: its boss), then the simple player for FRAMES frames.
     panels_shot AREA FRAMES [TOAST]     TOAST: an achievement's name, its pop-up shown (with "+2 MORE")
   MAP=file: the right panel the stage map, from that maps file (stagemap_check.dat)
   e.g. panels_shot 2 1800 "ONE CREDIT CLEAR"   (the README's area 3 picture is panels_shot 2 1800) */
#include "common.h"
#include "training.h"
#include "widgets.h"
#include "stagemap.h"
#include "cave/video.h"

int main(int argc, char **argv)
{
	const training_setup setup = { argc > 1 ? atoi(argv[1]) : 0, getenv("PLACE") ? atoi(getenv("PLACE")) : 0, 0,
		TRAINING_POWER_MAX, false, 0 };
	const long frames = argc > 2 ? atol(argv[2]) : 1800;
	static uint32_t left[WIDGET_W * WIDGET_H], right[WIDGET_W * WIDGET_H];
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	static uint32_t out[480 * 320];

	start_game();
	training_prepare(&setup);
	if (getenv("MAP"))
	{
		const widgets_status status = { false, false, false, 0, stagemap_load(getenv("MAP")) };

		if (!status.stage_map)
			printf("%s: no maps\n", getenv("MAP"));
		widgets_set_status(&status);
	}
	widgets_reset();
	widgets_begin_game();
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		widgets_frame();
		if (argc > 3 && f == frames - 20)
			widgets_toast(argv[3], 2);
	}
	widgets_draw(left, right);
	video_latch_sprites();
	video_draw(frame);
	video_rotated(frame, picture);
	for (int y = 0; y < 320; y++)
		for (int x = 0; x < 480; x++)
			out[y * 480 + x] = x < 120 ? left[y * WIDGET_W + x]
				: x < 360 ? picture[y * 240 + x - 120] : right[y * WIDGET_W + x - 360];
	save_ppm("panels.ppm", out, 480, 320);
	printf("panels.ppm: area %d after %ld frames\n", setup.area + 1, frames);
	return 0;
}
