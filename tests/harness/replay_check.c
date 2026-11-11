/* replay_check FRAMES: records a game played by the simple player (from the board's power-on, as the port starts),
   saves it as a replay (replays/ here), plays it back from a reset and compares: each check in the file, and the
   board's hash at the end against the recording's. Frame 300 has a write of the port's (as the high score file's
   table put back), recorded as a patch. TAMPER=frame: the stick held another way for a second in
   playback from then, which the checks should catch */
#include "common.h"
#include "replay.h"

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 20000;
	const char *saved = NULL;
	replay_info list[8];
	uint64_t recorded_hash;
	long played = 0;
	bool in_step = true;

	start_game();
	replay_folder("replays/");
	replay_reset();
	for (long f = 0; f < frames; f++)
	{
		const char *name;

		cave_board.in0 = (uint16_t)~autoplayer(f, 0);
		cave_board.in1 = 0xFFFFu;
		replay_input(cave_board.in0, cave_board.in1, 0);
		program_run_frame();
		cave_sound_drop();
		if (f == 300)
		{
			/* (as the port puts the high score file's table back: a write of its own, recorded as a patch) */
			*cave_work_ram_byte(0x101846u) = 0x09u;
			*cave_work_ram_byte(0x101782u) = 0x09u;
			replay_patch(0x101846u, 0x5Au);
			replay_patch(0x101782u, 4u);
		}
		if ((name = replay_after_frame()) != NULL)
		{
			printf("frame %ld: saved %s\n", f, name);
			saved = name;
			break;
		}
	}
	if (!saved && !(saved = replay_save_unfinished()))
	{
		printf("nothing saved (no game played?)\n");
		return 1;
	}
	recorded_hash = cave_state_hash();
	{
		const int count = replay_list(list, 8);

		printf("list: %d replay(s), the newest %s (%s): area %d, score %u, ship %d, difficulty %d, %s, %u frames\n",
			count, list[0].file, list[0].date, list[0].area, list[0].score, list[0].ship, list[0].difficulty,
			list[0].finished ? "finished" : "stopped", list[0].frames);
	}
	{
		const char *const problem = replay_play(list[0].file);

		if (problem)
		{
			printf("playback: %s\n", problem);
			return 1;
		}
	}
	while (replay_next(NULL))
	{
		if (getenv("TAMPER") && played >= atol(getenv("TAMPER")) && played < atol(getenv("TAMPER")) + 60)
			cave_board.in0 ^= 0x0004u;    /* (a second of the stick held another way) */
		program_run_frame();
		cave_sound_drop();
		if (!replay_after_playback_frame())
		{
			printf("out of step at a check, %ld frames into the game\n", played);
			in_step = false;
		}
		played++;
	}
	printf("played %ld frames of the game; end hash %s the recording's\n", played,
		cave_state_hash() == recorded_hash ? "the same as" : "DIFFERENT from");
	replay_stop();
	return in_step && cave_state_hash() == recorded_hash ? 0 : 1;
}
