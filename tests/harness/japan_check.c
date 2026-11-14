/* japan_check [FOLDER]: the Japanese program (src_jpn/, in a build that has it: DONPACHI_JAPAN_PROGRAM) run from the
   Japanese ROM set (FOLDER, default rom: donpachij.zip and donpachi.zip): it boots to its own warning, takes a coin
   and Start and plays (the simple player, shielded); the same inputs twice give the same frames (the board's hash);
   training's way to a place works on it (the port reads the same RAM in both versions); a picture of each
   (japan_*.ppm). PASS or FAIL each */
#include "common.h"
#include "game_mode.h"
#include "training.h"

#include "cave/rom_set.h"
#include "cave/video.h"

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-62s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static void picture(const char *name)
{
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], rotated[VIDEO_WIDTH * VIDEO_HEIGHT];

	video_latch_sprites();
	video_draw(frame);
	video_rotated(frame, rotated);
	save_ppm(name, rotated, VIDEO_HEIGHT, VIDEO_WIDTH);
}

#define FRAMES 6000
static uint16_t inputs[FRAMES];

/* a reset, then FRAMES of the attract mode, a coin and Start, and the simple player (recorded the first time,
   played again the second): the board's hash */
static uint64_t run(long frames, bool record)
{
	program_reset();
	for (long f = 0; f < frames; f++)
	{
		unsigned held = f > 1500 && f < 1505 ? CAVE_COIN : f > 1600 && f < 1605 ? CAVE_START : 0u;

		if (f > 1700)
			held = autoplayer(f, 1);
		if (record)
			inputs[f] = (uint16_t)~held;
		cave_board.in0 = inputs[f];
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		if (record && f == 300)
			picture("japan_warning.ppm");
		if (record && f == frames - 1)
			picture("japan_play.ppm");
	}
	return cave_state_hash();
}

int main(int argc, char **argv)
{
	const char *problem;
	uint64_t first, second;

	if (!program_has_version(DONPACHI_JAPAN))
	{
		printf("this build has no Japanese program (src_jpn/)\nFAILED\n");
		return 1;
	}
	if ((problem = rom_set_load_version(argc > 1 ? argv[1] : "rom", DONPACHI_JAPAN)) != NULL
			|| (problem = program_start()) != NULL)
	{
		printf("%s\nFAILED\n", problem);
		return 1;
	}
	verdict("the Japanese set and program", rom_set.version == DONPACHI_JAPAN
		&& memcmp(rom_set.program + 0x400u, "DONPACHI Ver1.01", 16) == 0);
	cave_board.in0 = cave_board.in1 = 0xFFFFu;
	program_run_frame();    /* (started: the runs below from a reset of the running program, as a replay's) */
	first = run(FRAMES, true);
	verdict("a coin and Start: a game played (mode 7, player 1 flying)",
		game_mode_playing() && (ram_word(0x102158u) & 0x8000u) != 0);
	printf("  (stage %u, scroll %d, score %08X)\n", ram_word(0x101A40u), (int16_t)ram_word(0x101A56u),
		(unsigned)ram_word(0x10177Au) << 16 | ram_word(0x10177Cu));
	second = run(FRAMES, false);
	verdict("the same inputs again: the same frames (the board's hash)", first == second);
	{
		const training_setup setup = { 2, TRAINING_BOSS, 1, TRAINING_POWER_MAX, false, 0 };

		problem = training_prepare(&setup);
		verdict("training's way to area 3's boss", problem == NULL && ram_word(0x101A40u) == 2u);
		picture("japan_training.ppm");
	}
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
