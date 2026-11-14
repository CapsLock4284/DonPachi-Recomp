/* romset_versions_check [FOLDER]: the ROM set's two versions (cave/rom_set.h): the US set and the Japanese one
   ('donpachij': its program and text from donpachij.zip, the rest from donpachi.zip), read from FOLDER (default rom,
   holding both .zips), from donpachij.zip named alone (its parent beside it), from a merged .zip (both sets in one,
   made here as romset_merged.zip), and refused without the parent (the parent's files listed as missing); the
   extracted data of each saved and read back the same, and refused as the other version. PASS or FAIL each */
#include "cave/rom_set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void verdict(const char *what, int pass)
{
	printf("%-64s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

static unsigned long long hash(const uint8_t *data, unsigned size)
{
	unsigned long long h = 1469598103934665603ull;

	for (unsigned i = 0; i < size; i++)
		h = (h ^ data[i]) * 1099511628211ull;
	return h;
}

static unsigned long long set_hash(void)
{
	return hash(rom_set.program, DONPACHI_PROGRAM_SIZE) ^ hash(rom_set.sprites, DONPACHI_SPRITES_SIZE) * 3u
		^ hash(rom_set.layer[0], DONPACHI_LAYER01_SIZE) * 5u ^ hash(rom_set.layer[1], DONPACHI_LAYER01_SIZE) * 7u
		^ hash(rom_set.layer[2], DONPACHI_LAYER2_SIZE) * 11u ^ hash(rom_set.oki[0], DONPACHI_OKI1_SIZE) * 13u
		^ hash(rom_set.oki[1], DONPACHI_OKI2_SIZE) * 17u ^ hash((const uint8_t *)rom_set.eeprom, 128u) * 19u;
}

/* the program's version string (at 000400, as the 68000 sees it) begins with 'text' */
static int says(const char *text)
{
	return rom_set.program && memcmp(rom_set.program + 0x400u, text, strlen(text)) == 0;
}

int main(int argc, char **argv)
{
	const char *const folder = argc > 1 ? argv[1] : "rom";
	char path[1024];
	unsigned long long us_hash, japan_hash, layer2_us, layer2_japan;
	const char *problem;

	problem = rom_set_load_version(folder, DONPACHI_US);
	verdict("the US set from the folder", !problem && rom_set.version == DONPACHI_US && says("DONPACHI Ver1.12"));
	us_hash = set_hash();
	layer2_us = hash(rom_set.layer[2], DONPACHI_LAYER2_SIZE);
	problem = rom_set_load_version(folder, DONPACHI_JAPAN);
	if (problem)
		printf("  (%s)\n", problem);
	verdict("the Japanese set from the folder (donpachij.zip and its parent)",
		!problem && rom_set.version == DONPACHI_JAPAN && says("DONPACHI Ver1.01"));
	japan_hash = set_hash();
	layer2_japan = hash(rom_set.layer[2], DONPACHI_LAYER2_SIZE);
	verdict("its own text layer (u58.bin), the US one not", layer2_japan != layer2_us && japan_hash != us_hash);
	snprintf(path, sizeof(path), "%s/donpachij.zip", folder);
	problem = rom_set_load_version(path, DONPACHI_JAPAN);
	verdict("donpachij.zip named alone: its parent from beside it", !problem && set_hash() == japan_hash);
	problem = rom_set_load_version(path, DONPACHI_US);
	verdict("donpachij.zip as the US set: refused (it has no US program)", problem != NULL);

	/* a folder with the clone alone: refused, the parent's 7 files missing */
	{
		char command[1200];
		const char *names[ROM_SET_FILES];
		bool different[ROM_SET_FILES];
		int missing;

		snprintf(command, sizeof(command), "mkdir -p romset_alone && cp '%s/donpachij.zip' romset_alone/", folder);
		if (system(command) != 0)
			printf("  (%s: failed)\n", command);
		problem = rom_set_load_version("romset_alone", DONPACHI_JAPAN);
		missing = rom_set_files_missing(names, different, ROM_SET_FILES);
		printf("  (%s; %d missing, the first %s)\n", problem ? problem : "loaded", missing, missing ? names[0] : "-");
		verdict("the clone alone: refused, its parent's 7 files missing", problem != NULL && missing == 7
			&& rom_set_files_found() == 2);
		if (system("rm -rf romset_alone") != 0)
			printf("  (rm failed)\n");

		/* a merged set: one .zip with both (the clone's files in a folder of their own, as MAME's merged sets) */
		snprintf(command, sizeof(command), "rm -rf romset_m && mkdir -p romset_m/donpachij && cd romset_m && "
			"unzip -q '../%s/donpachi.zip' && cd donpachij && unzip -q '../../%s/donpachij.zip' && cd .. && "
			"zip -q -r ../romset_merged.zip . && cd .. && rm -rf romset_m", folder, folder);
		if (system(command) != 0)
			printf("  (%s: failed)\n", command);
		problem = rom_set_load_version("romset_merged.zip", DONPACHI_JAPAN);
		verdict("a merged .zip: the Japanese set", !problem && set_hash() == japan_hash);
		problem = rom_set_load_version("romset_merged.zip", DONPACHI_US);
		verdict("a merged .zip: the US set", !problem && set_hash() == us_hash);
		remove("romset_merged.zip");
	}

	/* the extracted data, each version's */
	rom_set_load_version(folder, DONPACHI_JAPAN);
	problem = rom_set_save_data("romset_japan.dat");
	verdict("the Japanese data saved", problem == NULL);
	rom_set_load_version(folder, DONPACHI_US);
	rom_set_save_data("romset_us.dat");
	problem = rom_set_load_data_version("romset_japan.dat", DONPACHI_JAPAN);
	verdict("the Japanese data read back the same",
		!problem && rom_set.version == DONPACHI_JAPAN && set_hash() == japan_hash);
	verdict("the Japanese data refused as the US set's", rom_set_load_data("romset_japan.dat") != NULL);
	verdict("the US data refused as the Japanese set's",
		rom_set_load_data_version("romset_us.dat", DONPACHI_JAPAN) != NULL);
	problem = rom_set_load_data("romset_us.dat");
	verdict("the US data read back the same", !problem && rom_set.version == DONPACHI_US && set_hash() == us_hash);
	remove("romset_japan.dat");
	remove("romset_us.dat");
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
