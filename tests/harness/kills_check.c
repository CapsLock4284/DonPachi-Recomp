/* kills_check FRAMES: autoplay (shielded) from the attract mode, counting the enemies destroyed as the statistics do
   (stats.c): a hit record that could be shot (bits 15 and 13) turning to destroyed (bit 13 off, bit 8 on).
     LOG=1 each kill, SCORES=1 each score change (a kill's points come the frame after), TRACE=frame the hit records
     that change in the 40 frames from there */
#include "common.h"

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 3000;
	int last_hp[150];
	unsigned last_word[150] = { 0 };
	bool last_live[150] = { false };
	long kills = 0, scoring_frames = 0, kill_frames = 0;
	uint32_t last_score = 0;

	start_game();
	for (long f = 0; f < frames; f++)
	{
		int now = 0;
		uint32_t score;

		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		for (int r = 0; r < 150; r++)
		{
			const uint32_t at = r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
			const bool live = (ram_word(at) & 0x8000u) != 0;
			const int hp = (int16_t)ram_word(at + 0x18u);

			if ((last_word[r] & 0xA000u) == 0xA000u && live && !(ram_word(at) & 0x2000u) && (ram_word(at) & 0x0100u))
				now++;    /* (shootable, then dead: bit 13 off, bit 8 on) */
			if (getenv("TRACE") && f >= atol(getenv("TRACE")) && f < atol(getenv("TRACE")) + 40
					&& (ram_word(at) != last_word[r] || hp != last_hp[r]))
				printf("  frame %ld rec %3d: %04X %d -> %04X %d\n", f, r, last_word[r], last_hp[r], ram_word(at), hp);
			last_word[r] = ram_word(at);
			last_live[r] = live;
			last_hp[r] = hp;
		}
		score = (uint32_t)ram_word(0x10177Au) << 16 | ram_word(0x10177Cu);
		if (now)
		{
			kills += now;
			kill_frames++;
			if (getenv("LOG"))
				printf("frame %ld: %d kill(s), score %X -> %X\n", f, now, last_score, score);
		}
		if (score != last_score && getenv("SCORES"))
			printf("frame %ld: score %X -> %X\n", f, last_score, score);
		if (score != last_score && ram_word(0x1018ECu) == 7u)
			scoring_frames++;
		last_score = score;
	}
	printf("%ld kills in %ld frames; %ld frames the score went up\n", kills, kill_frames, scoring_frames);
	return 0;
}
