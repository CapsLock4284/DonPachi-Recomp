/* panels_shot: a picture of the game as the port shows it on a wide screen: the left panel, the game, the right
   panel (panels.ppm, 480 x 320; WIDTH=W: panels W wide, WIDGET_W to WIDGET_MAX_W, as on a wider screen: 156 at
   16:9, the picture 2W + 240 wide; METER=1: the FPS meter on the right panel, as F9 shows it; GADGETS=K: the
   gadget panels instead (gadgets.h), drawn K pixels to the game's, the picture K times as large: gadgets.ppm). Training's way to an area's start (PLACE=1: its mid-boss, 2: its boss), then the simple player for FRAMES frames.
     panels_shot AREA FRAMES [TOAST]     TOAST: an achievement's name, its pop-up shown (with "+2 MORE"); AI=1:
                                         player 2 flown by the computer (ai_player.h)
   e.g. panels_shot 2 1800 "ONE CREDIT CLEAR"   (the README's area 3 picture is panels_shot 2 1800) */
#include "common.h"
#include "training.h"
#include "widgets.h"
#include "gadgets.h"
#include "achievements.h"
#include "cave/video.h"
#include "ai_player.h"
#include "cave/program.h"

int main(int argc, char **argv)
{
	const training_setup setup = { argc > 1 ? atoi(argv[1]) : 0, getenv("PLACE") ? atoi(getenv("PLACE")) : 0, 0,
		TRAINING_POWER_MAX, false, 0 };
	const long frames = argc > 2 ? atol(argv[2]) : 1800;
	static uint32_t left[WIDGET_MAX_W * WIDGET_H], right[WIDGET_MAX_W * WIDGET_H];
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	static uint32_t out[(2 * WIDGET_MAX_W + 240) * 320];
	int width = getenv("WIDTH") ? atoi(getenv("WIDTH")) : WIDGET_W, all;

	start_game();
	training_prepare(&setup);
	{
		const widgets_status status = { false, false, false, 0, program_slowdown(), true };

		widgets_set_status(&status);
	}
	widgets_reset();
	widgets_set_clean(true);
	widgets_begin_game();
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		cave_board.in1 = getenv("AI") ? (uint16_t)~ai_player_inputs() : 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		widgets_frame();
		achievements_frame(true);
		if (argc > 3 && f == frames - 20)
			widgets_toast(argv[3], 2);
	}
	width = width < WIDGET_W ? WIDGET_W : width > WIDGET_MAX_W ? WIDGET_MAX_W : width;
	all = 2 * width + 240;
	if (getenv("METER"))
	{
		/* (the FPS meter on the right panel, as F9 shows it with the panels: figures for the picture) */
		const widgets_meter shown = { true, 56.6, 57.5, 57.5, 0, 3.2, 6.8 };

		widgets_set_meter(&shown);
	}
	if (getenv("GADGETS"))
	{
		/* the gadget panels, at K: the picture as large beside them */
		const int k = atoi(getenv("GADGETS")) < 1 ? 1 : atoi(getenv("GADGETS")) > 5 ? 5 : atoi(getenv("GADGETS"));
		uint32_t *const gl = malloc(sizeof(uint32_t) * (size_t)(width * k) * (size_t)(WIDGET_H * k));
		uint32_t *const gr = malloc(sizeof(uint32_t) * (size_t)(width * k) * (size_t)(WIDGET_H * k));
		uint32_t *const big = malloc(sizeof(uint32_t) * (size_t)(all * k) * (size_t)(320 * k));

		gadgets_draw(gl, gr, width, k, NULL);
		video_latch_sprites();
		video_draw(frame);
		video_rotated(frame, picture);
		for (int y = 0; y < 320 * k; y++)
			for (int x = 0; x < all * k; x++)
				big[y * all * k + x] = x < width * k ? gl[y * width * k + x]
					: x < (width + 240) * k ? picture[y / k * 240 + (x - width * k) / k]
					: gr[y * width * k + x - (width + 240) * k];
		save_ppm("gadgets.ppm", big, all * k, 320 * k);
		printf("gadgets.ppm: area %d after %ld frames, k %d\n", setup.area + 1, frames, k);
		return 0;
	}
	widgets_draw(left, right, width);
	video_latch_sprites();
	video_draw(frame);
	video_rotated(frame, picture);
	for (int y = 0; y < 320; y++)
		for (int x = 0; x < all; x++)
			out[y * all + x] = x < width ? left[y * width + x]
				: x < width + 240 ? picture[y * 240 + x - width] : right[y * width + x - width - 240];
	save_ppm("panels.ppm", out, all, 320);
	printf("panels.ppm: area %d after %ld frames\n", setup.area + 1, frames);
	return 0;
}
