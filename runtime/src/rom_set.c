/**
 *    Name:        rom_set.c
 *    Description: the player's DonPachi ROM set into memory, from a .zip or a folder
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    Each file is known by its CRC32 and size, whatever its name. The .zip reading is tmntarcade-decomp's
 *    runtime/src/rom_set.c (BSD-3-Clause, Sebastian Toyos: runtime/LICENSE.tmnt-runtime), with inflate.c; the
 *    regions follow ROM_START( donpachi ) and ROM_START( donpachij ) in MAME's src/mame/cave/cave.cpp (BSD-3-Clause).
 */
#include "cave/rom_set.h"

#include "inflate.h"

#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZIP_END             0x06054B50u
#define ZIP_ENTRY           0x02014B50u
#define ZIP_LOCAL           0x04034B50u
#define ZIP_STORED          0
#define ZIP_DEFLATED        8
#define ZIP_COMMENT_MOST    0xFFFFu
#define SET_ZIP             "donpachi.zip"    /* the US set, the Japanese one's parent */
#define CLONE_ZIP           "donpachij.zip"

donpachi_rom_set rom_set;

typedef struct
{
	const char *name;
	uint32_t crc, size;
} rom_file;

enum { U29, U44, U45, U54, U57, U58, U32, U33, U10, FILES };

/* what the last rom_set_load found of each file */
enum { FILE_MISSING, FILE_FOUND, FILE_DIFFERENT };
static int file_state[FILES];
typedef char files_counted[ROM_SET_FILES == FILES ? 1 : -1];    /* (rom_set.h's count, kept right) */
static bool name_seen[FILES];    /* (a file of its name, right or not) */

/* the files of the version asked for (the Japanese: its program and text, the rest the US set's) */
static const rom_file us_files[FILES] = {
	{ "prgu.u29", 0x89c36802u, 0x080000u },
	{ "atdp.u44", 0x7189e953u, 0x200000u },
	{ "atdp.u45", 0x6984173fu, 0x200000u },
	{ "atdp.u54", 0x6bda6b66u, 0x100000u },
	{ "atdp.u57", 0x0a0e72b9u, 0x100000u },
	{ "text.u58", 0x5dba06e7u, 0x040000u },
	{ "atdp.u32", 0x0d89fccau, 0x100000u },
	{ "atdp.u33", 0xd749de00u, 0x200000u },
	{ "eeprom-donpachi.u10", 0x315fb546u, 0x000080u },
};
static rom_file files[FILES];

static void use_version(donpachi_version version)
{
	memcpy(files, us_files, sizeof(files));
	if (version == DONPACHI_JAPAN)
	{
		files[U29] = (rom_file){ "prg.u29", 0x6be14af6u, 0x080000u };
		files[U58] = (rom_file){ "u58.bin", 0x285379ffu, 0x040000u };
	}
}

const char *rom_set_version_name(donpachi_version version)
{
	return version == DONPACHI_JAPAN ? "JAPAN" : "USA";
}

const char *rom_set_mame_name(donpachi_version version)
{
	return version == DONPACHI_JAPAN ? "donpachij" : "donpachi";
}

static uint32_t crc32_of(const uint8_t *data, uint32_t size)
{
	static uint32_t table[256];
	uint32_t crc = 0xFFFFFFFFu, index;
	int bit;

	if (table[1] == 0u)
		for (index = 0; index < 256u; index++)
		{
			table[index] = index;
			for (bit = 0; bit < 8; bit++)
				table[index] = (table[index] >> 1) ^ (0xEDB88320u & (0u - (table[index] & 1u)));
		}
	for (index = 0; index < size; index++)
		crc = (crc >> 8) ^ table[(crc ^ data[index]) & 0xFFu];
	return ~crc;
}

static uint32_t little16(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
}

static uint32_t little32(const uint8_t *bytes)
{
	return little16(bytes) | little16(bytes + 2) << 16;
}

static uint8_t *read_whole(const char *path, size_t *size)
{
	FILE *input = fopen(path, "rb");
	uint8_t *data = NULL;
	long length;

	if (!input)
		return NULL;
	if (fseek(input, 0, SEEK_END) == 0 && (length = ftell(input)) > 0 && fseek(input, 0, SEEK_SET) == 0
		&& (data = malloc((size_t)length)) != NULL && fread(data, 1, (size_t)length, input) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(input);
	if (data)
		*size = (size_t)length;
	return data;
}

/* a .zip entry's name ('length' bytes, maybe in a folder) is the set's file 'file' (in any case) */
static bool same_name(const uint8_t *name, size_t length, int file)
{
	const size_t want = strlen(files[file].name);
	size_t start = length;

	while (start > 0 && name[start - 1] != '/' && name[start - 1] != '\\')
		start--;
	if (length - start != want)
		return false;
	for (size_t at = 0; at < want; at++)
		if (tolower(name[start + at]) != files[file].name[at])
			return false;
	return true;
}

/* The set's files in the .zip 'zip' into 'chips', by their CRC32: 0 when it is no .zip */
static int read_zip(const uint8_t *zip, size_t size, uint8_t **chips)
{
	size_t end, entry, local;
	uint32_t entries, index;
	int file;

	if (size < 22)
		return 0;
	for (end = size - 22;; end--)
	{
		if (little32(zip + end) == ZIP_END)
			break;
		if (end == 0 || size - end > 22 + ZIP_COMMENT_MOST)
			return 0;
	}
	entries = little16(zip + end + 10);
	entry = little32(zip + end + 16);
	for (index = 0; index < entries; index++)
	{
		uint32_t method, crc, packed, unpacked;

		if (entry > size || size - entry < 46 || little32(zip + entry) != ZIP_ENTRY)
			return 0;
		method = little16(zip + entry + 10);
		crc = little32(zip + entry + 16);
		packed = little32(zip + entry + 20);
		unpacked = little32(zip + entry + 24);
		local = little32(zip + entry + 42);
		for (file = 0; file < FILES; file++)
			if (size - entry >= 46u + little16(zip + entry + 28)
					&& same_name(zip + entry + 46, little16(zip + entry + 28), file))
				name_seen[file] = true;
		entry += 46 + little16(zip + entry + 28) + little16(zip + entry + 30) + little16(zip + entry + 32);
		for (file = 0; file < FILES; file++)
			if (!chips[file] && files[file].crc == crc && files[file].size == unpacked)
				break;
		if (file == FILES || local > size || size - local < 30 || little32(zip + local) != ZIP_LOCAL)
			continue;
		local += 30 + little16(zip + local + 26) + little16(zip + local + 28);
		if (local > size || packed > size - local || (method == ZIP_STORED && packed != unpacked)
			|| !(chips[file] = malloc(unpacked)))
			continue;
		if ((method == ZIP_STORED ? (memcpy(chips[file], zip + local, unpacked), 1)
				: method == ZIP_DEFLATED && inflate_raw(zip + local, packed, chips[file], unpacked))
			&& crc32_of(chips[file], unpacked) == crc)
			continue;
		free(chips[file]);
		chips[file] = NULL;
	}
	return 1;
}

static void read_folder(const char *folder, uint8_t **chips)
{
	char path[1024];

	for (int file = 0; file < FILES; file++)
	{
		size_t size = 0;

		snprintf(path, sizeof(path), "%s/%s", folder, files[file].name);
		chips[file] = read_whole(path, &size);
		if (chips[file])
			name_seen[file] = true;
		if (chips[file] && (size != files[file].size || crc32_of(chips[file], files[file].size) != files[file].crc))
		{
			free(chips[file]);
			chips[file] = NULL;
		}
	}
}

/* ROM_LOAD16_WORD_SWAP: each 16-bit word's bytes the other way round */
static uint8_t *swapped(const uint8_t *data, uint32_t size)
{
	uint8_t *out = malloc(size);

	if (out)
		for (uint32_t i = 0; i + 1u < size; i += 2u)
		{
			out[i] = data[i + 1u];
			out[i + 1u] = data[i];
		}
	return out;
}

int rom_set_files_found(void)
{
	int found = 0;

	for (int file = 0; file < FILES; file++)
		found += file_state[file] == FILE_FOUND;
	return found;
}

int rom_set_files_missing(const char **names, bool *different, int most)
{
	int count = 0;

	for (int file = 0; file < FILES; file++)
		if (file_state[file] != FILE_FOUND)
		{
			if (count < most)
			{
				names[count] = files[file].name;
				different[count] = file_state[file] == FILE_DIFFERENT;
			}
			count++;
		}
	return count;
}

/* the files still missing from the .zip 'path', if it is there */
static void read_zip_file(const char *path, uint8_t **chips)
{
	size_t size = 0;
	uint8_t *const whole = read_whole(path, &size);

	if (whole)
	{
		read_zip(whole, size, chips);
		free(whole);
	}
}

static bool all_found(uint8_t **chips)
{
	for (int file = 0; file < FILES; file++)
		if (!chips[file])
			return false;
	return true;
}

const char *rom_set_load(const char *source)
{
	return rom_set_load_version(source, DONPACHI_US);
}

const char *rom_set_load_version(const char *source, donpachi_version version)
{
	static char problem[256];
	uint8_t *chips[FILES] = { 0 };
	size_t size = 0;
	uint8_t *whole = read_whole(source, &size);
	char path[1024];

	use_version(version);
	memset(file_state, 0, sizeof(file_state));
	memset(name_seen, 0, sizeof(name_seen));
	if (whole)
	{
		if (!read_zip(whole, size, chips))
		{
			free(whole);
			snprintf(problem, sizeof(problem), "%s is no .zip", source);
			return problem;
		}
		free(whole);
		if (version == DONPACHI_JAPAN && !all_found(chips))
		{
			/* (a clone's .zip: the rest from its parent's beside it) */
			const char *slash = strrchr(source, '/'), *const back = strrchr(source, '\\');
			int folder;

			if (back && (!slash || back > slash))
				slash = back;
			folder = slash ? (int)(slash - source) + 1 : 0;
			snprintf(path, sizeof(path), "%.*s%s", folder, source, SET_ZIP);
			read_zip_file(path, chips);
		}
	}
	else
	{
		read_folder(source, chips);
		if (version == DONPACHI_JAPAN && !all_found(chips))
		{
			snprintf(path, sizeof(path), "%s/%s", source, CLONE_ZIP);
			read_zip_file(path, chips);
		}
		if (!all_found(chips))
		{
			snprintf(path, sizeof(path), "%s/%s", source, SET_ZIP);
			read_zip_file(path, chips);
		}
	}
	for (int file = 0; file < FILES; file++)
		file_state[file] = chips[file] ? FILE_FOUND : name_seen[file] ? FILE_DIFFERENT : FILE_MISSING;
	for (int file = 0; file < FILES; file++)
		if (!chips[file])
		{
			snprintf(problem, sizeof(problem), "the ROM set in %s has no %s (CRC32 %08x), or a different one",
				source, files[file].name, (unsigned)files[file].crc);
			for (int other = 0; other < FILES; other++)
				free(chips[other]);
			return problem;
		}

	rom_set_free();    /* (the set loaded before: this one in its place) */
	rom_set.version = version;
	rom_set.program = swapped(chips[U29], DONPACHI_PROGRAM_SIZE);
	rom_set.sprites = malloc(DONPACHI_SPRITES_SIZE);
	rom_set.layer[0] = chips[U54];
	rom_set.layer[1] = chips[U57];
	rom_set.layer[2] = chips[U58];
	rom_set.oki[0] = calloc(1, DONPACHI_OKI1_SIZE);
	rom_set.oki[1] = calloc(1, DONPACHI_OKI2_SIZE);
	if (!rom_set.program || !rom_set.sprites || !rom_set.oki[0] || !rom_set.oki[1])
		return "no memory for the ROM set";
	{
		uint8_t *const low = swapped(chips[U44], 0x200000u), *const high = swapped(chips[U45], 0x200000u);

		if (!low || !high)
			return "no memory for the ROM set";
		memcpy(rom_set.sprites, low, 0x200000u);
		memcpy(rom_set.sprites + 0x200000u, high, 0x200000u);
		free(low);
		free(high);
	}
	memcpy(rom_set.oki[0] + 0x040000u, chips[U33], 0x200000u);
	memcpy(rom_set.oki[1] + 0x040000u, chips[U32], 0x100000u);
	memcpy(rom_set.oki[1] + 0x140000u, chips[U33], 0x200000u);
	for (int word = 0; word < 64; word++)
		rom_set.eeprom[word] = (uint16_t)(chips[U10][2 * word] << 8 | chips[U10][2 * word + 1]);    /* (16-bit, BE) */
	free(chips[U29]);
	free(chips[U44]);
	free(chips[U45]);
	free(chips[U32]);
	free(chips[U33]);
	free(chips[U10]);
	return NULL;
}

const char *rom_set_load_japan_over(const char *source)
{
	static char problem[256];
	uint8_t *chips[FILES] = { 0 };
	size_t size = 0;
	uint8_t *whole;
	uint8_t *program;

	if (!rom_set.program || rom_set.version != DONPACHI_US)
		return "the US set's data is needed first (the Japanese set shares its other chips)";
	use_version(DONPACHI_JAPAN);
	memset(file_state, 0, sizeof(file_state));
	memset(name_seen, 0, sizeof(name_seen));
	if ((whole = read_whole(source, &size)) != NULL)
	{
		const int is_zip = read_zip(whole, size, chips);

		free(whole);
		if (!is_zip)
		{
			snprintf(problem, sizeof(problem), "%s is no .zip", source);
			return problem;
		}
	}
	else
	{
		char path[1024];

		read_folder(source, chips);
		snprintf(path, sizeof(path), "%s/%s", source, CLONE_ZIP);
		if (!chips[U29] || !chips[U58])
			read_zip_file(path, chips);
	}
	/* (only the two of the Japanese set's own count: the others are the US set's, already here) */
	for (int file = 0; file < FILES; file++)
	{
		file_state[file] = file != U29 && file != U58 ? FILE_FOUND
			: chips[file] ? FILE_FOUND : name_seen[file] ? FILE_DIFFERENT : FILE_MISSING;
		if (file != U29 && file != U58)
		{
			free(chips[file]);
			chips[file] = NULL;
		}
	}
	if (!chips[U29] || !chips[U58])
	{
		snprintf(problem, sizeof(problem), "%s has no %s (CRC32 %08x), or a different one", source,
			files[chips[U29] ? U58 : U29].name, (unsigned)files[chips[U29] ? U58 : U29].crc);
		free(chips[U29]);
		free(chips[U58]);
		return problem;
	}
	if (!(program = swapped(chips[U29], DONPACHI_PROGRAM_SIZE)))
	{
		free(chips[U29]);
		free(chips[U58]);
		return "no memory for the ROM set";
	}
	free(chips[U29]);
	free(rom_set.program);
	free(rom_set.layer[2]);
	rom_set.program = program;
	rom_set.layer[2] = chips[U58];
	rom_set.version = DONPACHI_JAPAN;
	return NULL;
}

void rom_set_free(void)
{
	free(rom_set.program);
	free(rom_set.sprites);
	for (int layer = 0; layer < 3; layer++)
		free(rom_set.layer[layer]);
	free(rom_set.oki[0]);
	free(rom_set.oki[1]);
	memset(&rom_set, 0, sizeof(rom_set));
}

/* ---- the extracted data: a header, then the regions one after the other ----
   0  "DONPACHI-DATA" and 3 zero bytes
   16 the format (1), little endian 32-bit; the version (donpachi_version: 0 the US set, 'donpachi'; 1 the
      Japanese, 'donpachij'); the size of the regions; their CRC32 */

#define DATA_MAGIC      "DONPACHI-DATA"
#define DATA_HEADER     32u
#define DATA_FORMAT     1u

static const uint32_t region_sizes[] = {
	DONPACHI_PROGRAM_SIZE, DONPACHI_SPRITES_SIZE, DONPACHI_LAYER01_SIZE, DONPACHI_LAYER01_SIZE, DONPACHI_LAYER2_SIZE,
	DONPACHI_OKI1_SIZE, DONPACHI_OKI2_SIZE, 128u
};
#define REGIONS ((int)(sizeof(region_sizes) / sizeof(region_sizes[0])))

static uint8_t **region_of(donpachi_rom_set *set, int region)
{
	switch (region)
	{
	case 0: return &set->program;
	case 1: return &set->sprites;
	case 2: case 3: case 4: return &set->layer[region - 2];
	case 5: case 6: return &set->oki[region - 5];
	default: return NULL;    /* (the EEPROM: its words) */
	}
}

static uint32_t data_size(void)
{
	uint32_t size = 0;

	for (int region = 0; region < REGIONS; region++)
		size += region_sizes[region];
	return size;
}

static void put_little32(uint8_t *at, uint32_t value)
{
	for (int byte = 0; byte < 4; byte++)
		at[byte] = (uint8_t)(value >> (8 * byte));
}

const char *rom_set_save_data(const char *path)
{
	static char problem[1100];
	const uint32_t size = data_size();
	uint8_t *const whole = malloc(DATA_HEADER + size);
	uint8_t *at;
	char partial[1040];
	FILE *output;
	size_t written;

	if (!rom_set.program)
		return "no ROM set read";
	if (!whole)
		return "no memory for the data";
	memset(whole, 0, DATA_HEADER);
	memcpy(whole, DATA_MAGIC, sizeof(DATA_MAGIC) - 1u);
	at = whole + DATA_HEADER;
	for (int region = 0; region < REGIONS; region++)
	{
		uint8_t **const source = region_of(&rom_set, region);

		if (source)
			memcpy(at, *source, region_sizes[region]);
		else
			for (int word = 0; word < 64; word++)
			{
				at[2 * word] = (uint8_t)(rom_set.eeprom[word] >> 8);
				at[2 * word + 1] = (uint8_t)rom_set.eeprom[word];
			}
		at += region_sizes[region];
	}
	put_little32(whole + 16, DATA_FORMAT);
	put_little32(whole + 20, (uint32_t)rom_set.version);
	put_little32(whole + 24, size);
	put_little32(whole + 28, crc32_of(whole + DATA_HEADER, size));
	/* written beside it first, then put in its place: a program stopped half way leaves no damaged file */
	snprintf(partial, sizeof(partial), "%s.part", path);
	output = fopen(partial, "wb");
	if (!output)
	{
		free(whole);
		snprintf(problem, sizeof(problem), "cannot write %s", partial);
		return problem;
	}
	written = fwrite(whole, 1, DATA_HEADER + size, output);
	free(whole);
	if (fclose(output) != 0 || written != DATA_HEADER + size)
	{
		remove(partial);
		snprintf(problem, sizeof(problem), "cannot write %s (is the disk full?)", partial);
		return problem;
	}
	remove(path);    /* (Windows' rename will not replace a file) */
	if (rename(partial, path) != 0)
	{
		remove(partial);
		snprintf(problem, sizeof(problem), "cannot write %s", path);
		return problem;
	}
	return NULL;
}

const char *rom_set_load_data(const char *path)
{
	return rom_set_load_data_version(path, DONPACHI_US);
}

const char *rom_set_load_data_version(const char *path, donpachi_version version)
{
	static char problem[1100];
	const uint32_t size = data_size();
	size_t length = 0;
	uint8_t *const whole = read_whole(path, &length);
	donpachi_rom_set set;
	const uint8_t *at;

	if (!whole)
	{
		snprintf(problem, sizeof(problem), "no %s", path);
		return problem;
	}
	if (length != DATA_HEADER + size || memcmp(whole, DATA_MAGIC, sizeof(DATA_MAGIC) - 1u) != 0
			|| little32(whole + 16) != DATA_FORMAT || little32(whole + 20) != (uint32_t)version
			|| little32(whole + 24) != size || little32(whole + 28) != crc32_of(whole + DATA_HEADER, size))
	{
		free(whole);
		snprintf(problem, sizeof(problem), "%s is damaged, or from another version", path);
		return problem;
	}
	memset(&set, 0, sizeof(set));
	set.version = version;
	at = whole + DATA_HEADER;
	for (int region = 0; region < REGIONS; region++)
	{
		uint8_t **const target = region_of(&set, region);

		if (!target)
			for (int word = 0; word < 64; word++)
				set.eeprom[word] = (uint16_t)(at[2 * word] << 8 | at[2 * word + 1]);
		else if ((*target = malloc(region_sizes[region])) != NULL)
			memcpy(*target, at, region_sizes[region]);
		else
		{
			for (int other = 0; other < region; other++)
				if (region_of(&set, other))
					free(*region_of(&set, other));
			free(whole);
			return "no memory for the data";
		}
		at += region_sizes[region];
	}
	free(whole);
	rom_set_free();
	rom_set = set;
	return NULL;
}
