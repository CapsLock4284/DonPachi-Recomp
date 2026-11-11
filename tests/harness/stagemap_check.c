/* stagemap_check: the stage maps made as the port makes them (stagemap.c), saved (stagemap_check.dat) and read back;
   each area's picture with the screen's place at a few scroll values marked (stagemap_N.ppm), and the time it took */
#include "common.h"
#include "stagemap.h"

#include <time.h>

int main(void)
{
	const clock_t started = clock();
	const char *problem;

	start_game();
	if ((problem = stagemap_build("stagemap_check.dat", NULL)) != NULL)
	{
		printf("build: %s\n", problem);
		return 1;
	}
	printf("made in %.1f s; read back: %s\n", (double)(clock() - started) / CLOCKS_PER_SEC,
		stagemap_load("stagemap_check.dat") ? "ok" : "FAILED");
	for (int stage = 0; stage < 5; stage++)
	{
		const int height = stagemap_height(stage);
		const uint16_t *const pixels = stagemap_pixels(stage);
		uint32_t *out = malloc(sizeof(uint32_t) * STAGEMAP_WIDTH * (size_t)height);
		char name[32];

		for (int at = 0; at < STAGEMAP_WIDTH * height; at++)
		{
			const unsigned p = pixels[at];

			out[at] = (uint32_t)((p >> 11) << 3) << 16 | (uint32_t)((p >> 5 & 63u) << 2) << 8 | (uint32_t)((p & 31u) << 3);
		}
		/* (the screen at the start, at 0, half way, at the boss: a line at its top) */
		{
			static const int boss[5] = { 555, 576, 629, 888, 539 };
			const int at[4] = { -41, 0, boss[stage] / 2, boss[stage] };

			for (int k = 0; k < 4; k++)
			{
				const int top = stagemap_screen_top(stage, at[k]);

				for (int x = 0; x < STAGEMAP_WIDTH; x++)
				{
					if (top < height)
						out[top * STAGEMAP_WIDTH + x] = 0xFF0000u;
					if (top + STAGEMAP_SCREEN < height)
						out[(top + STAGEMAP_SCREEN) * STAGEMAP_WIDTH + x] = 0xFFFF00u;
				}
				printf("area %d: scroll %d -> screen top %d of %d\n", stage + 1, at[k], top, height);
			}
		}
		snprintf(name, sizeof(name), "stagemap_%d.ppm", stage + 1);
		save_ppm(name, out, STAGEMAP_WIDTH, height);
		free(out);
	}
	return 0;
}
