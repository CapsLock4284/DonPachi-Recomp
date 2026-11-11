/* sfx_log: the sound effects chip's (#1) samples as they start, with where they are in its ROM region: through the
   attract mode, a coin, Start, the ship select (moves down, back up, choice) and some play */
#include "common.h"
#include "cave/rom_set.h"
#include "game.h"

static const unsigned codes[] = { 0x332E, 0x7C13, 0x225D, 0x7E30, 0x3B86, 0x3D2F, 0x1099, 0x6812, 0x7A6D, 0x1685, 0x464F,
	0x5650, 0x3100, 0x1200, 0x2238, 0x2862, 0x7314, 0x501E, 0x4018 };

static uint32_t page_of(uint8_t value)
{
	const uint32_t pages = (DONPACHI_OKI2_SIZE - 0x40000u) >> 16;
	uint32_t mask = pages - 1u, page;

	while (mask & (mask + 1u))
		mask |= mask >> 1;
	page = value & mask;
	while (page >= pages)
	{
		uint32_t high = mask + 1u;

		while (!(page & high))
			high >>= 1;
		page &= ~high;
	}
	return page;
}

int main(void)
{
	bool was[4] = { false };

	start_game();
	for (long f = 0; f < (getenv("CODES") ? 4200 : 3000); f++)
	{
		const unsigned mode = ram_word(0x1018ECu);
		unsigned held = 0;
		static long select_at = -1;

		if (mode != 7u && mode != 0xBu && f > 400)
			held = f % 40 < 5 ? CAVE_COIN : (f % 40 >= 20 && f % 40 < 25 ? CAVE_START : 0u);
		else if (mode == 0xBu)
		{
			if (select_at < 0)
				select_at = f;
			const long t = f - select_at;
			if (t == 60 || t == 100)
				held = CAVE_DOWN;
			else if (t == 140)
				held = CAVE_UP;
			else if (t >= 200 && t < 205)
				held = CAVE_BUTTON1;
		}
		else if (mode == 7u)
			held = autoplayer(f, 1);
		if (getenv("CODES") && mode == 7u && f >= 1100 && (f - 1100) % 150 == 0 && (f - 1100) / 150 < (long)(sizeof(codes) / sizeof(codes[0])))
		{
			printf("-- play_sound(%04X)\n", codes[(f - 1100) / 150]);
			play_sound((uint16_t)codes[(f - 1100) / 150], 0);
		}
		if (getenv("CODES") && mode == 7u)
			held = 0;
		cave_board.in0 = (uint16_t)~held;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		for (int v = 0; v < 4; v++)
		{
			const cave_oki_voice *const voice = &cave_board.sound.oki[1].voice[v];
			static uint32_t last_sample[4], last_base[4];
			const bool fresh = voice->playing && (!was[v] || voice->sample < last_sample[v] || voice->base != last_base[v]);

			if (fresh)
			{
				const uint32_t at = (page_of(cave_board.sound.bank[4 + ((voice->base >> 16) & 3u)]) << 16)
					| (voice->base & 0xFFFFu);

				printf("frame %4ld mode %X input %03X: voice %d region %06X, %u nibbles (%.2f s), volume %d\n", f, mode,
					held, v, at, voice->count, voice->count / (4220000.0 / 2 / 132), voice->volume);
			}
			was[v] = voice->playing;
			last_sample[v] = voice->sample;
			last_base[v] = voice->base;
		}
	}
	return 0;
}
