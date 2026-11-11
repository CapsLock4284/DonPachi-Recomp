/**
 *    Name:        replay.c
 *    Description: replays: recorded, saved, listed and played back (replay.h)
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    The file (little endian):
 *      "DPREPLAY", format (16 bits: 1), the game ("donpachi", 8 bytes), the port's version (16 bytes)
 *      the date (20 bytes, "2026-11-11 15:20"), player 1's best score (32), the area reached (8), the ship (8), the
 *      difficulty (8), finished (8), the frames in all (32), the game's first frame (32: its ship select)
 *      the EEPROM's words (CAVE_EEPROM_WORDS of 16)
 *      the patches: their count (32), then each: the frame after which (32), the address (32), the size (16), the bytes
 *      the checks: their count (32), then each: the frame after which (32), cave_state_hash() then (64)
 *      the inputs: runs of the same frame's inputs, their count (32), then each: how many frames (32), in0 (16),
 *      in1 (16), the flags (8)
 */
#include "replay.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifndef DONPACHI_VERSION
#define DONPACHI_VERSION "0.0.0"
#endif

#define FORMAT          1u
#define CHECK_EVERY     600u    /* frames between checks (about 10 seconds) */
#define GAME_MODE       0x1018ECu
#define BIG_HIT_FLAGS   0x101738u

static const char magic[8] = { 'D', 'P', 'R', 'E', 'P', 'L', 'A', 'Y' };
static const char game_name[8] = { 'd', 'o', 'n', 'p', 'a', 'c', 'h', 'i' };

typedef struct
{
	uint16_t in0, in1;
	uint8_t flags;
} frame_input;

typedef struct
{
	uint32_t frame, address;
	uint16_t size;
	uint8_t bytes[128];
} patch;

typedef struct
{
	uint32_t frame;
	uint64_t hash;
} check;

/* a recording or a replay loaded: what the file holds */
typedef struct
{
	uint16_t eeprom[CAVE_EEPROM_WORDS];
	frame_input *inputs;
	uint32_t count, capacity;
	patch *patches;
	uint32_t patch_count, patch_capacity;
	check *checks;
	uint32_t check_count, check_capacity;
} track;

static void track_free(track *t)
{
	free(t->inputs);
	free(t->patches);
	free(t->checks);
	memset(t, 0, sizeof(*t));
}

static bool grow(void **items, uint32_t *capacity, uint32_t count, size_t size)
{
	void *bigger;
	uint32_t more;

	if (count < *capacity)
		return true;
	more = *capacity ? *capacity * 2u : 1024u;
	if (!(bigger = realloc(*items, size * more)))
		return false;
	*items = bigger;
	*capacity = more;
	return true;
}

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

static uint32_t bcd_long(uint32_t address)
{
	const uint32_t bcd = (uint32_t)word(address) << 16 | word(address + 2u);
	uint32_t value = 0;

	for (int digit = 7; digit >= 0; digit--)
		value = value * 10u + (bcd >> (digit * 4) & 15u);
	return value;
}

static char folder[1024];

void replay_folder(const char *path)
{
	snprintf(folder, sizeof(folder), "%s", path);
}

/* ---- recording ---- */

static track rec;
static bool tainted;

/* the game being recorded */
static struct
{
	bool on;                    /* from its ship select */
	bool played;                /* its play began */
	uint32_t begin;             /* its first frame */
	uint32_t score;
	int area, ship, difficulty;
	unsigned last_mode;
} game;

void replay_reset(void)
{
	track_free(&rec);
	memcpy(rec.eeprom, cave_board.eeprom.words, sizeof(rec.eeprom));
	tainted = false;
	memset(&game, 0, sizeof(game));
}

void replay_taint(void)
{
	tainted = true;
	game.on = false;
}

void replay_input(uint16_t in0, uint16_t in1, unsigned flags)
{
	if (tainted)
		return;
	if (!grow((void **)&rec.inputs, &rec.capacity, rec.count, sizeof(frame_input)))
	{
		replay_taint();    /* (no memory: no replay) */
		return;
	}
	rec.inputs[rec.count].in0 = in0;
	rec.inputs[rec.count].in1 = in1;
	rec.inputs[rec.count].flags = (uint8_t)flags;
	rec.count++;
}

void replay_patch(uint32_t address, unsigned size)
{
	patch *p;

	if (tainted || size > sizeof(p->bytes))
		return;
	if (!grow((void **)&rec.patches, &rec.patch_capacity, rec.patch_count, sizeof(patch)))
	{
		replay_taint();
		return;
	}
	p = &rec.patches[rec.patch_count++];
	p->frame = rec.count;
	p->address = address;
	p->size = (uint16_t)size;
	for (unsigned at = 0; at < size; at++)
		p->bytes[at] = *cave_work_ram_byte(address + at);
}

static void put(FILE *file, uint64_t value, int bytes)
{
	for (int at = 0; at < bytes; at++)
		fputc((int)(value >> (8 * at) & 0xFFu), file);
}

static void make_folder(void)
{
#ifdef _WIN32
	_mkdir(folder);
#else
	mkdir(folder, 0755);
#endif
}

/* the recording, to the end of the game, saved: the file's name, or NULL */
static const char *save(bool finished)
{
	static char name[64];
	char path[1400], date[20];
	time_t now = time(NULL);
	const struct tm *const local = localtime(&now);
	uint32_t runs = 0;
	FILE *file;

	if (!local)
		return NULL;
	strftime(name, sizeof(name), "%Y-%m-%d_%H%M%S.dpreplay", local);
	strftime(date, sizeof(date), "%Y-%m-%d %H:%M", local);
	make_folder();
	snprintf(path, sizeof(path), "%s%s", folder, name);
	if (!(file = fopen(path, "wb")))
		return NULL;
	fwrite(magic, 1, sizeof(magic), file);
	put(file, FORMAT, 2);
	fwrite(game_name, 1, sizeof(game_name), file);
	{
		char version[16] = { 0 };

		strncpy(version, DONPACHI_VERSION, sizeof(version) - 1u);
		fwrite(version, 1, sizeof(version), file);
	}
	{
		char padded[20] = { 0 };

		memcpy(padded, date, strlen(date));
		fwrite(padded, 1, sizeof(padded), file);
	}
	put(file, game.score, 4);
	put(file, (uint64_t)game.area, 1);
	put(file, (uint64_t)game.ship, 1);
	put(file, (uint64_t)game.difficulty, 1);
	put(file, finished ? 1u : 0u, 1);
	put(file, rec.count, 4);
	put(file, game.begin, 4);
	for (unsigned at = 0; at < CAVE_EEPROM_WORDS; at++)
		put(file, rec.eeprom[at], 2);
	put(file, rec.patch_count, 4);
	for (uint32_t at = 0; at < rec.patch_count; at++)
	{
		put(file, rec.patches[at].frame, 4);
		put(file, rec.patches[at].address, 4);
		put(file, rec.patches[at].size, 2);
		fwrite(rec.patches[at].bytes, 1, rec.patches[at].size, file);
	}
	put(file, rec.check_count, 4);
	for (uint32_t at = 0; at < rec.check_count; at++)
	{
		put(file, rec.checks[at].frame, 4);
		put(file, rec.checks[at].hash, 8);
	}
	for (uint32_t at = 0; at < rec.count; at++)
		if (at == 0 || memcmp(&rec.inputs[at], &rec.inputs[at - 1u], sizeof(frame_input)))
			runs++;
	put(file, runs, 4);
	for (uint32_t at = 0; at < rec.count;)
	{
		uint32_t length = 1;

		while (at + length < rec.count && !memcmp(&rec.inputs[at + length], &rec.inputs[at], sizeof(frame_input)))
			length++;
		put(file, length, 4);
		put(file, rec.inputs[at].in0, 2);
		put(file, rec.inputs[at].in1, 2);
		put(file, rec.inputs[at].flags, 1);
		at += length;
	}
	if (fclose(file) != 0)
		return NULL;
	return name;
}

static void add_check(track *t)
{
	if (!grow((void **)&t->checks, &t->check_capacity, t->check_count, sizeof(check)))
		return;
	t->checks[t->check_count].frame = t->count;
	t->checks[t->check_count].hash = cave_state_hash();
	t->check_count++;
}

const char *replay_save_unfinished(void)
{
	const char *name = NULL;

	if (!tainted && game.on && game.played)
		name = save(false);
	game.on = false;
	return name;
}

const char *replay_after_frame(void)
{
	const unsigned mode = word(GAME_MODE);
	const char *saved = NULL;

	if (tainted)
		return NULL;
	if (rec.count % CHECK_EVERY == 0u && rec.count)
		add_check(&rec);
	if (mode == 0xBu && game.last_mode != 0xBu)
	{
		/* a ship select: a game begins (one being recorded ends: a new one without the attract mode between) */
		if (game.on && game.played)
			saved = save(true);
		memset(&game, 0, sizeof(game));
		game.on = true;
		game.begin = rec.count ? rec.count - 1u : 0u;
	}
	else if (game.on && game.played && (mode == 0u || mode == 1u || mode == 5u))
	{
		saved = save(true);    /* (the attract mode again: the game over) */
		game.on = false;
	}
	if (game.on && mode == 7u)
	{
		const unsigned stage = word(0x101A40u), loop = word(0x101A42u);
		const uint32_t score = bcd_long(0x10177Au);

		if (!game.played)
		{
			game.played = true;
			game.ship = (int)(word(0x102158u + 0x1Cu) % 3u);
			game.difficulty = (int)(word(0x101284u) & 3u);
		}
		if (stage < 5u && loop < 2u && (int)(loop * 5u + stage + 1u) > game.area)
			game.area = (int)(loop * 5u + stage + 1u);
		if (score > game.score)
			game.score = score;
	}
	game.last_mode = mode;
	return saved;
}

/* ---- reading a file ---- */

static uint64_t get(FILE *file, int bytes, bool *ok)
{
	uint64_t value = 0;

	for (int at = 0; at < bytes; at++)
	{
		const int c = fgetc(file);

		if (c == EOF)
			*ok = false;
		value |= (uint64_t)(c & 0xFF) << (8 * at);
	}
	return value;
}

/* the header: false if it is not a replay of this game */
static bool read_header(FILE *file, replay_info *info, uint32_t *begin)
{
	char head[8], name[8], version[16], date[20];
	bool ok = true;

	if (fread(head, 1, 8, file) != 8 || memcmp(head, magic, 8) || get(file, 2, &ok) != FORMAT
			|| fread(name, 1, 8, file) != 8 || memcmp(name, game_name, 8)
			|| fread(version, 1, 16, file) != 16 || fread(date, 1, 20, file) != 20)
		return false;
	date[19] = '\0';
	snprintf(info->date, sizeof(info->date), "%s", date);
	info->score = (uint32_t)get(file, 4, &ok);
	info->area = (int)get(file, 1, &ok);
	info->ship = (int)get(file, 1, &ok);
	info->difficulty = (int)get(file, 1, &ok);
	info->finished = get(file, 1, &ok) != 0;
	{
		const uint32_t all = (uint32_t)get(file, 4, &ok);

		*begin = (uint32_t)get(file, 4, &ok);
		info->frames = all > *begin ? all - *begin : 0u;
	}
	return ok;
}

static int newest_first(const void *a, const void *b)
{
	return strcmp(((const replay_info *)b)->file, ((const replay_info *)a)->file);
}

static void list_one(const char *file_name, replay_info *out, int *count, int most)
{
	char path[1400];
	FILE *file;
	uint32_t begin;
	const size_t length = strlen(file_name);

	if (*count >= most || length < 9 || strcmp(file_name + length - 9, ".dpreplay"))
		return;
	snprintf(path, sizeof(path), "%s%s", folder, file_name);
	if (!(file = fopen(path, "rb")))
		return;
	memset(&out[*count], 0, sizeof(out[*count]));
	if (read_header(file, &out[*count], &begin))
	{
		snprintf(out[*count].file, sizeof(out[*count].file), "%s", file_name);
		(*count)++;
	}
	fclose(file);
}

int replay_list(replay_info *out, int most)
{
	int count = 0;
#ifdef _WIN32
	char pattern[1400];
	WIN32_FIND_DATAA found;
	HANDLE search;

	snprintf(pattern, sizeof(pattern), "%s*.dpreplay", folder);
	if ((search = FindFirstFileA(pattern, &found)) != INVALID_HANDLE_VALUE)
	{
		do
			list_one(found.cFileName, out, &count, most);
		while (FindNextFileA(search, &found));
		FindClose(search);
	}
#else
	DIR *const dir = opendir(folder[0] ? folder : ".");
	struct dirent *entry;

	if (dir)
	{
		while ((entry = readdir(dir)) != NULL)
			list_one(entry->d_name, out, &count, most);
		closedir(dir);
	}
#endif
	qsort(out, (size_t)count, sizeof(out[0]), newest_first);
	return count;
}

/* ---- playback ---- */

static track play;
static bool playing;
static uint32_t position, next_patch, next_check;
static uint16_t own_eeprom[CAVE_EEPROM_WORDS];
static unsigned frame_flags;
static bool big_hit_set, out_of_step;

static const char *load(const char *file_name, uint32_t *begin)
{
	char path[1400];
	FILE *file;
	replay_info info;
	bool ok = true;
	uint32_t runs;

	snprintf(path, sizeof(path), "%s%s", folder, file_name);
	if (!(file = fopen(path, "rb")))
		return "it could not be opened";
	track_free(&play);
	if (!read_header(file, &info, begin))
	{
		fclose(file);
		return "it is not a DonPachi replay of this version";
	}
	for (unsigned at = 0; at < CAVE_EEPROM_WORDS; at++)
		play.eeprom[at] = (uint16_t)get(file, 2, &ok);
	play.patch_count = (uint32_t)get(file, 4, &ok);
	if (ok && play.patch_count && !(play.patches = calloc(play.patch_count, sizeof(patch))))
		ok = false;
	for (uint32_t at = 0; ok && at < play.patch_count; at++)
	{
		patch *const p = &play.patches[at];

		p->frame = (uint32_t)get(file, 4, &ok);
		p->address = (uint32_t)get(file, 4, &ok);
		p->size = (uint16_t)get(file, 2, &ok);
		if (p->size > sizeof(p->bytes) || fread(p->bytes, 1, p->size, file) != p->size)
			ok = false;
	}
	play.check_count = ok ? (uint32_t)get(file, 4, &ok) : 0u;
	if (ok && play.check_count && !(play.checks = calloc(play.check_count, sizeof(check))))
		ok = false;
	for (uint32_t at = 0; ok && at < play.check_count; at++)
	{
		play.checks[at].frame = (uint32_t)get(file, 4, &ok);
		play.checks[at].hash = get(file, 8, &ok);
	}
	runs = ok ? (uint32_t)get(file, 4, &ok) : 0u;
	for (uint32_t run = 0; ok && run < runs; run++)
	{
		const uint32_t length = (uint32_t)get(file, 4, &ok);
		frame_input input;

		input.in0 = (uint16_t)get(file, 2, &ok);
		input.in1 = (uint16_t)get(file, 2, &ok);
		input.flags = (uint8_t)get(file, 1, &ok);
		for (uint32_t at = 0; ok && at < length; at++)
		{
			if (!grow((void **)&play.inputs, &play.capacity, play.count, sizeof(frame_input)))
				ok = false;
			else
				play.inputs[play.count++] = input;
		}
	}
	fclose(file);
	if (!ok)
	{
		track_free(&play);
		return "it is damaged";
	}
	return NULL;
}

bool replay_next(unsigned *flags)
{
	if (!playing || position >= play.count)
		return false;
	cave_board.in0 = play.inputs[position].in0;
	cave_board.in1 = play.inputs[position].in1;
	frame_flags = play.inputs[position].flags;
	if (flags)
		*flags = frame_flags;
	position++;
	return true;
}

bool replay_after_playback_frame(void)
{
	uint8_t *const big_hit = cave_work_ram_byte(BIG_HIT_FLAGS);

	/* the big HIT display, as the port's option did it as it was recorded */
	if ((frame_flags & REPLAY_BIG_HIT) && word(GAME_MODE) == 7u)
	{
		*big_hit |= 3u;
		big_hit_set = true;
	}
	else if (!(frame_flags & REPLAY_BIG_HIT) && big_hit_set)
	{
		*big_hit &= (uint8_t)~3u;
		big_hit_set = false;
	}
	while (next_patch < play.patch_count && play.patches[next_patch].frame <= position)
	{
		const patch *const p = &play.patches[next_patch++];

		for (unsigned at = 0; at < p->size; at++)
			*cave_work_ram_byte(p->address + at) = p->bytes[at];
	}
	while (next_check < play.check_count && play.checks[next_check].frame <= position)
	{
		if (play.checks[next_check++].hash != cave_state_hash() && !out_of_step)
		{
			out_of_step = true;
			return false;
		}
	}
	return true;
}

const char *replay_play(const char *file_name)
{
	uint32_t begin = 0;
	const char *const problem = load(file_name, &begin);

	if (problem)
		return problem;
	memcpy(own_eeprom, cave_board.eeprom.words, sizeof(own_eeprom));
	memcpy(cave_board.eeprom.words, play.eeprom, sizeof(own_eeprom));
	program_reset();
	playing = true;
	position = next_patch = next_check = 0;
	big_hit_set = out_of_step = false;
	tainted = true;    /* (no recording of a replay) */
	/* the frames before the game, unseen */
	while (position < begin && replay_next(NULL))
	{
		program_run_frame();
		cave_sound_drop();
		replay_after_playback_frame();
	}
	return NULL;
}

bool replay_playing(void)
{
	return playing;
}

void replay_stop(void)
{
	if (!playing)
		return;
	playing = false;
	memcpy(cave_board.eeprom.words, own_eeprom, sizeof(own_eeprom));
	track_free(&play);
}
