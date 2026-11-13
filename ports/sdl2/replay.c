/**
 *    Name:        replay.c
 *    Description: replays: recorded, saved, listed, renamed, deleted and played back (replay.h)
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    The file (little endian):
 *      "DPREPLAY", format (16 bits: 3; 1 and 2 are read too), the game ("donpachi", 8 bytes), the port's version (16 bytes)
 *      the date (20 bytes, "2026-11-11 15:20"), player 1's best score (32), the area reached (8), the ship (8), the
 *      difficulty (8), its end (8: 0 stopped part way, 1 the game over, 2 all clear: area 10 cleared), the frames in
 *      all (32), the game's first frame (32: its ship select)
 *      format 2: the start (8: 0 the board's reset, 1 a training place), the training setup (6 bytes: the area,
 *      the place (100 + the percentage: a point part way), the ship, the power, the bombs (1: 7), the difficulty)
 *      format 3: the slowdown (8: 1 on; cave/program.h's program_set_slowdown: the frames fall otherwise). A replay
 *      of format 1 or 2 was made without it
 *      the EEPROM's words (CAVE_EEPROM_WORDS of 16)
 *      the patches: their count (32), then each: the frame after which (32; 0: before the first), the address (32),
 *      the size (16), the bytes
 *      the checks: their count (32), then each: the frame after which (32), cave_state_hash() then (64)
 *      the inputs: runs of the same frame's inputs, their count (32), then each: how many frames (32), in0 (16),
 *      in1 (16), the flags (8)
 *    The file's name: the date and time it was saved ("2026-11-11_152529"), then the player's name for it if it has
 *    one ("_MY BEST RUN"), then ".dpreplay"
 */
#include "replay.h"
#include "game_mode.h"

#include "training.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/sound.h"

#include <ctype.h>
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

#define FORMAT          3u
#define CHECK_EVERY     600u    /* frames between checks (about 10 seconds) */
#define GAME_MODE       0x1018ECu
#define BIG_HIT_FLAGS   0x101738u
#define DATE_LENGTH     17u     /* "2026-11-11_152529": the file name's start */
#define SHORTEST_KEPT   (57u * 3u)    /* a training attempt shorter than this (3 seconds): the one before is kept */

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

/* where a replay starts: the board's reset, or a training place */
typedef struct
{
	bool training;
	uint8_t setup[6];           /* the area, the place, the ship, the power, the bombs (1: 7), the difficulty */
	bool slowdown;              /* the arcade's slowdown on (cave/program.h) */
} start_info;

/* the game in a recording */
typedef struct
{
	bool on;                    /* from its ship select (a training attempt: from its start) */
	bool played;                /* its play began */
	uint32_t begin;             /* its first frame */
	uint32_t score;
	int area, ship, difficulty;
	unsigned last_mode;
	int last_area;              /* (area 10 cleared: the stage moving on from it during play) */
	bool all_clear;
} game_record;

/* a recording, or a replay loaded: what the file holds */
typedef struct
{
	uint16_t eeprom[CAVE_EEPROM_WORDS];
	start_info start;
	game_record game;
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

static void setup_to_bytes(const training_setup *setup, uint8_t *bytes)
{
	bytes[0] = (uint8_t)setup->area;
	bytes[1] = (uint8_t)(setup->point == TRAINING_PART_WAY ? 100 + setup->percent : setup->point);
	bytes[2] = (uint8_t)setup->ship;
	bytes[3] = (uint8_t)setup->power;
	bytes[4] = setup->max_bombs ? 1u : 0u;
	bytes[5] = (uint8_t)setup->difficulty;
}

static training_setup setup_from_bytes(const uint8_t *bytes)
{
	training_setup setup;

	setup.area = bytes[0] % 10u;
	setup.point = bytes[1] >= 100u ? TRAINING_PART_WAY : bytes[1] % TRAINING_PART_WAY;
	setup.percent = bytes[1] >= 100u ? bytes[1] - 100 : 0;
	setup.ship = bytes[2] % 3u;
	setup.power = bytes[3] % TRAINING_POWERS;
	setup.max_bombs = bytes[4] != 0;
	setup.difficulty = bytes[5] & 3u;
	return setup;
}

/* ---- recording ---- */

static track rec;               /* since the reset, or the training attempt's start */
static track previous;          /* training: the attempt before this one */
static bool tainted;

/* a game ended, its replay waiting for the player's yes or no (replay_keep) */
static struct
{
	bool on;
	game_record game;
	uint32_t end;               /* its last frame (the recording's frames then) */
	bool finished;
} pending;

void replay_reset(void)
{
	track_free(&rec);
	track_free(&previous);
	memcpy(rec.eeprom, cave_board.eeprom.words, sizeof(rec.eeprom));
	rec.start.training = false;
	rec.start.slowdown = program_slowdown();
	tainted = false;
	pending.on = false;
}

void replay_taint(void)
{
	tainted = true;
	rec.game.on = false;
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

/* ---- training ---- */

/* a training attempt begins at the place, as it was made: the setup, the player's EEPROM, and the port's writes
   after the training's run there (the high score table and the HUD's HIGH and scores drawn again) */
static void training_begin(const start_info *start, const uint16_t *eeprom)
{
	track_free(&rec);
	memcpy(rec.eeprom, eeprom, sizeof(rec.eeprom));
	rec.start = *start;
	tainted = false;
	pending.on = false;
	rec.game.on = rec.game.played = true;
	rec.game.area = start->setup[0] + 1;
	rec.game.ship = start->setup[2];
	rec.game.difficulty = start->setup[5];
	rec.game.last_mode = 7u;
	replay_patch(0x101846u, 0x5Au);    /* (the table) */
	replay_patch(0x101782u, 0x44u);    /* (the HUD's HIGH, player 1's score's digits) */
	replay_patch(0x1017C6u, 0x40u);    /* (the HIGH's digits) */
}

void replay_begin_training(const training_setup *setup)
{
	start_info start = { true, { 0 }, false };

	setup_to_bytes(setup, start.setup);
	start.slowdown = program_slowdown();
	track_free(&previous);
	training_begin(&start, cave_board.eeprom.words);
}

void replay_training_retry(void)
{
	start_info start;
	uint16_t eeprom[CAVE_EEPROM_WORDS];

	if (!rec.start.training)
		return;
	start = rec.start;
	memcpy(eeprom, rec.eeprom, sizeof(eeprom));
	if (!tainted && rec.count >= SHORTEST_KEPT)
	{
		/* (this attempt becomes the one before) */
		track_free(&previous);
		previous = rec;
		memset(&rec, 0, sizeof(rec));
	}
	training_begin(&start, eeprom);
}

/* ---- saving ---- */

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

/* 't' to its 'end' frames saved, as its game 'g': the file's name, or NULL */
static const char *save(const track *t, const game_record *g, uint32_t end, bool finished)
{
	static char name[64];
	char path[1400], date[20];
	time_t now = time(NULL);
	const struct tm *const local = localtime(&now);
	uint32_t runs = 0, patches = 0, checks = 0;
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
		char version[16] = { 0 }, padded[20] = { 0 };

		strncpy(version, DONPACHI_VERSION, sizeof(version) - 1u);
		fwrite(version, 1, sizeof(version), file);
		memcpy(padded, date, strlen(date));
		fwrite(padded, 1, sizeof(padded), file);
	}
	put(file, g->score, 4);
	put(file, (uint64_t)g->area, 1);
	put(file, (uint64_t)g->ship, 1);
	put(file, (uint64_t)g->difficulty, 1);
	put(file, !finished ? 0u : g->all_clear ? 2u : 1u, 1);
	put(file, end, 4);
	put(file, g->begin, 4);
	put(file, t->start.training ? 1u : 0u, 1);
	fwrite(t->start.setup, 1, sizeof(t->start.setup), file);
	put(file, t->start.slowdown ? 1u : 0u, 1);
	for (unsigned at = 0; at < CAVE_EEPROM_WORDS; at++)
		put(file, t->eeprom[at], 2);
	while (patches < t->patch_count && t->patches[patches].frame <= end)
		patches++;
	while (checks < t->check_count && t->checks[checks].frame <= end)
		checks++;
	put(file, patches, 4);
	for (uint32_t at = 0; at < patches; at++)
	{
		put(file, t->patches[at].frame, 4);
		put(file, t->patches[at].address, 4);
		put(file, t->patches[at].size, 2);
		fwrite(t->patches[at].bytes, 1, t->patches[at].size, file);
	}
	put(file, checks, 4);
	for (uint32_t at = 0; at < checks; at++)
	{
		put(file, t->checks[at].frame, 4);
		put(file, t->checks[at].hash, 8);
	}
	for (uint32_t at = 0; at < end; at++)
		if (at == 0 || memcmp(&t->inputs[at], &t->inputs[at - 1u], sizeof(frame_input)))
			runs++;
	put(file, runs, 4);
	for (uint32_t at = 0; at < end;)
	{
		uint32_t length = 1;

		while (at + length < end && !memcmp(&t->inputs[at + length], &t->inputs[at], sizeof(frame_input)))
			length++;
		put(file, length, 4);
		put(file, t->inputs[at].in0, 2);
		put(file, t->inputs[at].in1, 2);
		put(file, t->inputs[at].flags, 1);
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

/* the game as it is now, waiting for replay_keep */
static void hold(bool finished)
{
	pending.on = true;
	pending.game = rec.game;
	pending.end = rec.count;
	pending.finished = finished;
	rec.game.on = false;
}

bool replay_in_game(void)
{
	return !tainted && !rec.start.training && rec.game.on && rec.game.played;
}

void replay_hold_unfinished(void)
{
	if (replay_in_game())
		hold(false);
}

void replay_hold_training(void)
{
	if (!tainted && rec.start.training && rec.game.on)
		hold(true);
}

bool replay_waiting(void)
{
	return pending.on;
}

const char *replay_keep(bool keep)
{
	const char *name = NULL;

	if (pending.on && keep)
		name = save(&rec, &pending.game, pending.end, pending.finished);
	pending.on = false;
	return name;
}

const char *replay_save_training(void)
{
	if (!rec.start.training)
		return NULL;
	if (!tainted && rec.count >= SHORTEST_KEPT)
		return save(&rec, &rec.game, rec.count, false);
	if (previous.count)
		return save(&previous, &previous.game, previous.count, false);
	return NULL;
}

bool replay_after_frame(void)
{
	const unsigned mode = game_mode_now();    /* (7: a game played, by one player or both: a second player joining is not
	                                         the attract mode's 0) */
	game_record *const game = &rec.game;
	bool ended = false;

	if (tainted)
		return false;
	if (rec.count % CHECK_EVERY == 0u && rec.count)
		add_check(&rec);
	if (!rec.start.training)
	{
		if (mode == 0xBu && game->last_mode != 0xBu)
		{
			/* a ship select: a game begins (one being recorded ends: a new one without the attract mode between) */
			if (game->on && game->played)
			{
				hold(true);
				ended = true;
			}
			memset(game, 0, sizeof(*game));
			game->on = true;
			game->begin = rec.count ? rec.count - 1u : 0u;
		}
		else if (game->on && game->played && (mode == 0u || mode == 1u || mode == 5u))
		{
			hold(true);    /* (the attract mode again: the game over) */
			ended = true;
		}
	}
	if (game->on && mode == 7u)
	{
		const unsigned stage = word(0x101A40u), loop = word(0x101A42u);
		const uint32_t score = bcd_long(0x10177Au);

		if (!game->played)
		{
			game->played = true;
			game->ship = (int)(word(0x102158u + 0x1Cu) % 3u);
			game->difficulty = (int)(word(0x101284u) & 3u);
		}
		if (stage < 5u && loop < 2u && (int)(loop * 5u + stage + 1u) > game->area)
			game->area = (int)(loop * 5u + stage + 1u);
		if (score > game->score)
			game->score = score;
	}
	{
		const unsigned stage = word(0x101A40u), loop = word(0x101A42u);
		const int area = stage < 5u && loop < 2u ? (int)(loop * 5u + stage) : -1;

		if (game->on && game->last_area == 9 && area == -1 && game->last_mode == 7u)
			game->all_clear = true;    /* (area 10 cleared, as the achievements and statistics see it) */
		game->last_area = area;
	}
	game->last_mode = mode;
	return ended;
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
static bool read_header(FILE *file, replay_info *info, uint32_t *begin, start_info *start)
{
	char head[8], name[8], version[16], date[20];
	bool ok = true;
	unsigned format;

	if (fread(head, 1, 8, file) != 8 || memcmp(head, magic, 8))
		return false;
	format = (unsigned)get(file, 2, &ok);
	if (format < 1u || format > FORMAT || fread(name, 1, 8, file) != 8 || memcmp(name, game_name, 8)
			|| fread(version, 1, 16, file) != 16 || fread(date, 1, 20, file) != 20)
		return false;
	date[19] = '\0';
	snprintf(info->date, sizeof(info->date), "%s", date);
	info->score = (uint32_t)get(file, 4, &ok);
	info->area = (int)get(file, 1, &ok);
	info->ship = (int)get(file, 1, &ok);
	info->difficulty = (int)get(file, 1, &ok);
	{
		const unsigned end = (unsigned)get(file, 1, &ok);

		info->finished = end != 0u;
		info->all_clear = end == 2u;
	}
	{
		const uint32_t all = (uint32_t)get(file, 4, &ok);

		*begin = (uint32_t)get(file, 4, &ok);
		info->frames = all > *begin ? all - *begin : 0u;
	}
	memset(start, 0, sizeof(*start));
	if (format >= 2u)
	{
		start->training = get(file, 1, &ok) != 0;
		if (fread(start->setup, 1, sizeof(start->setup), file) != sizeof(start->setup))
			ok = false;
	}
	if (format >= 3u)
		start->slowdown = get(file, 1, &ok) != 0;
	info->training = start->training;
	if (start->training)
	{
		const training_setup setup = setup_from_bytes(start->setup);

		snprintf(info->place, sizeof(info->place), "%s", training_name(&setup));
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
	start_info start;
	const size_t length = strlen(file_name);

	if (*count >= most || length < 9 || strcmp(file_name + length - 9, ".dpreplay"))
		return;
	snprintf(path, sizeof(path), "%s%s", folder, file_name);
	if (!(file = fopen(path, "rb")))
		return;
	memset(&out[*count], 0, sizeof(out[*count]));
	if (read_header(file, &out[*count], &begin, &start))
	{
		replay_info *const info = &out[*count];

		snprintf(info->file, sizeof(info->file), "%s", file_name);
		/* (the player's name for it: after the date, "_", before the extension) */
		if (length > DATE_LENGTH + 1u + 9u && file_name[DATE_LENGTH] == '_')
			snprintf(info->name, sizeof(info->name), "%.*s", (int)(length - DATE_LENGTH - 1u - 9u),
				file_name + DATE_LENGTH + 1u);
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

/* ---- renaming and deleting ---- */

bool replay_rename(const char *file, const char *name)
{
	char from[1400], to[1400], clean[REPLAY_NAME_MOST + 1];
	size_t length = 0;

	if (strlen(file) < DATE_LENGTH)
		return false;
	/* (letters, digits, spaces and dashes: safe in a file's name anywhere) */
	for (const char *c = name; *c && length < REPLAY_NAME_MOST; c++)
		if (isalnum((unsigned char)*c) || *c == ' ' || *c == '-')
			clean[length++] = (char)toupper((unsigned char)*c);
	while (length && clean[length - 1u] == ' ')
		length--;
	clean[length] = '\0';
	snprintf(from, sizeof(from), "%s%s", folder, file);
	if (length)
		snprintf(to, sizeof(to), "%s%.*s_%s.dpreplay", folder, (int)DATE_LENGTH, file, clean);
	else
		snprintf(to, sizeof(to), "%s%.*s.dpreplay", folder, (int)DATE_LENGTH, file);
	if (!strcmp(from, to))
		return true;
	return rename(from, to) == 0;
}

bool replay_delete(const char *file)
{
	char path[1400];

	snprintf(path, sizeof(path), "%s%s", folder, file);
	return remove(path) == 0;
}

/* ---- playback ---- */

static track play;
static bool playing, play_training;
static uint32_t position, next_patch, next_check;
static uint16_t own_eeprom[CAVE_EEPROM_WORDS];
static bool own_slowdown;      /* the player's slowdown setting, put back as playback stops */
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
	if (!read_header(file, &info, begin, &play.start))
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

static void put_patches_back(void)
{
	while (next_patch < play.patch_count && play.patches[next_patch].frame <= position)
	{
		const patch *const p = &play.patches[next_patch++];

		for (unsigned at = 0; at < p->size; at++)
			*cave_work_ram_byte(p->address + at) = p->bytes[at];
	}
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
	put_patches_back();
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
	own_slowdown = program_slowdown();
	program_set_slowdown(play.start.slowdown);    /* (as it was recorded: the frames fall the same way) */
	playing = true;
	play_training = play.start.training;
	position = next_patch = next_check = 0;
	big_hit_set = out_of_step = false;
	tainted = true;    /* (no recording of a replay) */
	if (play_training)
	{
		/* the training place made again as it was: the training's run there, then the port's writes after it */
		const training_setup setup = setup_from_bytes(play.start.setup);
		const char *const not_there = training_prepare(&setup);

		if (not_there)
		{
			replay_stop();
			return not_there;
		}
		put_patches_back();
		return NULL;
	}
	program_reset();
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

bool replay_playing_training(void)
{
	return playing && play_training;
}

void replay_stop(void)
{
	if (!playing)
		return;
	playing = false;
	memcpy(cave_board.eeprom.words, own_eeprom, sizeof(own_eeprom));
	program_set_slowdown(own_slowdown);
	track_free(&play);
}
