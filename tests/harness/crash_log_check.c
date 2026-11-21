/* crash_log_check [FRAMES]: the crash log's run (ports/sdl2/crash_log.h) played again the same, for each way the
   program can end badly: the simple player's game run FRAMES frames (default 6000) as the port runs it (the inputs
   kept, the high score table put back from a file after the boot, the big HIT display on for a stretch, a reset part
   way in the second run: the first is from the program's start), then the program stopped as the runtime stops it when the 68000 does (exit(3)), or crashed (SIGSEGV). Each
   time the inputs' file is played by replay_crash (built beside this) and its end compared with the run's: the work
   RAM's sum the same. Then a run with a save state put back: the file says it is not replayable. PASS or FAIL each.
   Runs in a folder of its own: it writes donpachi_crash_*.inp there */
#include "common.h"

#include "cave/sound.h"
#include "cave/video.h"

#include "../../ports/sdl2/crash_log.c"
#include "../../ports/sdl2/hiscore.c"

#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define BIG_HIT_FLAGS 0x101738u

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-60s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static uint32_t ram_sum(void)
{
	uint32_t sum = 0;

	for (uint32_t address = 0x100000u; address < 0x110000u; address++)
		sum = sum * 31u + ram_byte(address);
	return sum;
}

/* The run, in a child: its sum written to 'out', then the end 'how' (0 the 68000 stopped, 1 a crash) */
static void run_game(long frames, int how, bool state, int out)
{
	static unsigned char table[HISCORE_TABLE_SIZE];
	bool big_hit_set = false;
	char line[32];
	program_state *saved = NULL;

	for (unsigned at = 0; at < 20u; at += 4u)
	{
		table[at] = 0x01;    /* (the scores 1,0xx,xxx BCD: over the defaults) */
		table[at + 1] = (unsigned char)(0x99 - at);
	}
	for (unsigned at = 20u; at < HISCORE_TABLE_SIZE; at++)
		table[at] = (unsigned char)('A' + at % 26u);
	crash_log_start("", "./");
	program_set_slowdown(true);
	crash_log_reset(true, (int)rom_set.version);
	hiscore_use_table(table);
	hiscore_reset();
	for (long frame = 0; frame < frames; frame++)
	{
		const bool big_hit = frame > frames / 4 && frame < frames / 2;
		uint8_t *flags;

		if (how == 1 && frame == frames / 3)
		{
			program_reset();    /* (the RESET hotkey part way: the run from here) */
			crash_log_reset(true, (int)rom_set.version);
			hiscore_reset();
			big_hit_set = false;
		}
		if (state && frame == frames / 2)
			saved = program_state_save();
		if (state && frame == frames / 2 + 300)
		{
			program_state_load(saved);
			crash_log_not_replayable("a save state was put back");
		}
		cave_board.in0 = (uint16_t)~autoplayer(frame, 0);
		cave_board.in1 = 0xFFFFu;
		crash_log_frame(big_hit ? CRASH_LOG_BIG_HIT : 0u);
		program_run_frame();
		flags = cave_work_ram_byte(BIG_HIT_FLAGS);
		if (big_hit && ram_word(0x1018ECu) == 7u)
		{
			*flags |= 3u;
			big_hit_set = true;
		}
		else if (!big_hit && big_hit_set)
		{
			*flags &= (uint8_t)~3u;
			big_hit_set = false;
		}
		cave_sound_drop();
		video_latch_sprites();
		hiscore_frame(true, false);
		if (hiscore_take_put_back())
			crash_log_table(hiscore_put_back_table());
	}
	snprintf(line, sizeof(line), "%08X\n", (unsigned)ram_sum());
	if (write(out, line, strlen(line)) < 0)
		_exit(9);
	close(out);
	if (how == 0)
		exit(3);    /* (as the runtime ends the program when the 68000 stops) */
	raise(SIGSEGV);
	_exit(8);
}

/* The newest donpachi_crash_*.inp here: its name, or "" */
static void newest_inp(char *name, size_t size)
{
	DIR *const dir = opendir(".");
	struct dirent *entry;

	name[0] = '\0';
	while (dir && (entry = readdir(dir)) != NULL)
		if (!strncmp(entry->d_name, "donpachi_crash_", 15) && strcmp(entry->d_name, name) > 0)
			snprintf(name, size, "%s", entry->d_name);
	if (dir)
		closedir(dir);
}

static void clear_inps(void)
{
	char name[256];

	for (newest_inp(name, sizeof(name)); name[0]; newest_inp(name, sizeof(name)))
		remove(name);
}

/* One end: the run in a child, its file played by replay_crash; the sums compared. The log (the child's stderr) in
   'log' */
static void check(const char *tool, long frames, int how, bool state, const char *what)
{
	int pipe_ends[2];
	char sum[16] = "", name[256], command[1400], line[512], played[16] = "";
	pid_t child;
	FILE *output;
	bool marked = false;

	clear_inps();
	if (pipe(pipe_ends))
		exit(1);
	fflush(stdout);
	if ((child = fork()) == 0)
	{
		close(pipe_ends[0]);
		if (!freopen("crash_check.log", "w", stderr))
			_exit(7);
		run_game(frames, how, state, pipe_ends[1]);
	}
	close(pipe_ends[1]);
	if (read(pipe_ends[0], sum, 8) != 8)
		sum[0] = '\0';
	sum[8] = '\0';
	close(pipe_ends[0]);
	waitpid(child, NULL, 0);
	newest_inp(name, sizeof(name));
	if ((output = fopen("crash_check.log", "r")) != NULL)
	{
		while (fgets(line, sizeof(line), output))
		{
			fputs("    log: ", stdout);
			fputs(line, stdout);
			if (strstr(line, "not replayable"))
				marked = true;
		}
		fclose(output);
	}
	if (!name[0])
	{
		verdict(what, false);
		return;
	}
	if (state)
	{
		verdict(what, marked);
		return;
	}
	snprintf(command, sizeof(command), "%s %s", tool, name);
	if ((output = popen(command, "r")) != NULL)
	{
		while (fgets(line, sizeof(line), output))
		{
			const char *const at = strstr(line, "work RAM sum ");

			fputs("    replay: ", stdout);
			fputs(line, stdout);
			if (at)
				snprintf(played, sizeof(played), "%.8s", at + 13);
		}
		pclose(output);
	}
	printf("    the run's sum %s, played again %s\n", sum, played);
	verdict(what, sum[0] && !strcmp(sum, played) && !marked);
}

int main(int argc, char **argv)
{
	const long frames = argc > 1 ? atol(argv[1]) : 6000;
	char tool[1100];

	snprintf(tool, sizeof(tool), "%.*sreplay_crash", (int)(strrchr(argv[0], '/') ? strrchr(argv[0], '/') - argv[0] + 1
		: 0), argv[0]);
	start_game();
	check(tool, frames, 0, false, "the 68000 stopped (exit 3): played again the same");
	check(tool, frames, 1, false, "a crash (SIGSEGV): played again the same");
	check(tool, frames, 0, true, "a save state put back: marked not replayable");
	clear_inps();
	remove("crash_check.log");
	return failures ? 1 : 0;
}
