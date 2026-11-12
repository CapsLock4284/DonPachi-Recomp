/* targets_find AREA [MINHP]: the training run to AREA's start (at MAX), then the simple player (shielded) to the
   boss; logs each enemy destroyed (a hit record turning destroyed: bit 13 off, bit 8 on) with the object it is part
   of (its type), its most hit points, its place and whether a bomb was going off, each item as it appears (the item
   records from 0x106114: sprite 0361 a power-up, 0379 a bomb), and each life gained. For the achievements' targets */
#include "common.h"
#include "training.h"

static uint32_t rec_at(int r)
{
	return r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
}

/* the object (slot, type) whose hit records hold record 'at', or -1 */
static int owner(uint32_t at, unsigned *type)
{
	int best = -1;
	uint32_t best_start = 0;

	for (int i = 0; i < 64; i++)
	{
		const uint32_t o = 0x100102u + 0x40u * (uint32_t)i;
		const uint32_t hits = (uint32_t)ram_word(o + 6u) << 16 | ram_word(o + 8u);

		if ((ram_word(o) & 0x8000u) && hits <= at && at < hits + 0x20u * 16u && hits >= best_start)
			best = i, best_start = hits, *type = ram_word(o + 0xCu) >> 8;
	}
	return best;
}

int main(int argc, char **argv)
{
	const training_setup setup = { atoi(argv[1]), 0, 0, TRAINING_POWER_MAX, false, 0 };
	const int min_hp = argc > 2 ? atoi(argv[2]) : 50;
	static const int boss[5] = { 555, 576, 629, 888, 539 };
	unsigned last_word[150] = { 0 }, last_type[150] = { 0 };
	int most_hp[150] = { 0 }, last_slot[150];
	uint16_t items[16] = { 0 };
	unsigned lives = 99;
	long pending = -1;
	uint32_t score_before = 0;

	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 40000; f++)
	{
		const int scroll = (int16_t)ram_word(0x101A56u);
		const bool bombing = (ram_word(0x106E14u) & 0x8000u) != 0;

		if (scroll >= boss[setup.area % 5] || ram_word(0x1018ECu) != 7u)
			break;
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		if (getenv("NOBOMB"))
			cave_board.in0 |= CAVE_BUTTON2;
		cave_board.in1 = 0xFFFFu;
		/* (the record's owner before the frame: a destroyed object may be gone after it) */
		for (int r = 0; r < 150; r++)
			if (ram_word(rec_at(r)) & 0x8000u)
				last_slot[r] = owner(rec_at(r), &last_type[r]);
		{
			const uint32_t before = (uint32_t)ram_word(0x10177Au) << 16 | ram_word(0x10177Cu);

			program_run_frame();
			cave_sound_drop();
			if (pending >= 0)
			{
				/* (a kill's points come the frame after: BCD) */
				const uint32_t now = (uint32_t)ram_word(0x10177Au) << 16 | ram_word(0x10177Cu);
				uint32_t a = 0, b = 0;

				for (int d = 7; d >= 0; d--)
					a = a * 10u + (score_before >> (d * 4) & 15u), b = b * 10u + (now >> (d * 4) & 15u);
				printf("        points %u\n", b - a);
				pending = -1;
			}
			score_before = before;
		}
		for (int r = 0; r < 150; r++)
		{
			const uint32_t at = rec_at(r);
			const unsigned w = ram_word(at);
			const int hp = (int16_t)ram_word(at + 0x18u);

			if ((w & 0xA000u) == 0xA000u && hp > most_hp[r] && hp < 32767)
				most_hp[r] = hp;
			if ((last_word[r] & 0xA000u) == 0xA000u && (w & 0x8000u) && !(w & 0x2000u) && (w & 0x0100u))
			{
				if (most_hp[r] >= min_hp)
					printf("scroll %4d frame %5ld: DESTROYED rec %3d slot %2d type %02X hp %5d at x %3d y %3d%s\n",
						scroll, f, r, last_slot[r], last_type[r], most_hp[r], (int16_t)ram_word(at + 8u) >> 6,
						(int16_t)ram_word(at + 6u) >> 6, bombing ? "  (BOMB going off)" : "");
				if (most_hp[r] >= min_hp)
					pending = f;
				most_hp[r] = 0;
			}
			if (!(w & 0x8000u))
				most_hp[r] = 0;
			last_word[r] = w;
		}
		for (int i = 0; i < 16; i++)
		{
			const uint32_t at = 0x106114u + 0x20u * (uint32_t)i;
			const uint16_t sprite = (ram_word(at) & 0x8000u) ? ram_word(at + 4u) : 0u;

			if (sprite && !items[i])
				printf("scroll %4d frame %5ld:   item sprite %04X at x %3d y %3d\n", scroll, f, sprite,
					(int16_t)ram_word(at + 8u) >> 6, (int16_t)ram_word(at + 6u) >> 6);
			items[i] = sprite;
		}
		if (ram_word(0x101A2Eu) != lives)
		{
			if (lives != 99)
				printf("scroll %4d frame %5ld:   LIVES %u -> %u\n", scroll, f, lives, ram_word(0x101A2Eu));
			lives = ram_word(0x101A2Eu);
		}
	}
	return 0;
}
