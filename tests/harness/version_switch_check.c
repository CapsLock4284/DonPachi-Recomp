/* version_switch_check [FOLDER]: the game's version changed while it runs (cave/program.h's program_switch, as the
   menu's VERSION does it): the US set and program, then the Japanese set over it (rom_set_load_japan_over: from
   FOLDER, default rom, donpachij.zip alone), then back to the US one. Each switch starts its program from its reset
   (the frames since it: 0, for the boot's skip), its frames the same as a reset of it run again with the same
   inputs (the board's hash), and the US game after the round trip the same as before it. PASS or FAIL each */
#include "common.h"

#include "cave/rom_set.h"

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-64s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

#define FRAMES 2400

static uint64_t first_since;    /* the frames since the reset, after run's first */

/* FRAMES from where the program is: the attract mode, a coin and Start, the shot held: the board's hash */
static uint64_t run(void)
{
	for (long f = 0; f < FRAMES; f++)
	{
		const unsigned held = f > 900 && f < 905 ? CAVE_COIN : f > 1000 && f < 1005 ? CAVE_START
			: f > 1100 ? CAVE_BUTTON1 : 0u;

		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		if (f == 0)
			first_since = program_frames_since_reset();
	}
	return cave_state_hash();
}

int main(int argc, char **argv)
{
	const char *const folder = argc > 1 ? argv[1] : "rom";
	char us_zip[1024];
	const char *problem;
	uint64_t us_first, us_again, jp_switched, jp_again, us_back;

	if (!program_has_version(DONPACHI_JAPAN))
	{
		printf("this build has no Japanese program (src_jpn/)\nFAILED\n");
		return 1;
	}
	snprintf(us_zip, sizeof(us_zip), "%s/donpachi.zip", folder);
	if ((problem = rom_set_load_version(us_zip, DONPACHI_US)) != NULL || (problem = program_start()) != NULL)
	{
		printf("%s\nFAILED\n", problem);
		return 1;
	}
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	program_run_frame();
	program_reset();
	us_first = run();
	program_reset();
	us_again = run();
	verdict("US: a reset's frames since it, after a frame: 1", first_since == 1u);
	verdict("US: the same frames from a reset twice", us_first == us_again);

	/* the Japanese set over the US one, the program switched mid-game */
	problem = rom_set_load_japan_over(folder);
	verdict("the Japanese set over the US one (donpachij.zip)", !problem && rom_set.version == DONPACHI_JAPAN
		&& memcmp(rom_set.program + 0x400u, "DONPACHI Ver1.01", 16) == 0);
	if (problem)
		printf("  %s\n", problem);
	problem = program_switch();
	verdict("switched to the Japanese program", !problem && cave_board.program == rom_set.program);
	jp_switched = run();
	verdict("Japan: from its reset (its frames since it, after a frame: 1)", first_since == 1u);
	program_reset();
	jp_again = run();
	verdict("Japan: the switch's frames the same as a reset's", jp_switched == jp_again);
	verdict("Japan: not the US game's frames", jp_switched != us_first);

	/* back to the US one */
	problem = rom_set_load_version(us_zip, DONPACHI_US);
	if (!problem)
		problem = program_switch();
	verdict("switched back to the US program", !problem && rom_set.version == DONPACHI_US);
	us_back = run();
	verdict("US after the round trip: the same frames as before it", us_back == us_first);
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
