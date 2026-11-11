/* romdata_check: the ROM extraction (rom_set_save_data / rom_set_load_data) checked: the set read from the .zip, saved
   as romdata_check.dat, read back, each region compared; then a damaged copy must be refused.
     romdata_check [ZIP]   (default rom/donpachi.zip; a merged set's .zip works the same) */
#include "cave/rom_set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long hash(const uint8_t *data, unsigned size)
{
	unsigned long long h = 1469598103934665603ull;

	for (unsigned i = 0; i < size; i++)
		h = (h ^ data[i]) * 1099511628211ull;
	return h;
}

static void hashes(unsigned long long *out)
{
	out[0] = hash(rom_set.program, DONPACHI_PROGRAM_SIZE);
	out[1] = hash(rom_set.sprites, DONPACHI_SPRITES_SIZE);
	out[2] = hash(rom_set.layer[0], DONPACHI_LAYER01_SIZE);
	out[3] = hash(rom_set.layer[1], DONPACHI_LAYER01_SIZE);
	out[4] = hash(rom_set.layer[2], DONPACHI_LAYER2_SIZE);
	out[5] = hash(rom_set.oki[0], DONPACHI_OKI1_SIZE);
	out[6] = hash(rom_set.oki[1], DONPACHI_OKI2_SIZE);
	out[7] = hash((const uint8_t *)rom_set.eeprom, sizeof(rom_set.eeprom));
}

int main(int argc, char **argv)
{
	unsigned long long from_zip[8], from_data[8];
	const char *problem;
	FILE *file;

	if ((problem = rom_set_load(argc > 1 ? argv[1] : "rom/donpachi.zip")) != NULL)
		return printf("zip: %s\n", problem), 1;
	hashes(from_zip);
	if ((problem = rom_set_save_data("romdata_check.dat")) != NULL)
		return printf("save: %s\n", problem), 1;
	if ((problem = rom_set_load_data("romdata_check.dat")) != NULL)
		return printf("load: %s\n", problem), 1;
	hashes(from_data);
	printf("regions the same: %s\n", memcmp(from_zip, from_data, sizeof(from_zip)) ? "NO" : "yes");
	if ((file = fopen("romdata_check.dat", "r+b")) != NULL)
	{
		fseek(file, 5000000, SEEK_SET);
		fputc('X', file);
		fclose(file);
	}
	problem = rom_set_load_data("romdata_check.dat");
	printf("a damaged file: %s\n", problem ? problem : "ACCEPTED (wrong)");
	remove("romdata_check.dat");
	return 0;
}
