/* cheats_check [FRAMES]: the port's own cheats (ports/sdl2/own_cheats.c), each played from area 1's start (the
   training's way there) by the simple autoplayer, unshielded, for FRAMES (default 3600), with the cheat on, checking
   what it should do:
     P1 INFINITE LIVES   lives lost, but never fewer than 2 left, and no game over
     P1 INFINITE BOMBS   a bomb let go every 5 seconds: the bombs back at the slots' count the next frame
     P1 INVINCIBLE       no life lost
     P1 MAX POWER        from the lowest power: MAX (power 4, the MAX flag) on every frame flying
     P1 ENDLESS CHAIN    once a chain starts, its timer never runs out
   and the same without the cheat, to show the test would see it fail. Prints PASS or FAIL for each; with SHOTS=1,
   a picture of the game after each (cheat_N.ppm) */
#include "common.h"
#include "training.h"
#include "own_cheats.h"

#include "cave/cheats.h"
#include "cave/video.h"
#include "cave/program.h"

#define LIVES       0x101A2Fu
#define SHIP        0x102158u
#define BOMBS       (SHIP + 0x22u)
#define SLOTS       0x101A8Au
#define POWER       (SHIP + 0x21u)
#define MAX_POWER   0x101A86u
#define TIMER       0x101742u
#define HITS        0x10174Eu

typedef struct
{
	unsigned least_lives, lost, game_over, bombs_short, bombs_let_go, power_short, chain_breaks, chains;
} seen;

static unsigned byte_at(uint32_t address)
{
	return *cave_work_ram_byte(address);
}

static seen play(int cheat, int also, long frames, const char *shot)
{
	const training_setup setup = { 0, 0, 0, 0, false, 0 };    /* (area 1's start, the lowest power) */
	seen s = { 99u, 0, 0, 0, 0, 0, 0, 0 };
	unsigned last_lives, last_timer = 0, last_bombs;
	bool last_flying = false, last_bomb_on = false;
	long bomb_at = -1;

	cheats_off();
	start_game();
	training_prepare(&setup);
	if (cheat >= 0)
		cheats_set(cheat, true);
	if (also >= 0)
		cheats_set(also, true);
	last_lives = byte_at(LIVES);
	last_bombs = ram_word(BOMBS);
	for (long f = 0; f < frames; f++)
	{
		unsigned held = autoplayer(f, 0);

		if (f % 300 == 150)
		{
			held |= CAVE_BUTTON2;    /* (a bomb) */
			bomb_at = f;
		}
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		cheats_apply();
		if (program_frame_lagged())
			continue;
		{
			const unsigned mode = ram_word(0x1018ECu), lives = byte_at(LIVES), bombs = ram_word(BOMBS);
			const unsigned timer = ram_word(TIMER);

			if (mode != 7u)
			{
				if (mode == 8u || mode == 0u || mode == 1u)
					s.game_over = 1;
				continue;
			}
			if (lives < s.least_lives)
				s.least_lives = lives;
			/* (a death: the ship down; a bomb: its flash begun. The cheats put the lives and bombs back before the
			   frame is looked at) */
			{
				const bool now_flying = (ram_word(SHIP) & 0x8000u) != 0, bomb_on = (ram_word(0x106E14u) & 0x8000u) != 0;

				if (now_flying && (byte_at(POWER) != 4u || ram_word(MAX_POWER) != 1u))
					s.power_short++;    /* (flying below MAX) */
				if (last_flying && !now_flying)
					s.lost++;
				if (bomb_on && !last_bomb_on)
					s.bombs_let_go++;
				last_flying = now_flying;
				last_bomb_on = bomb_on;
			}
			/* (a few frames after a bomb: the bombs back, or not) */
			if (bomb_at >= 0 && f == bomb_at + 10 && (ram_word(SHIP) & 0x8000u) && bombs < byte_at(SLOTS))
				s.bombs_short++;
			if (last_timer > 0 && timer == 0 && ram_word(HITS) == 0)
				s.chain_breaks++;
			if (last_timer == 0 && timer > 0)
				s.chains++;
			last_lives = lives;
			last_bombs = bombs;
			last_timer = timer;
		}
	}
	if (getenv("VERBOSE"))
		printf("  cheat %d: %u lives lost (least %u), game over %u, bombs let go %u, short after a bomb %u, chains %u "
			"broken %u\n", cheat, s.lost, s.least_lives, s.game_over, s.bombs_let_go, s.bombs_short, s.chains,
			s.chain_breaks);
	if (shot)
	{
		static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];

		video_latch_sprites();
		video_draw(frame);
		video_rotated(frame, picture);
		save_ppm(shot, picture, 240, 320);
	}
	return s;
}

static int find(const char *name)
{
	for (int index = 0; index < cheats_count(); index++)
		if (!strcmp(cheats_name(index), name))
			return index;
	return -1;
}

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-44s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 3600;
	const bool shots = getenv("SHOTS") != NULL;
	seen with, without;

	own_cheats_add();
	cheats_load("no such file");
	printf("%d cheats without a cheat file\n", cheats_count());

	without = play(-1, -1, frames, shots ? "cheat_none.ppm" : NULL);
	printf("no cheat: %u lives lost (least %u), game over %u, bombs let go %u, short after a bomb %u, frames "
		"flying below MAX %u, chains %u broken %u\n", without.lost, without.least_lives, without.game_over,
		without.bombs_let_go, without.bombs_short, without.power_short, without.chains, without.chain_breaks);

	with = play(find("P1 INFINITE LIVES"), -1, frames, shots ? "cheat_lives.ppm" : NULL);
	verdict("P1 INFINITE LIVES: lives lost, never below 2", with.lost > 0 && with.least_lives >= 2u && !with.game_over);

	with = play(find("P1 INFINITE BOMBS"), -1, frames, shots ? "cheat_bombs.ppm" : NULL);
	verdict("P1 INFINITE BOMBS: bombs let go, refilled", with.bombs_let_go > without.bombs_let_go
		&& with.bombs_short == 0u && without.bombs_short > 0u);

	with = play(find("P1 INVINCIBLE"), -1, frames, shots ? "cheat_invincible.ppm" : NULL);
	verdict("P1 INVINCIBLE: no life lost", with.lost == 0u && without.lost > 0u);

	with = play(find("P1 MAX POWER"), -1, frames, shots ? "cheat_power.ppm" : NULL);
	verdict("P1 MAX POWER: always MAX while flying", with.power_short == 0u && without.power_short > 0u);

	with = play(find("P1 ENDLESS CHAIN"), find("P1 INVINCIBLE"), frames, shots ? "cheat_chain.ppm" : NULL);
	verdict("P1 ENDLESS CHAIN: a chain, never broken", with.chains > 0u && with.chain_breaks == 0u
		&& without.chain_breaks > 0u);
	with = play(find("P1 INVINCIBLE"), -1, frames, NULL);
	printf("  (invincible alone: chains %u, broken %u)\n", with.chains, with.chain_breaks);

	printf("%s\n", failures ? "FAILED" : "all passed");
	return failures != 0;
}
