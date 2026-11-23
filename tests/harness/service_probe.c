/* service_probe: the service switch held from the board's reset until some frames after its self-test (the frame
   counter, $101260, no longer 0), then let go: whether the game came up in its test mode (its mode, $1018EC, 000C)
   1200 frames after the reset, for holds of 0 to 130 frames after the self-test. The port's service menu reset
   holds it SERVICE_MENU_FRAMES (60) after the self-test (main.c): the test mode comes up for about 4 to 120 */
#include "common.h"

int main(void)
{
	start_game();
	for (int extra = 0; extra <= 130; extra += 10)
	{
		unsigned held = (unsigned)extra;

		program_reset();
		for (long frame = 0; frame < 1200; frame++)
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
		}
		printf("held %3d frames after the self-test: %s\n", extra,
			ram_word(0x1018ECu) == 0xCu ? "TEST MODE" : "the game");
	}
	return 0;
}
