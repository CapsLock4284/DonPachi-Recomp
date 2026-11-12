/* targets_hunt AREA [MINHP MAXHP]: the training run to AREA's start (at MAX), then a hunter (shielded, never bombing):
   each enemy with MINHP-MAXHP hit points (default 300-5000) is, as it comes on screen, left at 1 hit point and shot
   from under it; logs each kill (the record, its owner's type, its hit points, its points) and each item or life it
   gives. An enemy not destroyed in 4 seconds is left. DELAY=n waits until it has been on the screen n frames. STILL=n waits for the enemy to stand still on the screen n frames first (the hover boat's bomb). LOOP=1
   the second loop's area (AREA + 5). For the achievements' targets: which enemy gives the 1-Up, the bombs */
#include "common.h"
#include "training.h"

static uint32_t rec_at(int r)
{
	return r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
}

static unsigned owner_type(uint32_t at)
{
	unsigned type = 0xFFu;
	uint32_t best_start = 0;

	for (int i = 0; i < 64; i++)
	{
		const uint32_t o = 0x100102u + 0x40u * (uint32_t)i;
		const uint32_t hits = (uint32_t)ram_word(o + 6u) << 16 | ram_word(o + 8u);

		if ((ram_word(o) & 0x8000u) && hits <= at && at < hits + 0x20u * 16u && hits >= best_start)
			best_start = hits, type = ram_word(o + 0xCu) >> 8;
	}
	return type;
}

static uint32_t score(void)
{
	const uint32_t bcd = (uint32_t)ram_word(0x10177Au) << 16 | ram_word(0x10177Cu);
	uint32_t value = 0;

	for (int d = 7; d >= 0; d--)
		value = value * 10u + (bcd >> (d * 4) & 15u);
	return value;
}

int main(int argc, char **argv)
{
	const int area = atoi(argv[1]) + (getenv("LOOP") ? 5 : 0);
	const training_setup setup = { area, 0, 0, TRAINING_POWER_MAX, false, 0 };
	const int min_hp = argc > 2 ? atoi(argv[2]) : 300, max_hp = argc > 3 ? atoi(argv[3]) : 5000;
	const int still_frames = getenv("STILL") ? atoi(getenv("STILL")) : 0;
	const int delay = getenv("DELAY") ? atoi(getenv("DELAY")) : 0;
	static const int boss[5] = { 555, 576, 629, 888, 539 };
	bool done[150] = { false };
	int still[150] = { 0 }, seen_for[150] = { 0 }, last_x[150], last_y[150], target = -1, report = 0;
	long target_since = 0;
	uint16_t items[48] = { 0 };
	unsigned lives = 99, target_type = 0;
	uint32_t before = 0;

	start_game();
	training_prepare(&setup);
	for (long f = 0; f < 40000; f++)
	{
		const int scroll = (int16_t)ram_word(0x101A56u);
		const int sx = (int16_t)ram_word(0x102158u + 8u) >> 6, sy = (int16_t)ram_word(0x102158u + 6u) >> 6;
		unsigned in = CAVE_BUTTON2 * 0;

		if (scroll >= boss[area % 5] || ram_word(0x1018ECu) != 7u)
			break;
		*cave_work_ram_byte(0x102196u) = 2;
		for (int r = 0; r < 150; r++)
		{
			const uint32_t at = rec_at(r);
			const int x = (int16_t)ram_word(at + 8u) >> 6, y = (int16_t)ram_word(at + 6u) >> 6;

			still[r] = (ram_word(at) & 0x8000u) && x == last_x[r] && y == last_y[r] ? still[r] + 1 : 0;
			seen_for[r] = (ram_word(at) & 0x8000u) && x > 8 && x < 232 && y > 16 && y < 240 ? seen_for[r] + 1 : 0;
			last_x[r] = x, last_y[r] = y;
			if (!(ram_word(at) & 0x8000u))
				done[r] = false;
		}
		if (target >= 0 && (ram_word(rec_at(target)) & 0xA000u) != 0xA000u)
		{
			report = 2;
			done[target] = true;
			target = -1;
		}
		if (target < 0)
			for (int r = 0; r < 150 && target < 0; r++)
			{
				const uint32_t at = rec_at(r);
				const int hp = (int16_t)ram_word(at + 0x18u), x = last_x[r], y = last_y[r];

				if (!done[r] && (ram_word(at) & 0xA000u) == 0xA000u && hp >= min_hp && hp <= max_hp && x > 8 && x < 232
						&& y > 16 && y < 240 && still[r] >= still_frames && seen_for[r] >= delay)
				{
					target = r;
					target_type = owner_type(at);
					printf("scroll %4d frame %5ld: target rec %3d type %02X hp %d at %d,%d\n", scroll, f, r, target_type,
						hp, x, y);
					before = score();
					target_since = f;
				}
			}
		if (target >= 0)
		{
			const uint32_t at = rec_at(target);
			const int x = last_x[target];

			if ((int16_t)ram_word(at + 0x18u) > 1)
				*(uint16_t *)cave_work_ram_at(at + 0x18u, 2) = 1;
			if (sx < x - 3)
				in |= CAVE_RIGHT;
			if (sx > x + 3)
				in |= CAVE_LEFT;
			if (sy < 280)
				in |= CAVE_DOWN;
			in |= (f / 2) % 2 ? CAVE_BUTTON1 : 0u;
			if (last_y[target] > 240 || last_y[target] < 0 || x < 0 || x > 240 || f - target_since > 240)
			{
				done[target] = true;    /* (gone off the screen) */
				target = -1;
			}
		}
		cave_board.in0 = (uint16_t)~in;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		if (report && --report == 0)
			printf("        destroyed: points %u\n", score() - before);
		for (int i = 0; i < 48; i++)
		{
			const uint32_t at = 0x106114u + 0x20u * (uint32_t)i;
			const uint16_t sprite = (ram_word(at) & 0x8000u) ? ram_word(at + 4u) : 0u;

			if (sprite && !items[i])
				printf("        item sprite %04X at %d,%d\n", sprite, (int16_t)ram_word(at + 8u) >> 6, (int16_t)ram_word(at + 6u) >> 6);
			items[i] = sprite;
		}
		if (ram_word(0x101A2Eu) != lives)
		{
			if (lives != 99)
				printf("        LIVES %u -> %u\n", lives, ram_word(0x101A2Eu));
			lives = ram_word(0x101A2Eu);
		}
	}
	return 0;
}
