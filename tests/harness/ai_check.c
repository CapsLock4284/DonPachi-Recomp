/* ai_check [AREA FRAMES]: the computer's player 2 (ports/sdl2/ai_player.c) beside the simple player 1 (shielded),
   from AREA's start (the training's way there; default area 1; PLACE=2: its boss) for FRAMES (default 5400: about 1.5 minutes): whether
   it joined, its deaths, bombs and score; SHOT=frame: a picture then (ai.ppm). PASS: joined, and on average more
   than 20 seconds a life */
#include "common.h"
#include "training.h"
#include "ai_player.h"
#include "game_mode.h"

#include "cave/program.h"
#include "cave/video.h"

#define P2_SHIP 0x102198u

int main(int argc, char **argv)
{
	const training_setup setup = { argc > 1 ? atoi(argv[1]) - 1 : 0, getenv("PLACE") ? atoi(getenv("PLACE")) : 0, 0,
		TRAINING_POWER_MAX, false, 0 };
	const long frames = argc > 2 ? atol(argv[2]) : 5400;
	const long shot_at = getenv("SHOT") ? atol(getenv("SHOT")) : -1;
	long joined_at = -1, flying_frames = 0;
	unsigned deaths = 0, bombs = 0, last_bombs = 0, game_overs = 0, last_inputs = 0;
	long laser_frames = 0;
	bool was_flying = false;

	start_game();
	training_prepare(&setup);
	ai_player_reset();
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		{
			const unsigned inputs = ai_player_inputs();

			if ((inputs & CAVE_BUTTON1) && (last_inputs & CAVE_BUTTON1))
				laser_frames++;    /* (the shot held: the laser) */
			last_inputs = inputs;
			cave_board.in1 = (uint16_t)~inputs;
		}
		program_run_frame();
		cave_sound_drop();
		if (f == shot_at)
		{
			static uint32_t board[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];

			video_latch_sprites();
			video_draw(board);
			video_rotated(board, picture);
			save_ppm("ai.ppm", picture, 240, 320);
		}
		if (program_frame_lagged())
			continue;
		{
			const bool flying = (ram_word(P2_SHIP) & 0x8000u) != 0;
			const unsigned b = ram_word(P2_SHIP + 0x22u);

			if (flying && joined_at < 0)
				joined_at = f;
			if (flying)
				flying_frames++;
			if (was_flying && !flying && game_mode_playing())
			{
				deaths++;
				if (*cave_work_ram_byte(0x101A31u) == 0u)
					game_overs++;
			}
			if (flying && was_flying && b + 1u == last_bombs)
				bombs++;
			was_flying = flying;
			last_bombs = b;
		}
	}
	{
		const uint32_t score = (uint32_t)ram_word(0x10177Eu) << 16 | ram_word(0x101780u);
		const double per_life = (double)flying_frames / 57.55 / (double)(deaths + 1u);

		printf("area %d, %ld frames: joined at frame %ld, flying %.0f s, deaths %u (game overs %u), bombs %u, "
			"score %08X (BCD); %.0f s a life; laser %.0f s; stage now %u\n", setup.area + 1, frames, joined_at,
			(double)flying_frames / 57.55, deaths, game_overs, bombs, score, per_life, (double)laser_frames / 57.55,
			ram_word(0x101A40u));
		printf("%s\n", joined_at >= 0 && per_life > 20.0 ? "PASS" : "FAIL");
		return joined_at >= 0 && per_life > 20.0 ? 0 : 1;
	}
}
