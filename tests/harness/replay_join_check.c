/* replay_join_check: a second player joining a game played by the simple player 1 (a coin in player 2's slot,
   then its Start) is the same game to the replays' recording (replay.c): no "save the replay?" as it joins (the
   program's mode turns 7 into 0 then), only as the game is over. PASS or FAIL */
#include "common.h"
#include "replay.h"
#include "game_mode.h"

int main(void)
{
	long join_at = -1, ended_at = -1, both_from = -1;

	start_game();
	replay_folder("replays/");
	replay_reset();
	for (long f = 0; f < 20000 && ended_at < 0; f++)
	{
		unsigned p1 = autoplayer(f, 0), p2 = 0;

		/* player 2 in, a few seconds into player 1's game */
		if (join_at < 0 && game_mode_raw() == 7u && *cave_work_ram_byte(0x101A2Fu) == 3u)
			join_at = f + 600;
		if (join_at >= 0 && f >= join_at && f < join_at + 6)
			p2 = CAVE_COIN;
		if (join_at >= 0 && f >= join_at + 20 && f < join_at + 26)
			p2 = CAVE_START;
		if (game_mode_raw() == 0u && game_mode_playing() && both_from < 0)
			both_from = f;
		if (game_mode_raw() == 8u)
			p1 = 0;    /* (no continue) */
		cave_board.in0 = (uint16_t)~p1;
		cave_board.in1 = (uint16_t)~p2;
		replay_input(cave_board.in0, cave_board.in1, 0);
		program_run_frame();
		cave_sound_drop();
		if (replay_after_frame())
		{
			ended_at = f;
			replay_keep(false);
		}
	}
	printf("player 2 joined at frame %ld (both playing from %ld); the game's end told at frame %ld, mode %u, lives %u / %u\n",
		join_at, both_from, ended_at, game_mode_raw(), *cave_work_ram_byte(0x101A2Fu), *cave_work_ram_byte(0x101A31u));
	{
		const bool pass = both_from >= 0 && ended_at > both_from && !game_mode_playing();

		printf("%s\n", pass ? "PASS" : "FAIL");
		return pass ? 0 : 1;
	}
}
