/* fx_probe AREA: the records at 104AD4 (30) and 106914 (40) as they start: their kind and words, with the score
   and the chain's hits then (a kill?), the simple player shielded */
#include "common.h"
#include "training.h"
int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), 0, 0, TRAINING_POWER_MAX, false, 0 };
	unsigned a[30] = { 0 }, b[40] = { 0 }, hits = 0;
	int ca[16] = { 0 }, cb[16] = { 0 };
	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 12000; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		program_run_frame();
		cave_sound_drop();
		const unsigned h = ram_word(0x10174Eu);
		for (int i = 0; i < 30; i++)
		{
			const uint32_t at = 0x104AD4u + 0x20u * (uint32_t)i; const unsigned w = ram_word(at);
			if ((w & 0x8000u) && !(a[i] & 0x8000u)) { ca[w & 15]++; if (ca[w & 15] <= 3) printf("%6ld A kind %u words %04X %04X %04X %04X %04X %04X hits %u->%u\n", f, w & 15, ram_word(at+2), ram_word(at+4), ram_word(at+6), ram_word(at+8), ram_word(at+0xA), ram_word(at+0xC), hits, h); }
			a[i] = w;
		}
		for (int i = 0; i < 40; i++)
		{
			const uint32_t at = 0x106914u + 0x20u * (uint32_t)i; const unsigned w = ram_word(at);
			if ((w & 0x8000u) && !(b[i] & 0x8000u)) { cb[w & 15]++; if (cb[w & 15] <= 3) printf("%6ld B kind %u words %04X %04X %04X %04X %04X %04X\n", f, w & 15, ram_word(at+2), ram_word(at+4), ram_word(at+6), ram_word(at+8), ram_word(at+0xA), ram_word(at+0xC)); }
			b[i] = w;
		}
		hits = h;
	}
	printf("A kinds:"); for (int k = 0; k < 16; k++) if (ca[k]) printf(" %d:%d", k, ca[k]);
	printf("\nB kinds:"); for (int k = 0; k < 16; k++) if (cb[k]) printf(" %d:%d", k, cb[k]);
	printf("\n");
	return 0;
}
