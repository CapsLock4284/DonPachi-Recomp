/* sound_timing_check: where a command to the sound chips takes effect in the frame (cave/sound.h). With the
   68000's clock (cave_sound_clock), a play command given halfway through a frame starts its sound halfway through
   that frame's samples (as MAME's stream update has it); without it (the reference's way), from that frame's start
   (all of a frame's samples made as it ends).
   Then a game's sound (the attract mode, 3000 frames) as WAVE files: sound_timing_new.wav (the port's way) and
   sound_timing_frame.wav (every command from its frame's start), to listen to. PASS or FAIL */
#include "common.h"

#include "cave/program.h"
#include "cave/sound.h"

#define FRAME_CYCLES 278016u

static uint64_t fake_time;
static int failures;

static uint64_t fake_now(void)
{
	return fake_time;
}

static void verdict(const char *what, bool pass)
{
	printf("%-60s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

/* a sample of chip 0 (not paged) with a sound: its number */
static int some_sample(void)
{
	for (int number = 1; number < 128; number++)
	{
		const uint8_t *const table = rom_set.oki[0] + 0x40000u + (uint32_t)number * 8u;
		const uint32_t start = ((uint32_t)table[0] << 16 | table[1] << 8 | table[2]) & 0x3FFFFu;
		const uint32_t stop = ((uint32_t)table[3] << 16 | table[4] << 8 | table[5]) & 0x3FFFFu;

		if (start < stop && stop - start > 2000u)
			return number;
	}
	return -1;
}

/* the first output sample (from the frame's first) loud enough: where the sound begins */
static int first_sound(const int16_t *out, unsigned count)
{
	for (unsigned index = 0; index < count; index++)
		if (out[index] > 300 || out[index] < -300)
			return (int)index;
	return -1;
}

/* a play command at 'fraction' of a frame: where its sound starts in the output, from that frame's first sample */
static int started_at(bool clock, double fraction, int number)
{
	static int16_t out[8192];
	unsigned count = 0;

	cave_sound_reset();
	cave_sound_clock(clock ? fake_now : NULL, FRAME_CYCLES);
	fake_time = 1000000u;
	cave_sound_frame();    /* (a frame of silence: the frames are counted from here) */
	cave_sound_drop();
	fake_time += (uint64_t)(FRAME_CYCLES * fraction);
	cave_sound_command(0, (uint8_t)(0x80u | (unsigned)number));
	cave_sound_command(0, 0x10u);    /* (voice 0, full volume) */
	fake_time = 1000000u + FRAME_CYCLES;
	cave_sound_frame();
	fake_time += FRAME_CYCLES;
	cave_sound_frame();
	count = cave_sound_take(out, 8192);
	return first_sound(out, count);
}

static void write_wav(const char *path, bool clock)
{
	FILE *const file = fopen(path, "wb");
	static int16_t out[8192];
	uint32_t bytes = 0;
	const uint8_t header[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
		1, 0, 1, 0, 0x80, 0xBB, 0, 0, 0, 0x77, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0 };

	fwrite(header, 1, 44, file);
	program_reset();
	cave_sound_drop();
	if (!clock)
		cave_sound_clock(NULL, 0);
	for (int frame = 0; frame < 3000; frame++)
	{
		unsigned count;

		program_run_frame();
		count = cave_sound_take(out, 8192);
		fwrite(out, 2, count, file);
		bytes += 2u * count;
	}
	fseek(file, 4, SEEK_SET);
	{
		const uint32_t riff = bytes + 36u;

		fwrite(&riff, 4, 1, file);
		fseek(file, 40, SEEK_SET);
		fwrite(&bytes, 4, 1, file);
	}
	fclose(file);
	printf("wrote %s (%u samples)\n", path, bytes / 2u);
}

int main(void)
{
	const int per_frame = 48000 * 17376 / 1000000;    /* (833 outputs a frame) */
	int number, half, quarter, end;

	start_game();
	write_wav("sound_timing_new.wav", true);      /* (the program's clock, as program_start gives it) */
	write_wav("sound_timing_frame.wav", false);
	number = some_sample();
	printf("chip 0's sample %d\n", number);
	half = started_at(true, 0.5, number);
	quarter = started_at(true, 0.25, number);
	end = started_at(false, 0.5, number);
	printf("a command halfway through a frame: its sound at output %d (the frame has %d); a quarter: %d;\n"
		"without the clock: %d\n", half, per_frame, quarter, end);
	verdict("halfway: the sound starts in the middle of its frame", half > per_frame * 45 / 100 && half < per_frame * 60 / 100);
	verdict("a quarter: a quarter of a frame earlier", abs((half - quarter) - per_frame / 4) < 12);
	verdict("without the clock: from its frame's start", end >= 0 && end < 30);
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
