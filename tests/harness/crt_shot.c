/* crt_shot [AREA FRAMES]: the CRT look (ports/sdl2/crt.c) drawn by SDL's software renderer: the game played to an
   area (the training's way, then the simple player for FRAMES), its picture shown with CRT light and full at
   1080 pixels high (crt_light.ppm, crt_full.ppm), at 2160 (crt_full_4k.ppm), turned (crt_turned.ppm: the monitor
   on its side, 1920 x 1080) and at 1x, too small for the lines (crt_small.ppm); plain, for comparison
   (crt_plain.ppm). Prints how long a frame of it takes */
#include "common.h"
#include "training.h"
#include "crt.h"

#include "cave/video.h"

#include <SDL.h>
#include <time.h>

static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];

static void shoot(const char *name, int out_w, int out_h, int turn, int level)
{
	SDL_Surface *const surface = SDL_CreateRGBSurfaceWithFormat(0, out_w, out_h, 32, SDL_PIXELFORMAT_ARGB8888);
	SDL_Renderer *const renderer = SDL_CreateSoftwareRenderer(surface);
	SDL_Texture *const texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		VIDEO_HEIGHT, VIDEO_WIDTH);
	/* the picture as big as fits, upright (or turned: 320 x 240 on the screen) */
	const float shown_w = turn ? VIDEO_WIDTH : VIDEO_HEIGHT, shown_h = turn ? VIDEO_HEIGHT : VIDEO_WIDTH;
	const float scale = (float)out_w / shown_w < (float)out_h / shown_h ? (float)out_w / shown_w : (float)out_h / shown_h;
	SDL_FRect unturned;
	clock_t start;

	SDL_UpdateTexture(texture, NULL, picture, VIDEO_HEIGHT * (int)sizeof(uint32_t));
	unturned.w = VIDEO_HEIGHT * scale;
	unturned.h = VIDEO_WIDTH * scale;
	unturned.x = (float)out_w / 2.0f - unturned.w / 2.0f;
	unturned.y = (float)out_h / 2.0f - unturned.h / 2.0f;
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	start = clock();
	if (level == CRT_OFF)
		SDL_RenderCopyExF(renderer, texture, NULL, &unturned, (double)turn, NULL, SDL_FLIP_NONE);
	else if (!crt_draw(renderer, texture, picture, unturned, turn, level))
		printf("%s: crt_draw couldn't (%s)\n", name, SDL_GetError());
	SDL_RenderPresent(renderer);
	printf("%s: %d x %d, %.1f ms (the software renderer)\n", name, out_w, out_h,
		(double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC);
	save_ppm(name, surface->pixels, out_w, out_h);
	crt_reset();
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_FreeSurface(surface);
}

int main(int argc, char **argv)
{
	const training_setup setup = { argc > 1 ? atoi(argv[1]) : 2, 0, 0, TRAINING_POWER_MAX, false, 0 };
	const long frames = argc > 2 ? atol(argv[2]) : 1800;

	start_game();
	training_prepare(&setup);
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
	}
	video_latch_sprites();
	video_draw(frame);
	video_rotated(frame, picture);
	shoot("crt_plain.ppm", 810, 1080, 0, CRT_OFF);
	shoot("crt_light.ppm", 810, 1080, 0, CRT_LIGHT);
	shoot("crt_full.ppm", 810, 1080, 0, CRT_FULL);
	shoot("crt_full_4k.ppm", 1620, 2160, 0, CRT_FULL);
	shoot("crt_turned.ppm", 1920, 1080, 90, CRT_FULL);
	shoot("crt_small.ppm", 240, 320, 0, CRT_FULL);
	return 0;
}
