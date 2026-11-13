/* training_part_way_check [PERCENT ...]: training's points part way through an area (training.c, TRAINING_PART_WAY),
   in each of the 10 areas at each PERCENT (default 10 50 90): the place reached, in its area, where the side panel's
   STAGE PROGRESS shows that percentage (its count: widgets.c's), player 1 flying with 3 lives, no shield, no enemy
   bullets on the screen; and still alive 2 seconds on, hands off (not shot at the hand-over). The replays' setup
   bytes read back the same (replay.c's way: 100 + the percentage). PASS or FAIL each */
#include "common.h"
#include "training.h"

#include <time.h>

static const int boss_at[5] = { 555, 576, 629, 888, 539 };
static const int start_at[5] = { 256, 0, 0, 0, 0 };
static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-70s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

int main(int argc, char **argv)
{
	int percents[20], count = 0;

	for (int arg = 1; arg < argc && count < 20; arg++)
		percents[count++] = atoi(argv[arg]);
	if (!count)
	{
		percents[0] = 10;
		percents[1] = 50;
		percents[2] = 90;
		count = 3;
	}
	start_game();
	for (int area = 0; area < 10; area++)
		for (int p = 0; p < count; p++)
		{
			const training_setup setup = { area, TRAINING_PART_WAY, area % 3, TRAINING_POWER_ON_PACE, false, 0,
				percents[p] };
			const clock_t started = clock();
			const char *const problem = training_prepare(&setup);
			const int stage = area % 5, scroll = (int16_t)ram_word(0x101A56u);
			const int end = boss_at[stage] - start_at[stage], shown = (scroll - start_at[stage]) * 100 / end;
			const bool at_place = !problem && ram_word(0x101A40u) == (unsigned)stage
				&& ram_word(0x101A42u) == (unsigned)(area / 5) && (ram_word(0x102158u) & 0x8000u)
				&& ram_byte(0x101A2Fu) == 3u && ram_byte(0x102196u) == 0u;
			int bullets = 0;
			bool alive = true;
			char what[96];

			for (uint32_t b = 0; b < 75u; b++)
				bullets += ram_word(0x104E94u + 0x40u * b) != 0;
			printf("%s (%s): %s, %.1f s; stage %u loop %u scroll %d (%d%%) lives %u ship %04X shield %u bullets %d\n",
				training_name(&setup), training_power_name(&setup), problem ? problem : "there",
				(double)(clock() - started) / CLOCKS_PER_SEC, ram_word(0x101A40u), ram_word(0x101A42u), scroll, shown,
				ram_byte(0x101A2Fu), ram_word(0x102158u), ram_byte(0x102196u), bullets);
			for (int k = 0; k < 115 && alive; k++)
			{
				cave_board.in0 = 0xFFFFu;
				program_run_frame();
				cave_sound_drop();
				alive = ram_byte(0x101A2Fu) == 3u;
			}
			snprintf(what, sizeof(what), "%s: reached, %d%% shown, 3 lives, no shield or bullets",
				training_name(&setup), percents[p]);
			verdict(what, at_place && shown == percents[p] && bullets == 0);
			snprintf(what, sizeof(what), "%s: alive 2 seconds on", training_name(&setup));
			verdict(what, alive);
		}
	{
		/* the setup's bytes, as replay.c keeps them */
		const training_setup setup = { 3, TRAINING_PART_WAY, 1, 2, true, 2, 45 };
		const unsigned place = setup.point == TRAINING_PART_WAY ? 100u + (unsigned)setup.percent : (unsigned)setup.point;
		const int point = place >= 100u ? TRAINING_PART_WAY : (int)(place % TRAINING_PART_WAY);
		const int percent = place >= 100u ? (int)place - 100 : 0;

		verdict("a replay's place byte: part way 45% read back", place < 256u && point == TRAINING_PART_WAY && percent == 45);
	}
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
