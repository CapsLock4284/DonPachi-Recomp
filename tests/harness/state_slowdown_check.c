/* state_slowdown_check SAVE_AT RUN: the port with its slowdown, played by slowdown_check's simple player: a save
   state taken at VBLANK SAVE_AT (a VBLANK that may have come in the middle of the program's work), RUN more
   VBLANKs played, the state loaded and the same VBLANKs played again: the board's hash at each must be the same */
#include "common.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "cave/program.h"

static unsigned player(uint64_t frame)
{
	const unsigned mode = ram_word(0x1018ECu);
	unsigned held = 0;

	if (mode == 0xBu)
		held = (frame / 8) % 2 ? CAVE_BUTTON1 : 0u;
	else if (mode == 7u)
	{
		*cave_work_ram_byte(0x102196u) = 2;
		held = ((frame / 40) % 2 ? CAVE_RIGHT : CAVE_LEFT) | ((frame / 2) % 2 ? CAVE_BUTTON1 : 0u);
		if ((frame / 300) % 4 == 3)
			held |= CAVE_BUTTON1;
	}
	else if (frame % 120 < 4)
		held = CAVE_COIN;
	else if (frame % 120 > 60 && frame % 120 < 64)
		held = CAVE_START;
	return held;
}

int main(int argc, char **argv)
{
	const uint64_t save_at = argc > 1 ? strtoull(argv[1], NULL, 10) : 6000;
	const uint64_t run = argc > 2 ? strtoull(argv[2], NULL, 10) : 3000;
	uint64_t *hashes = calloc(run, sizeof *hashes);
	uint64_t slow_at_save, slow_first;
	program_state *state;
	int differ = 0;

	program_set_slowdown(true);
	start_game();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < save_at)
	{
		program_run_frame();
		cave_sound_drop();
		cave_board.in0 = (uint16_t)~player(cave_board.frame);
	}
	state = program_state_save();
	if (!state)
	{
		printf("no state\n");
		return 1;
	}
	slow_at_save = program_slow_frames();
	for (uint64_t i = 0; i < run; i++)
	{
		program_run_frame();
		cave_sound_drop();
		cave_board.in0 = (uint16_t)~player(cave_board.frame);
		hashes[i] = cave_state_hash();
	}
	slow_first = program_slow_frames() - slow_at_save;
	program_state_load(state);
	for (uint64_t i = 0; i < run; i++)
	{
		program_run_frame();
		cave_sound_drop();
		cave_board.in0 = (uint16_t)~player(cave_board.frame);
		if (cave_state_hash() != hashes[i] && differ++ == 0)
			printf("first difference %" PRIu64 " VBLANKs after the state\n", i + 1);
	}
	printf("state at VBLANK %" PRIu64 ", %" PRIu64 " VBLANKs played twice: %s; slow frames %" PRIu64 " then %" PRIu64 "\n",
		save_at, run, differ ? "DIFFERENT" : "the same", slow_first, program_slow_frames() - slow_at_save);
	program_state_free(state);
	return differ != 0;
}
