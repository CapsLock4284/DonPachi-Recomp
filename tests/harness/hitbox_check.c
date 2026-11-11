/* hitbox_check: a game from the attract mode (the simple player, shielded); prints each frame the hitbox appears or
   goes, with the ship's record, and saves the picture with the hitbox as it first appears (hitbox_N.ppm) */
#include "common.h"
#include "hitbox.h"
#include "cave/video.h"

int main(void)
{
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT], blank[240 * 320];
	bool shown = false;
	int shots = 0;

	start_game();
	for (long f = 0; f < 4000; f++)
	{
		const unsigned mode = ram_word(0x1018ECu);
		unsigned held = 0;
		int lit = 0;

		if (mode != 7u && mode != 0xBu && f > 400)
			held = f % 40 < 5 ? CAVE_COIN : (f % 40 >= 20 && f % 40 < 25 ? CAVE_START : 0u);
		else if (mode == 0xBu && (f / 8) % 2)
			held = CAVE_BUTTON1;
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		hitbox_record();
		memset(blank, 0, sizeof(blank));
		hitbox_draw(blank, 1);
		for (int p = 0; p < 240 * 320; p++)
			lit += blank[p] != 0;
		if ((lit > 0) != shown)
		{
			shown = lit > 0;
			printf("frame %4ld mode %X: hitbox %s, ship %04X at %04X,%04X\n", f, ram_word(0x1018ECu), shown ? "shown" : "gone",
				ram_word(0x102158u), ram_word(0x102158u + 6u), ram_word(0x102158u + 8u));
			if (shown && shots < 3)
			{
				char name[32];

				video_latch_sprites();
				video_draw(frame);
				video_rotated(frame, picture);
				hitbox_draw(picture, 1);
				snprintf(name, sizeof(name), "hitbox_%d.ppm", shots++);
				save_ppm(name, picture, VIDEO_HEIGHT, VIDEO_WIDTH);
			}
		}
	}
	return 0;
}
