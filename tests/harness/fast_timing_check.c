/* fast_timing_check [FRAMES]: the picture with fast timing (drawn as the program's frame ends, the scroll and the
   palette put in as the vblank interrupt is about to: ports/sdl2/fast_timing.c) against the picture with arcade
   timing (drawn as the interrupt returns), frame by frame, the simple player (shielded) for FRAMES (default 6000)
   from the power on: the same in every frame; and without fast_timing.c's help (as before 0.7.0), how many
   differed. PASS or FAIL */
#include "common.h"
#include "fast_timing.h"

#include "cave/program.h"
#include "cave/video.h"

static uint32_t fast[VIDEO_WIDTH * VIDEO_HEIGHT], plain[VIDEO_WIDTH * VIDEO_HEIGHT],
	arcade[VIDEO_WIDTH * VIDEO_HEIGHT];
static bool have;
static long frames_compared, fast_differ, plain_differ;

static void after_vblank(void)
{
	if (!have)
		return;
	video_draw(arcade);
	frames_compared++;
	fast_differ += memcmp(fast, arcade, sizeof(arcade)) != 0;
	plain_differ += memcmp(plain, arcade, sizeof(arcade)) != 0;
	have = false;
}

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 6000;

	start_game();
	program_after_vblank = after_vblank;
	for (long f = 0; f < frames; f++)
	{
		cave_board.in0 = (uint16_t)~autoplayer(f, 1);
		program_run_frame();
		video_latch_sprites();    /* (VBLANK: the 013 takes the sprites) */
		video_draw(plain);
		fast_timing_begin();
		video_draw(fast);
		fast_timing_end();
		have = true;
	}
	printf("%ld frames: fast timing's picture differed from arcade timing's in %ld (without fast_timing.c: %ld)\n",
		frames_compared, fast_differ, plain_differ);
	printf("%-60s %s\n", "fast timing's picture: arcade timing's, every frame", fast_differ == 0 ? "PASS" : "FAIL");
	return fast_differ != 0;
}
