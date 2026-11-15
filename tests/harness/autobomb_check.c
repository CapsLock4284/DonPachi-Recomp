/* autobomb_check [FRAMES]: AUTOBOMB (ports/sdl2/autobomb.c) with the simple player (no shield, no bombs of its own),
   FRAMES of play (default 30000) without it and then with it, from the same start: the deaths, those with a bomb
   left, the bombs it set off and how long it took a frame. With it on, no death with a bomb left (PASS or FAIL),
   and fewer deaths than without (PASS or FAIL). Then the inputs of the run with it played again without it: the same
   board at the end (its bombs are inputs like any other: replays keep them) */
#include "common.h"
#include "autobomb.h"

#include <time.h>

static int failures;
static uint16_t *recorded;

static void verdict(const char *what, bool pass)
{
	printf("%-64s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static double now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

typedef struct
{
	int deaths, with_bombs;
	unsigned set_off;
	double ms;
	uint64_t hash;
} outcome;

/* FRAMES from the state 'start': 'mode' 0 the simple player, 1 the same with autobomb (inputs recorded), 2 the
   recorded inputs played back */
static outcome play(const program_state *start, long frames, int mode)
{
	outcome o = { 0 };
	const unsigned before = autobomb_count();
	unsigned last_bombs = 0;
	bool was_flying = false;
	double began;

	program_state_load(start);
	began = now_ms();
	for (long f = 0; f < frames; f++)
	{
		const bool flying = (ram_word(0x102158u) & 0x8000u) != 0;

		if (mode == 2)
			cave_board.in0 = recorded[f];
		else
		{
			cave_board.in0 = (uint16_t)~autoplayer(f + 100000, 0);
			if (mode == 1)
				autobomb_frame(false);
			recorded[f] = cave_board.in0;
		}
		cave_board.in1 = 0xFFFFu;
		if (flying)
			last_bombs = ram_word(0x102158u + 0x22u);
		program_run_frame();
		cave_sound_drop();
		if (ram_word(0x1018ECu) == 7u && was_flying && !(ram_word(0x102158u) & 0x8000u))
		{
			o.deaths++;
			if (last_bombs > 0u)
				o.with_bombs++;
		}
		was_flying = (ram_word(0x102158u) & 0x8000u) != 0;
	}
	o.ms = (now_ms() - began) / (double)frames;
	o.set_off = autobomb_count() - before;
	o.hash = cave_state_hash();
	return o;
}

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 30000;
	program_state *start;
	outcome off, on, again;
	char what[128];

	recorded = malloc((size_t)frames * sizeof(uint16_t));
	start_game();
	for (long f = 0; f < 1200 || ram_word(0x1018ECu) != 7u; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 0);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
	}
	start = program_state_save();
	off = play(start, frames, 0);
	on = play(start, frames, 1);
	again = play(start, frames, 2);
	printf("without: %d deaths (%d with a bomb left), %.3f ms a frame\n", off.deaths, off.with_bombs, off.ms);
	printf("with:    %d deaths (%d with a bomb left), %u bombs set off, %.3f ms a frame\n", on.deaths, on.with_bombs,
		on.set_off, on.ms);
	snprintf(what, sizeof(what), "with it: no death with a bomb left (%d)", on.with_bombs);
	verdict(what, on.with_bombs == 0);
	snprintf(what, sizeof(what), "with it: fewer deaths (%d against %d)", on.deaths, off.deaths);
	verdict(what, on.deaths < off.deaths);
	verdict("its inputs played back: the same board at the end", again.hash == on.hash);
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
