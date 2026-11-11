/* ui_sounds_wav: the port's own sounds (ui_sound.c) as they play, each to ui_sound_N.wav (48 kHz mono), to listen to */
#include "common.h"
#include "ui_sound.h"
#include "cave/sound.h"

static void put32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f); fputc(v >> 24, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }

int main(void)
{
	static const char *const names[UI_SOUNDS] = { "move", "choose", "back", "achievement" };

	start_game();
	for (int sound = 0; sound < UI_SOUNDS; sound++)
	{
		static int16_t samples[CAVE_SOUND_RATE * 3];
		char name[48];
		unsigned count = 0;
		FILE *f;

		memset(samples, 0, sizeof(samples));
		ui_sound_play((ui_sound)sound);
		while (ui_sound_playing() && count + 800u < sizeof(samples) / 2u)
		{
			ui_sound_mix(samples + count, 800u);
			count += 800u;
		}
		snprintf(name, sizeof(name), "ui_sound_%d_%s.wav", sound, names[sound]);
		f = fopen(name, "wb");
		fputs("RIFF", f); put32(f, 36u + count * 2u); fputs("WAVEfmt ", f); put32(f, 16); put16(f, 1); put16(f, 1);
		put32(f, CAVE_SOUND_RATE); put32(f, CAVE_SOUND_RATE * 2u); put16(f, 2); put16(f, 16); fputs("data", f);
		put32(f, count * 2u);
		fwrite(samples, 2, count, f);
		fclose(f);
		{
			int peak = 0;

			for (unsigned i = 0; i < count; i++)
				peak = abs(samples[i]) > peak ? abs(samples[i]) : peak;
			printf("%s: %.2f s, peak %d\n", name, count / (double)CAVE_SOUND_RATE, peak);
		}
	}
	return 0;
}
