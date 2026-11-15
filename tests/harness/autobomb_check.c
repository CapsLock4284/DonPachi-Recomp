/* autobomb_check [FRAMES]: AUTOBOMB (ports/sdl2/autobomb.c) with the simple player (no shield, no bombs of its own),
   FRAMES of play (default 30000) without it and then with it, from the same start: the deaths, those with a bomb
   left, the bombs it set off (and how many were the laser's: the shot held into them) and how long it took a frame. With it on, no death with a bomb left (PASS or FAIL),
   and fewer deaths than without (PASS or FAIL). Then the inputs of the run with it played again without it: the same
   board at the end (its bombs are inputs like any other: replays keep them). Then with the shot held most of the
   time (the laser on): its bombs normal ones, not the laser's (it lets the shot go a frame first), but where there was no time
   for that (at most 1 in 10), and still no death with a bomb left. And a reset asked for in play with it on: done */
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
	unsigned set_off, laser, let_go;
	unsigned seen_normal, seen_laser;    /* the bombs that went off, as the game's memory has them ($106E14) */
	double ms;
	uint64_t hash;
} outcome;

/* FRAMES from the state 'start': 'mode' 0 the simple player, 1 the same with autobomb (inputs recorded), 2 the
   recorded inputs played back, 3 the simple player with the shot held most of the time (the laser) and autobomb */
static outcome play(const program_state *start, long frames, int mode)
{
	outcome o = { 0 };
	const unsigned before = autobomb_count(), laser_before = autobomb_laser_count();
	unsigned last_bombs = 0;
	bool was_flying = false, was_bomb = false;
	double began;

	program_state_load(start);
	autobomb_reset();
	began = now_ms();
	for (long f = 0; f < frames; f++)
	{
		const bool flying = (ram_word(0x102158u) & 0x8000u) != 0;

		if (mode == 2)
			cave_board.in0 = recorded[f];
		else
		{
			unsigned held = autoplayer(f + 100000, 0);
			uint16_t own;

			if (mode == 3)
			{
				/* the shot held most of the time (the laser), let go now and then to start it again */
				held &= ~(unsigned)CAVE_BUTTON1;
				if (f % 200 >= 10 && f % 200 < 190)
					held |= CAVE_BUTTON1;
			}
			own = (uint16_t)~held;

			cave_board.in0 = own;
			if (mode == 1 || mode == 3)
				autobomb_frame(false);
			if (!(own & CAVE_BUTTON1) && (cave_board.in0 & CAVE_BUTTON1))
				o.let_go++;    /* (the shot let go for a normal bomb next frame) */
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
		{
			const unsigned bomb = ram_word(0x106E14u);

			if ((bomb & 0x8000u) && !was_bomb)
			{
				if (!(bomb & 0x0100u))    /* (the laser's: no bit 8) */
					o.seen_laser++;
				else
					o.seen_normal++;
			}
			was_bomb = (bomb & 0x8000u) != 0;
		}
	}
	o.ms = (now_ms() - began) / (double)frames;
	o.set_off = autobomb_count() - before;
	o.laser = autobomb_laser_count() - laser_before;
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
	printf("with:    %d deaths (%d with a bomb left), %u bombs set off (%u the laser's: no time to let the shot go), "
		"%.3f ms a frame\n", on.deaths, on.with_bombs, on.set_off, on.laser, on.ms);
	printf("         the shot let go %u times for a normal bomb instead of the laser's\n", on.let_go);
	snprintf(what, sizeof(what), "with it: no death with a bomb left (%d)", on.with_bombs);
	verdict(what, on.with_bombs == 0);
	snprintf(what, sizeof(what), "with it: fewer deaths (%d against %d)", on.deaths, off.deaths);
	verdict(what, on.deaths < off.deaths);
	verdict("its inputs played back: the same board at the end", again.hash == on.hash);
	{
		const outcome laser = play(start, frames, 3);

		printf("the laser on most of the time: %d deaths (%d with a bomb left), %u bombs set off: %u normal, %u the laser's "
			"(%u by its own count); the shot let go %u times first\n", laser.deaths, laser.with_bombs, laser.set_off,
			laser.seen_normal, laser.seen_laser, laser.laser, laser.let_go);
		snprintf(what, sizeof(what), "the laser on: normal bombs (%u of %u the laser's)", laser.seen_laser,
			laser.seen_normal + laser.seen_laser);
		verdict(what, laser.seen_laser == laser.laser && laser.seen_laser * 10u <= laser.seen_normal + laser.seen_laser);
		snprintf(what, sizeof(what), "the laser on: no death with a bomb left (%d)", laser.with_bombs);
		verdict(what, laser.with_bombs == 0);
	}
	{
		/* a reset asked for in the middle of play (F3, the menu's MAIN MENU) with it on: not used up by its looking
		   ahead (the board's frame counter starts again) */
		program_state_load(start);
		for (int f = 0; f < 3; f++)
		{
			cave_board.in0 = 0xFFFFu;
			autobomb_frame(false);
			program_run_frame();
			cave_sound_drop();
		}
		program_reset();
		cave_board.in0 = 0xFFFFu;
		autobomb_frame(false);
		program_run_frame();
		cave_sound_drop();
		snprintf(what, sizeof(what), "a reset in play with it on: done (frames since it %llu)",
			(unsigned long long)program_frames_since_reset());
		verdict(what, program_frames_since_reset() <= 1u);
	}
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
