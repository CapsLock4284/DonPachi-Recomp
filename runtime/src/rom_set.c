/**
 *    Name:        rom_set.c
 *    Description: the player's DonPachi ROM set into memory, from a .zip or a folder
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    Each file is known by its CRC32 and size, whatever its name. The .zip reading is tmntarcade-decomp's
 *    runtime/src/rom_set.c (BSD-3-Clause, Sebastian Toyos: runtime/LICENSE.tmnt-runtime), with inflate.c; the
 *    regions follow ROM_START( donpachi ) in MAME's src/mame/cave/cave.cpp (BSD-3-Clause).
 */
#include "cave/rom_set.h"

#include "inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZIP_END             0x06054B50u
#define ZIP_ENTRY           0x02014B50u
#define ZIP_LOCAL           0x04034B50u
#define ZIP_STORED          0
#define ZIP_DEFLATED        8
#define ZIP_COMMENT_MOST    0xFFFFu
#define SET_ZIP             "donpachi.zip"

donpachi_rom_set rom_set;

typedef struct
{
	const char *name;
	uint32_t crc, size;
} rom_file;

enum { U29, U44, U45, U54, U57, U58, U32, U33, U10, FILES };

static const rom_file files[FILES] = {
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

const char *rom_set_load(const char *source)
{
	static char problem[256];
	uint8_t *chips[FILES] = { 0 };
	size_t size = 0;
	uint8_t *whole = read_whole(source, &size);

	if (whole)
	{
		if (!read_zip(whole, size, chips))
		{
			free(whole);
			snprintf(problem, sizeof(problem), "%s is no .zip", source);
			return problem;
		}
		free(whole);
	}
	else
	{
		char path[1024];

		read_folder(source, chips);
		snprintf(path, sizeof(path), "%s/%s", source, SET_ZIP);
		if (!chips[U29] && (whole = read_whole(path, &size)) != NULL)
		{
			read_zip(whole, size, chips);
			free(whole);
		}
	}
	for (int file = 0; file < FILES; file++)
		if (!chips[file])
		{
			snprintf(problem, sizeof(problem), "the ROM set in %s has no %s (CRC32 %08x), or a different one",
				source, files[file].name, (unsigned)files[file].crc);
			return problem;
		}

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
