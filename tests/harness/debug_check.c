/* debug_check [FRAMES]: the developers' debug mode (ports/sdl2/own_cheats.c's DEBUG MODE: the buttons' byte's bit 2
   set as the vblank interrupt returns, in play), from area 1's start (the training's way there):
     not shot down: the simple player, unshielded, for FRAMES (default 2400): no life lost (and the same without it:
       lives lost)
     the pause: player 2's Start, then 120 frames: the stage's scroll doesn't move
     the stage advance: held player 1's C and Up, 120 frames, paused: the scroll moves; player 1's Start goes on
   and the RAM editor's flag ($1018E8) set while DEBUG RAM EDITOR is on, cleared as it goes off. PASS or FAIL each;
   SHOTS=1: pictures (debug_paused.ppm, debug_advanced.ppm, debug_ram_editor.ppm) */
#include "common.h"
#include "training.h"
#include "own_cheats.h"

#include "cave/cheats.h"
#include "cave/program.h"
#include "cave/video.h"

#define SCROLL  0x101A56u
#define LIVES   0x101A2Fu
#define SHIP    0x102158u

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-52s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static int find(const char *name)
{
	for (int index = 0; index < cheats_count(); index++)
		if (!strcmp(cheats_name(index), name))
			return index;
	return -1;
}

static void frame(unsigned p1, unsigned p2)
{
	cave_board.in0 = (uint16_t)~p1;
	cave_board.in1 = (uint16_t)~p2;
	program_run_frame();
	cave_sound_drop();
	cheats_apply();
}

static void shot(const char *name)
{
	static uint32_t board[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];

	if (!getenv("SHOTS"))
		return;
	video_latch_sprites();
	video_draw(board);
	video_rotated(board, picture);
	save_ppm(name, picture, 240, 320);
}

/* the simple player for 'frames', unshielded: the deaths (the ship going down) */
static unsigned play(long frames)
{
	unsigned deaths = 0;
	bool was_flying = true;

	for (long f = 0; f < frames; f++)
	{
		frame(autoplayer(f, 0), 0);
		if (program_frame_lagged())
			continue;
		{
			const bool flying = (ram_word(SHIP) & 0x8000u) != 0;

			if (was_flying && !flying && ram_word(0x1018ECu) == 7u)
				deaths++;
			was_flying = flying;
		}
	}
	return deaths;
}

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 2400;
	const training_setup setup = { 0, 0, 0, TRAINING_POWER_MAX, false, 0 };
	int debug, editor;
	unsigned without, with;

	own_cheats_add();
	cheats_load("no such file");
	debug = find(OWN_CHEATS_DEBUG "MODE");
	editor = find(OWN_CHEATS_DEBUG "RAM EDITOR");
	program_after_vblank = own_cheats_after_vblank;
	if (debug < 0 || editor < 0)
	{
		printf("the debug tools aren't in the cheats' table\n");
		return 1;
	}

	start_game();
	training_prepare(&setup);
	without = play(frames);

	start_game();
	training_prepare(&setup);
	cheats_set(debug, true);
	with = play(frames);
	printf("deaths without the debug mode %u, with it %u\n", without, with);
	verdict("DEBUG MODE: the ship not shot down", with == 0u && without > 0u);

	/* the pause: player 2's Start */
	{
		unsigned before, after;

		frame(0, CAVE_START);
		frame(0, 0);
		before = ram_word(SCROLL);
		for (int f = 0; f < 120; f++)
			frame(0, 0);
		after = ram_word(SCROLL);
		printf("paused: scroll %04X -> %04X\n", before, after);
		verdict("DEBUG MODE: P2 Start pauses (the stage stops)", before == after);
		shot("debug_paused.ppm");
		before = after;
		for (int f = 0; f < 120; f++)
			frame(CAVE_BUTTON3 | CAVE_UP, 0);
		after = ram_word(SCROLL);
		printf("advanced: scroll %04X -> %04X\n", before, after);
		verdict("DEBUG MODE: paused, P1 C + Up moves the stage", before != after);
		shot("debug_advanced.ppm");
		frame(CAVE_START, 0);
		frame(0, 0);
		before = ram_word(SCROLL);
		for (int f = 0; f < 60; f++)
			frame(0, 0);
		after = ram_word(SCROLL);
		verdict("DEBUG MODE: P1 Start goes on (the stage moves)", before != after);
	}

	/* the RAM editor */
	cheats_set(editor, true);
	for (int f = 0; f < 30; f++)
		frame(0, 0);
	verdict("DEBUG RAM EDITOR: its flag set", (*cave_work_ram_byte(0x1018E8u)) == 1u);
	shot("debug_ram_editor.ppm");
	cheats_set(editor, false);
	for (int f = 0; f < 5; f++)
		frame(0, 0);
	verdict("DEBUG RAM EDITOR off: its flag cleared", (*cave_work_ram_byte(0x1018E8u)) == 0u);

	printf("%s\n", failures ? "FAILED" : "all passed");
	return failures != 0;
}
