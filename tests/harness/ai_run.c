/* ai_run [FRAMES]: the computer plays the game by itself for the best run it can find, no help of any kind (no
   cheats, no shield, no slowdown off: the arcade game as it is), and saves it as a replay (replays/) to watch in the
   game. Stage 1 of it: area 1, cleared without a miss.

   How: the game is deterministic, so the run is searched, not played. From where the run is, every action (the 9
   ways of the stick, with the shot tapped or the laser held) is tried for SEGMENT frames from a save state; each that
   lives is looked at further (LOOK frames, held on, or a few ways out of trouble), and the best by its score and
   how safe it is goes into the run. If none lives, the run goes back a few segments and tries the next best there
   (a bomb as a last way out: not yet). Then the whole run is played again from the board's reset as a replay is
   recorded, and checked: the same board at the end, and the replay played back as REPLAYS does.

   Prints the run's progress, and PASS or FAIL: area 1 cleared, no miss, the replay the same.
   First run (14-11-2026): area 1 cleared without a miss or a bomb in 13,602 frames of play (3:47), 262,550 points
   (the boss shot down); 6.2 minutes here, 5.6 million frames looked at */
#include "common.h"
#include "replay.h"

#include <time.h>

#define SEGMENT     6           /* the frames an action is held */
#define LOOK        30          /* the frames looked further, after it */
#define BACK        6           /* the segments gone back when nothing lives */
#define RUN_MOST    (60 * 60 * 6)   /* the frames of play the run may take */

#define MODE        0x1018ECu
#define SHIP        0x102158u   /* player 1's record: +0 bit 15 flying, +6 / +8 its place, +$22 its bombs */
#define LIVES       0x101A2Fu
#define STAGE       0x101A40u
#define SCORE       0x10177Au   /* player 1's score: BCD, 4 bytes */

enum { ACTIONS = 18 };

typedef struct
{
	program_state *state;       /* before the segment */
	int action;
	uint32_t tried;             /* the actions tried here and found wanting (bits) */
} step;

static uint16_t *inputs;        /* the run's inputs, frame by frame from the reset */
static long input_count;
static step *steps;
static int step_count;
static long frames_emulated;

static double now_s(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static bool flying(void)
{
	return (ram_word(SHIP) & 0x8000u) != 0;
}

static uint32_t score(void)
{
	uint32_t s = 0;

	for (int i = 0; i < 4; i++)
	{
		const unsigned b = ram_byte(SCORE + (uint32_t)i);

		s = s * 100u + (b >> 4) * 10u + (b & 15u);
	}
	return s;
}

/* an action's inputs in its 'frame'-th frame (0-): the stick's way, and the shot tapped (every other frame) or the
   laser held */
static uint16_t action_input(int action, int frame)
{
	static const unsigned ways[9] = { 0, CAVE_UP, CAVE_DOWN, CAVE_LEFT, CAVE_RIGHT, CAVE_UP | CAVE_LEFT,
		CAVE_UP | CAVE_RIGHT, CAVE_DOWN | CAVE_LEFT, CAVE_DOWN | CAVE_RIGHT };
	const int way = action % 9;
	const bool laser = action >= 9;
	unsigned held = ways[way];

	if (laser || frame % 2 == 0)
		held |= CAVE_BUTTON1;
	return (uint16_t)~held;
}

/* one frame with these inputs (player 2 idle) */
static void frame_with(uint16_t in0)
{
	cave_board.in0 = in0;
	cave_board.in1 = 0xFFFFu;
	program_run_frame();
	cave_sound_drop();
	frames_emulated++;
}

/* the run's inputs: one more frame */
static void keep_input(uint16_t in0)
{
	inputs[input_count++] = in0;
}

/* 'frames' frames of the action from frame 'from' of it: false if the ship is shot down in them (or the play ends) */
static bool hold(int action, int from, int frames, bool keep)
{
	for (int f = 0; f < frames; f++)
	{
		const uint16_t in = action_input(action, from + f);

		frame_with(in);
		if (keep)
			keep_input(in);
		if (!flying() || ram_word(MODE) != 7u)
			return flying();
	}
	return true;
}

/* how good an action is from the state 'from' (loaded first): -1 if the ship is shot down in its segment or in every
   way out of the look after it; otherwise its score gained (in the segment and the best look) and the ways out that
   lived */
static double value(const program_state *from, int action)
{
	static const int ways_out[5] = { -1, 0, 3, 4, 2 };    /* (held on; still; left; right; down: with the shot) */
	const uint32_t before = score();
	program_state *after;
	double best = -1.0;
	int lived = 0;

	program_state_load(from);
	if (!hold(action, 0, SEGMENT, false))
		return -1.0;
	if (ram_word(STAGE) != 0u || ram_word(MODE) != 7u)
		return 1e9;    /* (the area cleared in it) */
	after = program_state_save();
	for (int w = 0; w < 5; w++)
	{
		const int out = ways_out[w] < 0 ? action : ways_out[w];

		if (w)
			program_state_load(after);
		if (hold(out, w ? 0 : SEGMENT, LOOK, false))
		{
			const double gained = (double)(score() - before);

			lived++;
			if (gained > best)
				best = gained;
		}
	}
	program_state_free(after);
	if (!lived)
		return -1.0;
	/* the score, then how many ways out lived (safety) */
	return best + 200.0 * (double)lived;
}

int main(int argc, char **argv)
{
	const long run_most = argc > 1 ? atol(argv[1]) : RUN_MOST;
	const double began = now_s();
	long boot = 0;
	int backs = 0;
	unsigned lives_at_start;
	bool cleared = false, missed = false;

	inputs = malloc(sizeof(uint16_t) * (size_t)(run_most + 20000));
	steps = calloc((size_t)(run_most / SEGMENT + 10), sizeof(step));
	start_game();
	frame_with(0xFFFFu);
	program_reset();
	/* the boot, a coin, Start and the ship select (type A), as a player would: until the play begins */
	for (boot = 0; ram_word(MODE) != 7u && boot < 20000; boot++)
	{
		unsigned held = 0;

		if (ram_word(MODE) == 0xBu)
			held = (boot / 8) % 2 ? CAVE_BUTTON1 : 0u;
		else if (boot > 400)
		{
			const long beat = boot % 40;

			held = beat < 5 ? CAVE_COIN : beat >= 20 && beat < 25 ? CAVE_START : 0u;
		}
		frame_with((uint16_t)~held);
		keep_input((uint16_t)~held);
	}
	printf("play begins at frame %ld\n", boot);
	lives_at_start = ram_byte(LIVES);
	/* the search */
	while (input_count - boot < run_most)
	{
		step *const here = &steps[step_count];
		double best = -1.0;
		int choice = -1;

		if (!here->state)
			here->state = program_state_save();
		for (int a = 0; a < ACTIONS; a++)
		{
			double v;

			if (here->tried >> a & 1u)
				continue;
			v = value(here->state, a);
			if (v > best)
			{
				best = v;
				choice = a;
			}
		}
		if (choice < 0)
		{
			/* nothing lives: back a few segments, the action taken there ruled out */
			int to = step_count - BACK;

			if (to < 0)
				to = 0;
			backs++;
			for (int s = step_count; s > to; s--)
			{
				program_state_free(steps[s].state);
				steps[s].state = NULL;
				steps[s].tried = 0;
			}
			step_count = to;
			steps[to].tried |= 1u << steps[to].action;
			input_count = boot + (long)to * SEGMENT;
			if (backs % 50 == 0)
				printf("  stuck at frame %ld: %d times back so far\n", input_count - boot, backs);
			if (backs > 3000)
			{
				printf("given up: too many dead ends\n");
				break;
			}
			continue;
		}
		/* the best action into the run */
		program_state_load(here->state);
		here->action = choice;
		hold(choice, 0, SEGMENT, true);
		step_count++;
		if (!flying())
		{
			missed = true;
			break;
		}
		if (ram_word(STAGE) != 0u)
		{
			cleared = true;
			break;
		}
		if (step_count % 200 == 0)
			printf("  frame %ld: score %u, %d times back, %.0f s, %.1f M frames emulated\n", input_count - boot,
				score(), backs, now_s() - began, (double)frames_emulated / 1e6);
	}
	printf("the run: %ld frames of play, score %u, lives %u, %s; %d times back; %.0f s (%.1f M frames emulated)\n",
		input_count - boot, score(), ram_byte(LIVES), cleared ? "area 1 cleared" : missed ? "a miss" : "not cleared",
		backs, now_s() - began, (double)frames_emulated / 1e6);
	{
		/* the run again from the reset, recorded as a replay */
		const uint64_t searched = cave_state_hash();
		const char *saved = NULL;
		uint64_t again;

		program_reset();
		replay_folder("replays/");
		replay_reset();
		for (long f = 0; f < input_count; f++)
		{
			replay_input(inputs[f], 0xFFFFu, 0);
			frame_with(inputs[f]);
			replay_after_frame();
		}
		again = cave_state_hash();
		if (replay_in_game())
		{
			replay_hold_unfinished();
			saved = replay_keep(true);
		}
		const bool no_miss = !missed && ram_byte(LIVES) == lives_at_start;
		bool played_back = false;

		printf("replay: %s\n", saved ? saved : "(not saved)");
		if (saved)
		{
			/* the replay played as the game's REPLAYS does: in step all through, the same board at the end */
			const char *const problem = replay_play(saved);
			bool in_step = !problem;

			while (in_step && replay_next(NULL))
			{
				program_run_frame();
				cave_sound_drop();
				in_step = replay_after_playback_frame();
			}
			played_back = in_step && cave_state_hash() == searched;
			replay_stop();
		}
		printf("%-60s %s\n", "area 1 cleared", cleared ? "PASS" : "FAIL");
		printf("%-60s %s\n", "no miss", no_miss ? "PASS" : "FAIL");
		printf("%-60s %s\n", "the run again from the reset: the same board", again == searched ? "PASS" : "FAIL");
		printf("%-60s %s\n", "its replay played back: in step, the same board", played_back ? "PASS" : "FAIL");
		return !(cleared && no_miss && again == searched && played_back);
	}
}
