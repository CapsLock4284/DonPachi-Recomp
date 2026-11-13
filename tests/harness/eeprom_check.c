/* eeprom_check: the EEPROM's file (cave_eeprom_load / cave_eeprom_save) in MAME's order, little endian words as
   its nvram/donpachi/eeprom (eeprom.cpp), and the port's older files (big endian, before 0.7.0) still read right:
     MAME's file of the ROM set's EEPROM, and of changed settings; the port's old file with the home defaults, and
     with changed settings; a file saved is MAME's order and reads back the same
   and the game's own reading of a MAME file: "C BUTTON FULL-AUTO" (word 4 bit 2) reaches $101287 bit 2 as it starts.
   PASS or FAIL each */
#include "common.h"

#include "cave/program.h"

static int failures;

static void verdict(const char *what, bool pass)
{
	printf("%-60s %s\n", what, pass ? "PASS" : "FAIL");
	if (!pass)
		failures++;
}

/* a file of 'words' in either order */
static void write_file(const char *path, const uint16_t *words, bool little)
{
	FILE *const file = fopen(path, "wb");

	for (int word = 0; word < CAVE_EEPROM_WORDS; word++)
	{
		const uint8_t high = (uint8_t)(words[word] >> 8), low = (uint8_t)words[word];

		fputc(little ? low : high, file);
		fputc(little ? high : low, file);
	}
	fclose(file);
}

/* the file loaded gives 'words' */
static bool loads_as(const char *path, const uint16_t *words)
{
	memset(cave_board.eeprom.words, 0, sizeof(cave_board.eeprom.words));
	return cave_eeprom_load(path) == 1 && !memcmp(cave_board.eeprom.words, words, sizeof(cave_board.eeprom.words));
}

int main(void)
{
	const char *const path = "eeprom_check.eeprom";
	uint16_t words[CAVE_EEPROM_WORDS], home[CAVE_EEPROM_WORDS], changed[CAVE_EEPROM_WORDS];
	uint8_t bytes[2 * CAVE_EEPROM_WORDS];
	FILE *file;
	bool little;
	unsigned with, without;

	start_game();
	memcpy(words, rom_set.eeprom, sizeof(words));
	memcpy(home, words, sizeof(home));
	home[1] = 0xFFFFu;
	home[4] |= 0x0004u;
	memcpy(changed, home, sizeof(changed));
	changed[0] = 0x0008u;    /* (other settings: word 0's low byte, word 4's high byte) */
	changed[4] = 0x0104u;

	write_file(path, words, true);
	verdict("MAME's file, the ROM set's EEPROM", loads_as(path, words));
	write_file(path, changed, true);
	verdict("MAME's file, settings changed", loads_as(path, changed));
	write_file(path, home, true);
	verdict("MAME's file, the home defaults", loads_as(path, home));
	write_file(path, home, false);
	verdict("the port's old file, the home defaults", loads_as(path, home));
	write_file(path, changed, false);
	verdict("the port's old file, settings changed", loads_as(path, changed));
	write_file(path, words, false);
	verdict("the port's old file, the ROM set's EEPROM", loads_as(path, words));

	memcpy(cave_board.eeprom.words, changed, sizeof(changed));
	cave_eeprom_save(path);
	file = fopen(path, "rb");
	little = file && fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes) && bytes[0] == (changed[0] & 0xFFu)
		&& bytes[1] == changed[0] >> 8 && bytes[8] == (changed[4] & 0xFFu);
	if (file)
		fclose(file);
	verdict("saved in MAME's order (little endian)", little);
	verdict("saved, read back the same", loads_as(path, changed));

	/* the game itself, from a MAME file: word 4 bit 2 (C BUTTON FULL-AUTO) into $101287 */
	write_file(path, home, true);
	program_reset();
	cave_eeprom_load(path);
	for (int frame = 0; frame < 300; frame++)
		program_run_frame();
	with = ram_byte(0x101287u);
	write_file(path, words, true);
	program_reset();
	cave_eeprom_load(path);
	for (int frame = 0; frame < 300; frame++)
		program_run_frame();
	without = ram_byte(0x101287u);
	printf("($101287: %02X with C BUTTON FULL-AUTO, %02X without)\n", with, without);
	verdict("the game reads MAME's file: C BUTTON FULL-AUTO", (with & 4u) && !(without & 4u));
	remove(path);
	printf("%s\n", failures ? "FAILED" : "ALL PASS");
	return failures != 0;
}
