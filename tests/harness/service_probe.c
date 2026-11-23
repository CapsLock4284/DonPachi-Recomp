/* service_probe: the service switch held from the board's reset until some frames after its self-test (the frame
   counter, $101260, no longer 0), then let go: whether the game came up in its test mode 900 frames after the reset
   (its first page, the colour bars on a white grid: told by the picture's white pixels), for holds of 0 to 240
   frames after the self-test. The port's service menu reset holds it SERVICE_MENU_FRAMES (240) after the
   self-test (main.c): the test mode comes up for about 130 or more */
#include "common.h"
#include "cave/video.h"

static uint32_t board[VIDEO_WIDTH * VIDEO_HEIGHT];

static bool in_test_mode(void)
{
	long white = 0;

	video_latch_sprites();
	video_draw(board);
	for (int at = 0; at < VIDEO_WIDTH * VIDEO_HEIGHT; at++)
		white += (board[at] & 0xFFFFFFu) == 0xFFFFFFu;
	return white > VIDEO_WIDTH * VIDEO_HEIGHT / 20;
}

int main(void)
{
	start_game();
	for (int extra = 0; extra <= 240; extra += 20)
	{
		unsigned held = (unsigned)extra;

		program_reset();
		for (long frame = 0; frame < 900; frame++)
		{
			unsigned pressed = 0;

			if (held)
			{
				pressed = CAVE_SERVICE;
				if (ram_word(0x101260u))
					held--;
			}
			cave_board.in0 = (uint16_t)~pressed;
			cave_board.in1 = 0xFFFFu;
			program_run_frame();
			cave_sound_drop();
		}
		printf("held %3d frames after the self-test: %s\n", extra, in_test_mode() ? "TEST MODE" : "the game");
	}
	return 0;
}
