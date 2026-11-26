/**
 *    Name:        playtest.c
 *    Description: --playtest: the game played through by itself, player 1 kept from harm (playtest.h)
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    The game's memory it reads (achievements.c and training.c have how each was found): $1018EC the mode (0xB the
 *    ship select, 7 player 1 playing, 8 the continue's count), $101A40 the stage (0-4; 5 between the loops and after
 *    the last), $101A42 the loop, $102158 player 1's ship's record (+0 bit 15 flying, bit 10 at MAX; +6 y, +8 x; +$21
 *    the power; +$1C the ship, 0-2; +$3C the power-ups toward the next level; +$3E the respawn shield), $101A86 at
 *    MAX, $100102 the objects (64 of $40: +0 bit 15 in use, +C's high byte the type; $7C area 10's true last boss),
 *    the hit records $103814 (100) and $104494 (50, the bosses') of $20 (+0 bits 15 and 13 shootable, +6 y, +8 x,
 *    +$18 hit points). Checked 25-11-2026: types A, B and C to the ending (the US and Japanese versions), 0:56-1:04 of
 *    the game's time each, about 55 times the arcade's speed on Claude's workspace
 */
#include "playtest.h"
#include "game_mode.h"

#include "cave/machine.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAGE           0x101A40u
#define LOOP            0x101A42u
#define SHIP            0x102158u
#define SHIELD          (SHIP + 0x3Eu)    /* (byte) frames the ship cannot be hit */
#define SHIELD_ON       2u
#define POWER           (SHIP + 0x21u)    /* (byte) 0-4 */
#define POWER_UPS       (SHIP + 0x3Cu)
#define AT_MAX          0x101A86u
#define SHIP_TYPE       (SHIP + 0x1Cu)    /* the ship, 0-2: types A, B, C (stats.c) */
#define OBJECTS         0x100102u
#define TRUE_BOSS       0x7Cu
#define FRAMES_A_SECOND 57.55
#define STUCK_FRAMES    (30L * 60L * 58L)    /* an area this long (30 minutes of the game's): stuck */
#define ENDING_MOST     (10L * 60L * 58L)    /* the ending's way back to the title at most */
#define PLACE_STEADY    30L                  /* a place counted once the stage and loop have held it this long */
#define RUNS            3

static const char *const run_names[RUNS] = { "type A", "type B", "type C" };

enum { WAITING, SELECTING, PLAYING, ENDING };

static int first_run, last_run, run;
static int phase;
static long frame, phase_frames, select_frames, play_frames, since_progress, run_began, seen_for;
static int place_loop = -1, place_stage = -1, seen_loop = -1, seen_stage = -1;
static bool failed, finished, report_written, true_boss_seen;
static char report[16384], status[96], file_path[1100], summary[256];

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

static void set_word(uint32_t address, unsigned value)
{
	*(uint16_t *)cave_work_ram_at(address, 2) = (uint16_t)value;
}

static void note(const char *format, ...)
{
	const size_t used = strlen(report);
	va_list list;

	if (used + 1u >= sizeof(report))
		return;
	va_start(list, format);
	vsnprintf(report + used, sizeof(report) - used, format, list);
	va_end(list);
}

static const char *game_time(long frames)
{
	static char text[2][32];
	static int which;
	const long seconds = (long)(frames / FRAMES_A_SECOND);

	which ^= 1;
	snprintf(text[which], sizeof(text[which]), "%ld:%02ld:%02ld", seconds / 3600, seconds / 60 % 60, seconds % 60);
	return text[which];
}

static bool second_loop_played;    /* (area 6 seen: stage 5 is the ending from then on, the pause between the loops
                                       before, the loop already 1 there) */

/* the place in the game: "AREA 3", "between the loops", "the ending" */
static const char *place_name(int loop, int stage)
{
	static char text[32];

	if (stage >= 5 || loop >= 2)
		return second_loop_played ? "the ending" : "between the loops";
	snprintf(text, sizeof(text), "AREA %d", loop * 5 + stage + 1);
	return text;
}

bool playtest_choose(const char *which)
{
	char ship;

	if (!which || !strcmp(which, "all") || !strcmp(which, "ALL"))
	{
		first_run = 0;
		last_run = RUNS - 1;
		return true;
	}
	ship = (char)toupper((unsigned char)which[0]);
	if (ship < 'A' || ship > 'C' || which[1] != '\0')
		return false;
	first_run = last_run = ship - 'A';
	return true;
}

static void write_report(void)
{
	FILE *const file = file_path[0] ? fopen(file_path, "w") : NULL;

	if (file)
	{
		fputs(report, file);
		fclose(file);
	}
	report_written = true;
}

/* (a crash or the 68000 stopping ends the program: the report says where it was) */
static void at_exit(void)
{
	if (finished || report_written)
		return;
	note("\nTHE PROGRAM STOPPED during %s, in %s at %s of the game's time: see donpachi.log\n", run_names[run],
		place_loop >= 0 ? place_name(place_loop, place_stage) : "its start", game_time(frame - run_began));
	write_report();
	fputs(report, stderr);
}

void playtest_begin(const char *path, int version)
{
	snprintf(file_path, sizeof(file_path), "%s", path ? path : "");
	run = first_run;
	phase = WAITING;
	frame = phase_frames = select_frames = play_frames = since_progress = run_began = 0;
	report[0] = '\0';
	note("DonPachi Recomp playtest: %s version, %s\n", version ? "the Japanese" : "the US",
		first_run == last_run ? run_names[first_run] : "every ship");
	note("(player 1 kept from harm, at MAX power; the ROM set's own EEPROM; the arcade's slowdown on; nothing "
		"saved)\n\n");
	atexit(at_exit);
}

/* an object of the type in play */
static bool object_up(unsigned type)
{
	for (unsigned slot = 0; slot < 64u; slot++)
	{
		const uint32_t object = OBJECTS + 0x40u * slot;

		if ((word(object) & 0x8000u) && word(object + 0xCu) >> 8 == type)
			return true;
	}
	return false;
}

/* the stick towards the strongest enemy on the screen (training.c's towards_target); 'strongest' its hit points */
static unsigned towards_target(int *strongest)
{
	int most = 0, target = -1;

	for (int record = 0; record < 150; record++)
	{
		const uint32_t at = record < 100 ? 0x103814u + 0x20u * (uint32_t)record
			: 0x104494u + 0x20u * (uint32_t)(record - 100);
		const int hit_points = (int16_t)word(at + 0x18u), x = (int16_t)word(at + 8u), y = (int16_t)word(at + 6u);

		if ((word(at) & 0xA000u) == 0xA000u && hit_points > most && y >= 0 && y < 0x5000 && x >= 0 && x < 0x3C00)
		{
			most = hit_points;
			target = x;
		}
	}
	*strongest = most;
	if (target >= 0)
	{
		const int ship = (int16_t)word(SHIP + 8u);

		if (target > ship + 256)
			return CAVE_RIGHT;
		if (target < ship - 256)
			return CAVE_LEFT;
	}
	return 0;
}

/* the ship select, as training.c's: the ship (down presses), then the shot */
static unsigned select_inputs(void)
{
	const long at = select_frames % 600;    /* (again from the top if the select is still up 600 frames on) */

	if (at > 60 && at <= 60 + 30L * run && (at - 61) % 30 < 8)
		return CAVE_DOWN;
	if (at > 80 + 30L * run && (at / 8) % 2)
		return CAVE_BUTTON1;
	return 0;
}

/* towards the strongest enemy, the shot tapped, the laser held a while now and then, a bomb every 900 frames */
static unsigned play_inputs(void)
{
	const long f = play_frames;
	int strongest;
	unsigned held = towards_target(&strongest);

	if ((f / 130) % 5 == 4)
		held |= CAVE_BUTTON1;    /* (held: the laser) */
	else if ((f / 2) % 2)
		held |= CAVE_BUTTON1;
	if (f % 900 == 450)
		held |= CAVE_BUTTON2;
	return held;
}

unsigned playtest_inputs(void)
{
	const unsigned mode = game_mode_raw();

	if (finished)
		return 0;
	if (mode == 0xBu)
		return select_inputs();
	if (game_mode_playing() || mode == 8u)
		return phase == PLAYING ? play_inputs() : 0u;
	if (phase == WAITING)
	{
		/* the title or the attract mode: a coin, then Start */
		const long beat = phase_frames % 120;

		if (phase_frames > 120 && beat < 6)
			return CAVE_COIN;
		if (phase_frames > 120 && beat >= 60 && beat < 66)
			return CAVE_START;
	}
	return 0;
}

static int fail(const char *format, ...)
{
	char why[256];
	va_list list;

	va_start(list, format);
	vsnprintf(why, sizeof(why), format, list);
	va_end(list);
	note("  FAILED: %s (%s of the game's time into the run)\n", why, game_time(frame - run_began));
	failed = true;
	finished = true;
	write_report();
	return PLAYTEST_FAILED;
}

static void set_phase(int to)
{
	phase = to;
	phase_frames = 0;
}

int playtest_frame(void)
{
	const unsigned mode = game_mode_raw();
	const bool playing = game_mode_playing();
	const bool attract = !playing && mode != 8u && mode != 0xBu;

	if (finished)
		return failed ? PLAYTEST_FAILED : PLAYTEST_PASSED;
	frame++;
	phase_frames++;
	if ((playing || mode == 8u) && (word(SHIP) & 0x8000u))
	{
		/* kept from harm (the respawn shield held), and at MAX power (own_cheats.c's) */
		if (*cave_work_ram_byte(SHIELD) < SHIELD_ON)
			*cave_work_ram_byte(SHIELD) = SHIELD_ON;
		*cave_work_ram_byte(POWER) = 4u;
		set_word(POWER_UPS, 0u);
		set_word(AT_MAX, 1u);
		set_word(SHIP, word(SHIP) | 0x400u);
	}
	switch (phase)
	{
	case WAITING:
		if (mode == 0xBu)
		{
			set_phase(SELECTING);
			select_frames = 0;
			run_began = frame;
			note("%s (%d of %d)\n", run_names[run], run - first_run + 1, last_run - first_run + 1);
		}
		else if (phase_frames > 20L * 60L * 58L)
			return fail("no game began (the ship select never came)");
		break;
	case SELECTING:
		if (mode == 0xBu)
			select_frames++;
		else if (playing)
		{
			const unsigned ship = word(SHIP_TYPE);

			if ((int)ship != run)
				return fail("the ship select gave type %c, not %s", 'A' + (ship % 3u), run_names[run]);
			set_phase(PLAYING);
			play_frames = since_progress = seen_for = 0;
			place_loop = place_stage = seen_loop = seen_stage = -1;
			true_boss_seen = second_loop_played = false;
		}
		else if (attract && phase_frames > 600)
			return fail("the game went back to its title from the ship select");
		break;
	case PLAYING:
	{
		const int loop = (int)word(LOOP), stage = (int)word(STAGE);

		play_frames++;
		since_progress++;
		if (loop != seen_loop || stage != seen_stage)
		{
			seen_loop = loop;
			seen_stage = stage;
			seen_for = 0;
		}
		if (++seen_for >= PLACE_STEADY && (loop != place_loop || stage != place_stage))
		{
			place_loop = loop;
			place_stage = stage;
			since_progress = 0;
			note("  %-18s at %s\n", place_name(loop, stage), game_time(frame - run_began - PLACE_STEADY));
			if (second_loop_played && (stage >= 5 || loop >= 2))
				set_phase(ENDING);
			if (loop == 1 && stage < 5)
				second_loop_played = true;
		}
		if (!true_boss_seen && loop == 1 && stage == 4 && object_up(TRUE_BOSS))
		{
			true_boss_seen = true;
			note("  %-18s at %s\n", "the true last boss", game_time(frame - run_began));
		}
		if (mode == 8u)
			return fail("the ship was lost in %s (the continue's count down came)", place_name(loop, stage));
		if (attract)
		{
			if (true_boss_seen && phase_frames > 0)
			{
				/* (the game back at its title straight after the last area: the ending was seen on the way) */
				note("  %-18s at %s: PASSED\n\n", "back at the title", game_time(frame - run_began));
				goto passed;
			}
			return fail("the game ended in %s, before the ending", place_name(loop, stage));
		}
		if (since_progress > STUCK_FRAMES)
			return fail("%s went on for 30 minutes of the game's time (stuck?)", place_name(loop, stage));
		break;
	}
	case ENDING:
		play_frames++;
		if (attract)
		{
			if (!true_boss_seen)
				return fail("the ending came without area 10's true last boss");
			note("  %-18s at %s: PASSED\n\n", "back at the title", game_time(frame - run_began));
			goto passed;
		}
		if (phase_frames > ENDING_MOST)
			return fail("the ending did not come back to the title in 10 minutes of the game's time");
		break;
	}
	return PLAYTEST_GOING;

passed:
	if (run == last_run)
	{
		finished = true;
		return PLAYTEST_PASSED;
	}
	run++;
	set_phase(WAITING);
	return PLAYTEST_GOING;
}

const char *playtest_status(void)
{
	const char *where = phase == WAITING ? "starting a game" : phase == SELECTING ? "the ship select"
		: phase == ENDING ? "the ending" : place_loop >= 0 ? place_name(place_loop, place_stage) : "";

	snprintf(status, sizeof(status), "%s (%d of %d)  %s  %s", run_names[run], run - first_run + 1,
		last_run - first_run + 1, where, game_time(frame - run_began));
	return status;
}

const char *playtest_finish(double seconds, bool stopped_by_player)
{
	if (!finished && stopped_by_player)
	{
		note("  STOPPED by the player in %s\n", playtest_status());
		failed = true;
	}
	finished = true;
	snprintf(summary, sizeof(summary), "%s: %s of the game's time in %.0f seconds (%.1f times the arcade's speed)",
		failed ? "NOT PASSED" : "ALL PASSED", game_time(frame), seconds,
		seconds > 0.0 ? frame / FRAMES_A_SECOND / seconds : 0.0);
	note("%s\n", summary);
	write_report();
	return report;
}

const char *playtest_summary(void)
{
	return summary;
}
