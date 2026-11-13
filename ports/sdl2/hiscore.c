/**
 *    Name:        hiscore.c
 *    Description: the high score table kept in a file (hiscore.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "hiscore.h"
#include "paths.h"

#include "cave/machine.h"
#include "cave/program.h"

#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TABLE       0x101846u
#define TABLE_SIZE  0x5Au           /* to $10189F */
#define HUD_HIGH    0x101782u

static const char magic[8] = { 'D', 'P', 'H', 'I', 'S', 'C', '0', '1' };

/* the default scores, BCD: what the game's first run of func_00694E writes */
static const uint32_t default_scores[5] = { 0x800000u, 0x700000u, 0x600000u, 0x500000u, 0x400000u };

static char path[1024];
static uint8_t file_table[TABLE_SIZE];  /* the file's, as the game's bytes */
static bool have_file;
static uint8_t saved[TABLE_SIZE];       /* the table as last saved (or put back) */
static bool pending = true;             /* waiting for the game to set its table up */
static bool restored;                   /* the file's table was put back this boot: the table may be saved */
static bool put_back;                   /* it was put back in the game's memory since hiscore_take_put_back */

static void read_table(uint8_t *out)
{
	for (uint32_t at = 0; at < TABLE_SIZE; at++)
		out[at] = *cave_work_ram_byte(TABLE + at);
}

static uint32_t long_at(uint32_t address)
{
	uint32_t value = 0;

	for (uint32_t at = 0; at < 4u; at++)
		value = value << 8 | *cave_work_ram_byte(address + at);
	return value;
}

static bool defaults_set(void)
{
	for (int entry = 0; entry < 5; entry++)
		if (long_at(TABLE + 4u * (uint32_t)entry) != default_scores[entry])
			return false;
	return long_at(HUD_HIGH) == default_scores[0];
}

const char *hiscore_open(const char *file_path)
{
	FILE *file;
	uint8_t header[sizeof(magic)];

	if (file_path)
		snprintf(path, sizeof(path), "%s", file_path);
	else
		snprintf(path, sizeof(path), "%sdonpachi.hiscore", paths_folder());
	have_file = false;
	if (!(file = fopen(path, "rb")))
		return NULL;
	if (fread(header, 1, sizeof(header), file) != sizeof(header) || memcmp(header, magic, sizeof(magic))
			|| fread(file_table, 1, TABLE_SIZE, file) != TABLE_SIZE)
	{
		fclose(file);
		return "not a DonPachi high score file: the game's defaults are used (and saved over it)";
	}
	fclose(file);
	have_file = true;
	return NULL;
}

void hiscore_reset(void)
{
	pending = true;
	restored = false;
}

static void save(const uint8_t *table)
{
	FILE *const file = fopen(path, "wb");

	if (!file)
	{
		fprintf(stderr, "%s: could not be written\n", path);
		return;
	}
	if (fwrite(magic, 1, sizeof(magic), file) != sizeof(magic) || fwrite(table, 1, TABLE_SIZE, file) != TABLE_SIZE)
		fprintf(stderr, "%s: could not be written\n", path);
	fclose(file);
	memcpy(saved, table, TABLE_SIZE);
}

void hiscore_state_loaded(void)
{
	uint32_t high = 0;

	if (!restored || !have_file)
		return;
	for (uint32_t at = 0; at < TABLE_SIZE; at++)
		*cave_work_ram_byte(TABLE + at) = file_table[at];
	/* the HUD's HIGH: the file's #1, or the player's score already over it */
	for (uint32_t at = 0; at < 4u; at++)
		high = high << 8 | file_table[at];
	if (long_at(HUD_HIGH) < high)
		for (uint32_t at = 0; at < 4u; at++)
			*cave_work_ram_byte(HUD_HIGH + at) = file_table[at];
	read_table(saved);
}

bool hiscore_take_put_back(void)
{
	const bool was = put_back;

	put_back = false;
	return was;
}

void hiscore_frame(bool enabled, bool saving)
{
	uint8_t table[TABLE_SIZE];

	if (pending)
	{
		/* (the boot clears the memory, then the game writes its defaults, once) */
		if (!defaults_set())
			return;
		pending = false;
		if (!enabled)
			return;    /* (this boot's table is the game's own: it is not saved) */
		if (have_file)
		{
			for (uint32_t at = 0; at < TABLE_SIZE; at++)
				*cave_work_ram_byte(TABLE + at) = file_table[at];
			for (uint32_t at = 0; at < 4u; at++)
				*cave_work_ram_byte(HUD_HIGH + at) = file_table[at];    /* (#1's score) */
			put_back = true;
		}
		read_table(saved);
		restored = true;
		return;
	}
	if (!enabled || !restored || !saving || program_frame_lagged())
		return;    /* (a frame of slowdown: the table may be half written) */
	read_table(table);
	if (memcmp(table, saved, TABLE_SIZE))
	{
		save(table);
		memcpy(file_table, table, TABLE_SIZE);
		have_file = true;
	}
}
