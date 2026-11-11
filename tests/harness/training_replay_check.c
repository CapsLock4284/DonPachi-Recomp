/* training_replay_check AREA PLACE FRAMES: a training place made as the port makes it, an attempt played from there
   by the simple player (recorded as the port does), kept with SAVE REPLAY, then played back (the place made again
   from the file's setup) and compared: each check, and the board's hash at the end. Then the replay renamed and
   deleted (replays/ here; KEEP=1: kept). DEBUG=1: the first frame that differs, and the memory that differs at the
   start */
#include "common.h"
#include "replay.h"
#include "training.h"

int main(int argc, char **argv)
{
	const training_setup setup = { argc > 1 ? atoi(argv[1]) : 2, argc > 2 ? atoi(argv[2]) : TRAINING_MIDBOSS, 1,
		TRAINING_POWER_ON_PACE, false, 2 };
	const long frames = argc > 3 ? atol(argv[3]) : 3000;
	const char *saved, *problem;
	char file[256];
	replay_info list[16];
	uint64_t recorded_hash;
	long played = 0;
	bool in_step = true;
	int count;

	start_game();
	replay_folder("replays/");
	replay_reset();
	if ((problem = training_prepare(&setup)) != NULL)
	{
		printf("training: %s\n", problem);
		return 1;
	}
	training_hud();
	replay_begin_training(&setup);
	static uint64_t hashes[700];
	hashes[0] = cave_state_hash();
	static uint8_t ram0[0x10000];
	for (uint32_t a = 0; a < 0x10000u; a++)
		ram0[a] = *cave_work_ram_byte(0x100000u + a);
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 0);
		cave_board.in1 = 0xFFFFu;
		replay_input(cave_board.in0, cave_board.in1, 0);
		program_run_frame();
		cave_sound_drop();
		replay_after_frame();
		if (f + 1 < 700)
			hashes[f + 1] = cave_state_hash();
	}
	recorded_hash = cave_state_hash();
	if (!(saved = replay_save_training()))
	{
		printf("nothing saved\n");
		return 1;
	}
	snprintf(file, sizeof(file), "%s", saved);
	count = replay_list(list, 16);
	printf("saved %s; listed %d, the newest: %s, training %d (%s), area %d, %u frames\n", file, count, list[0].file,
		list[0].training, list[0].place, list[0].area, list[0].frames);
	if ((problem = replay_play(file)) != NULL)
	{
		printf("playback: %s\n", problem);
		return 1;
	}
	if (getenv("DEBUG"))
	{
		printf("start: %s\n", cave_state_hash() == hashes[0] ? "same" : "DIFFERENT");
		for (uint32_t a = 0; a < 0x10000u; a++)
			if (ram0[a] != *cave_work_ram_byte(0x100000u + a))
				printf("  %06X: %02X -> %02X\n", 0x100000u + a, ram0[a], *cave_work_ram_byte(0x100000u + a));
	}
	while (replay_next(NULL))
	{
		program_run_frame();
		cave_sound_drop();
		if (!replay_after_playback_frame())
		{
			printf("out of step at a check, %ld frames in\n", played);
			in_step = false;
		}
		played++;
		if (getenv("DEBUG") && played < 700 && cave_state_hash() != hashes[played])
		{
			printf("first different after frame %ld\n", played);
			unsetenv("DEBUG");
		}
	}
	printf("played %ld frames; end hash %s the recording's\n", played,
		cave_state_hash() == recorded_hash ? "the same as" : "DIFFERENT from");
	replay_stop();
	/* renamed, then deleted (KEEP=1: renamed only) */
	if (!replay_rename(file, "my best-run 2!"))
		printf("rename failed\n");
	count = replay_list(list, 16);
	printf("renamed: %s (name \"%s\")\n", list[0].file, list[0].name);
	if (!getenv("KEEP"))
		printf("deleted: %s\n", replay_delete(list[0].file) ? "yes" : "NO");
	return in_step && cave_state_hash() != 0 ? 0 : 1;
}
