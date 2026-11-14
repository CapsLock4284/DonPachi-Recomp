/* items_probe AREA: the simple player (unshielded) from AREA's start: each item as it comes (its kind, +0 low bits,
   its sprite +4), and each life lost */
#include "common.h"
#include "training.h"
#include "game_mode.h"
int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), 0, 0, 0, false, 0 };
	unsigned was[16] = { 0 }, lives = 0;
	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 30000; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 0);
		program_run_frame();
		cave_sound_drop();
		if (ram_byte(0x101A2Fu) != lives) { printf("%6ld lives %u\n", f, ram_byte(0x101A2Fu)); lives = ram_byte(0x101A2Fu); }
		for (int i = 0; i < 16; i++)
		{
			const uint32_t at = 0x106114u + 0x20u * (uint32_t)i;
			const unsigned w = ram_word(at);
			if ((w & 0x8000u) && !(was[i] & 0x8000u))
				printf("%6ld item %2d kind %u sprite %04X scroll %d\n", f, i, w & 0xFu, ram_word(at + 4u), (int16_t)ram_word(0x101A56u));
			was[i] = w;
		}
		if (!game_mode_playing()) break;
	}
	return 0;
}
