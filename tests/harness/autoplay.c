/* autoplay: plays the arcade game by itself from the attract mode (the first ship, sweeping and firing) and reports
   what happens: the stage and loop changes, deaths, bees taken, and the achievements earned (as the port would show
   them, one after another), into a test achievements file; and the lifetime statistics it made (autoplay.stats).
     autoplay FRAMES [SHIELD]     SHIELD 1: the ship can't be hit (default 0)
   e.g. autoplay 15500 1   (area 1 to its clear, about 14200 frames) */
#include "common.h"
#include "achievements.h"
#include "widgets.h"
#include "stats.h"

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 15500;
	const int shield = argc > 2 ? atoi(argv[2]) : 0;
	unsigned last_stage = 99, last_loop = 99, last_lives = 0, last_bees = 0;

	start_game();
	remove("autoplay.achievements");
	achievements_load("autoplay.achievements");
	remove("autoplay.stats");
	stats_load("autoplay.stats");
	for (long frame = 0; frame < frames; frame++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(frame, shield);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		widgets_frame();
		achievements_frame(1);
		{
			const unsigned defeats = widgets_take_defeats();

			if (defeats)
				printf("frame %ld: shot down:%s%s%s\n", frame, defeats & 4u ? " MID-BOSS" : "", defeats & 1u ? " BOSS" : "",
					defeats & 2u ? " TRUE BOSS" : "");
			stats_frame(1, defeats);
		}
		if (!widgets_toast_busy())
		{
			const char *const earned = achievements_next_shown();

			if (earned)
			{
				printf("frame %ld: ACHIEVEMENT %s (%d more waiting)\n", frame, earned, achievements_waiting());
				widgets_toast(earned, achievements_waiting());
			}
		}
		if (ram_word(0x101A40u) != last_stage || ram_word(0x101A42u) != last_loop)
		{
			last_stage = ram_word(0x101A40u);
			last_loop = ram_word(0x101A42u);
			printf("frame %ld: stage %u loop %u (mode %X, scroll %d)\n", frame, last_stage, last_loop,
				ram_word(0x1018ECu), (int16_t)ram_word(0x101A56u));
		}
		if (ram_word(0x101A2Eu) < last_lives)
			printf("frame %ld: a life lost (%u left)\n", frame, ram_word(0x101A2Eu));
		last_lives = ram_word(0x101A2Eu);
		if (ram_word(0x10379Au) > last_bees)
			printf("frame %ld: a bee (%u in this area)\n", frame, ram_word(0x10379Au));
		last_bees = ram_word(0x10379Au);
	}
	printf("score %X%04X; achievements %d of %d\n", ram_word(0x10177Au), ram_word(0x10177Cu), achievements_done(),
		achievements_count());
	stats_save();
	printf("statistics:\n");
	for (int line = 0; line < stats_lines(); line++)
		printf("  %-16s %s\n", stats_label(line), stats_value(line) ? stats_value(line) : "");
	return 0;
}
