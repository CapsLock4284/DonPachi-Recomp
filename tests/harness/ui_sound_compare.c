/* ui_sound_compare: the game's own cursor sound (the ship select, the stick down) as the board makes it, alone, against
   ui_sound.c's decoding of it: their best match (correlation) */
#include "common.h"
#include "ui_sound.h"
#include "cave/sound.h"
#include <math.h>

int main(void)
{
	static int16_t board[48000], ours[48000];
	unsigned got = 0, mine = 0;
	long select_at = -1;

	start_game();
	for (long f = 0; f < 2000 && got < 20000u; f++)
	{
		const unsigned mode = ram_word(0x1018ECu);
		unsigned held = 0;

		if (mode != 7u && mode != 0xBu && f > 400)
			held = f % 40 < 5 ? CAVE_COIN : (f % 40 >= 20 && f % 40 < 25 ? CAVE_START : 0u);
		if (mode == 0xBu && select_at < 0)
			select_at = f;
		if (select_at >= 0 && f - select_at == 60)
			held = CAVE_DOWN;
		for (int v = 0; v < 4; v++)
			cave_board.sound.oki[0].voice[v].playing = false;    /* (the music off) */
		if (select_at >= 0 && f - select_at == 60)
			for (int v = 0; v < 4; v++)
				cave_board.sound.oki[1].voice[v].playing = false;
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		if (select_at >= 0 && f - select_at >= 60)
			got += cave_sound_take(board + got, 48000u - got);
		else
			cave_sound_drop();
	}
	ui_sound_play(UI_SOUND_MOVE);
	while (ui_sound_playing() && mine + 800u < 48000u)
	{
		ui_sound_mix(ours + mine, 800u);
		mine += 800u;
	}
	if (getenv("DUMP"))
	{
		FILE *f = fopen("board.raw", "wb"); fwrite(board, 2, got, f); fclose(f);
		f = fopen("ours.raw", "wb"); fwrite(ours, 2, mine, f); fclose(f);
	}
	{
		double best = 0;
		int best_lag = 0;

		for (int lag = 0; lag < 4000; lag++)
		{
			double sab = 0, saa = 0, sbb = 0;

			for (unsigned i = 0; i < mine && i + lag < got; i++)
			{
				sab += (double)board[i + lag] * ours[i];
				saa += (double)board[i + lag] * board[i + lag];
				sbb += (double)ours[i] * ours[i];
			}
			if (saa > 0 && sbb > 0 && sab / sqrt(saa * sbb) > best)
				best = sab / sqrt(saa * sbb), best_lag = lag;
		}
		printf("board %u samples, ours %u: best correlation %.4f at %d samples in\n", got, mine, best, best_lag);
	}
	return 0;
}
