/* slowdown_check VBLANKS [SLOWDOWN]: the port (with its slowdown: SLOWDOWN 1, the default; 0 off) played from the
   power on by the same simple player as tools/cref/cref_timed --play, for VBLANKS of the board's frames; writes a
   line per VBLANK, "vblank lagged hash" (slowdown_check.txt), and counts the VBLANKs lagged in play (no frame of the
   game since the last); and a line per frame of the game as it comes to its wait, "game_frame vblank hash"
   (slowdown_waits.txt), as cref_timed --waits writes MAME's, by the same rule. Compare the waits with
   tests/harness/slowdown_compare.py:
     cref_timed --rom rom --frames 30000 --play --waits mame_waits.txt
     slowdown_check 30000
     python tests/harness/slowdown_compare.py mame_waits.txt slowdown_waits.txt
   DUMPFRAME=N: the board's memories at game frame N's wait in port_dump.bin (cref_timed's: mame_dump.bin) */
#include "common.h"

#include <inttypes.h>

/* tools/cref/cref_timed.cpp's player: player 1's inputs (active high) for the VBLANK 'frame', set as its interrupt
   is taken */
static unsigned player_inputs(uint64_t frame)
{
	static uint64_t select_frames;
	const unsigned mode = ram_word(0x1018ECu);
	unsigned held = 0;

	if (mode == 0xBu)
	{
		if (++select_frames > 60 && (select_frames / 8) % 2)
			held = CAVE_BUTTON1;
	}
	else if (mode == 7u)
	{
		const int x = (int16_t)ram_word(0x102158u + 6u) >> 6;
		int target = 40 + (int)(frame / 3 % 160);

		if ((frame / 480) & 1)
			target = 200 - (int)(frame / 3 % 160);
		*cave_work_ram_byte(0x102196u) = 2;
		if (x < target - 4)
			held |= CAVE_RIGHT;
		else if (x > target + 4)
			held |= CAVE_LEFT;
		held |= (frame / 2) % 2 ? CAVE_BUTTON1 : 0u;
		if ((frame / 300) % 4 == 3)
			held |= CAVE_BUTTON1;
	}
	else if (frame % 120 < 4)
		held = CAVE_COIN;
	else if (frame % 120 > 60 && frame % 120 < 64)
		held = CAVE_START;
	return held;
}

static FILE *waits;
static uint64_t game_frames;

/* (the game's frame done, as it comes to its wait: cref_timed --waits writes MAME's) */
static bool arrived;

static void at_wait(void)
{
	/* (a wait the program has made ready, once: cref_timed's rule; a wait it comes to when the VBLANK's interrupt
	   has already taken its frame, nothing ready, is the board's frame shown twice) */
	if (arrived || !(ram_word(0x1011FCu) & 1u))
		return;
	arrived = true;
	game_frames++;
	if (getenv("DUMPFRAME") && game_frames == strtoull(getenv("DUMPFRAME"), NULL, 10))
		cave_dump("port_dump.bin");
	if (waits)
		fprintf(waits, "%" PRIu64 " %" PRIu64 " %016" PRIx64 "\n", game_frames, cave_board.frame, cave_state_hash());
}

int main(int argc, char **argv)
{
	const uint64_t vblanks = argc > 1 ? strtoull(argv[1], NULL, 10) : 18000;
	FILE *const out = fopen("slowdown_check.txt", "w");
	uint64_t slow_before = 0, lagged_in_play = 0;

	program_set_slowdown(argc > 2 ? atoi(argv[2]) != 0 : true);
	waits = fopen("slowdown_waits.txt", "w");
	program_at_wait = at_wait;
	start_game();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	while (cave_board.frame < vblanks)
	{
		uint64_t slow;

		program_run_frame();
		cave_sound_drop();
		/* (this VBLANK came while the program worked: no frame of the game since the last one) */
		slow = slow_before + (!arrived && cave_board.frame > 1u);
		arrived = false;
		/* (this VBLANK came while the program worked: the slow frames went up as it came) */
		if (out)
			fprintf(out, "%" PRIu64 " %d %016" PRIx64 "\n", cave_board.frame, slow != slow_before, cave_state_hash());
		if (slow != slow_before && ram_word(0x1018ECu) == 7u)
			lagged_in_play++;
		slow_before = slow;
		cave_board.in0 = (uint16_t)~player_inputs(cave_board.frame);
	}
	if (out)
		fclose(out);
	if (waits)
		fclose(waits);
	printf("vblanks %" PRIu64 ", lagged in play %" PRIu64 " (timed program: %s)\n", vblanks, lagged_in_play,
		program_timed() ? "yes" : "no");
	return 0;
}
