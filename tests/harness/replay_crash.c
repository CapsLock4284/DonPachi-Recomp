/* replay_crash FILE.inp: a run's inputs, as the window port writes them when it stops or crashes
   (ports/sdl2/crash_log.h: donpachi_crash_DATE_TIME.inp), played again from the board's reset: the same version,
   EEPROM and slowdown setting, the same high score table put back after the boot (ports/sdl2/hiscore.c), each
   frame's inputs and the big HIT display as the port set it. The game is the same from them, so it stops where the
   run stopped (the runtime says where, as there); if it gets to the end, it says so, where the game was and a sum of
   the work RAM (crash_log_check compares it). The ROM set: DONPACHI_ROM, or rom/donpachi.zip (the Japanese version:
   the rom folder). ENDSHOT=FILE: the last picture (PPM) */
#include "common.h"

#include "cave/sound.h"
#include "cave/video.h"

#include "../../ports/sdl2/crash_log.h"
#include "../../ports/sdl2/hiscore.c"    /* (the port's, as it puts the table back) */

#define BIG_HIT_FLAGS 0x101738u    /* (main.c's big_hit_frame) */

static uint32_t get32(const unsigned char *bytes)
{
	return bytes[0] | bytes[1] << 8 | bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

int main(int argc, char **argv)
{
	FILE *file;
	unsigned char header[16], eeprom[128], table[HISCORE_TABLE_SIZE];
	unsigned char *frames;
	uint32_t count, sum = 0;
	bool big_hit_set = false;
	const char *problem, *rom;
	donpachi_version version;

	if (argc < 2 || !(file = fopen(argv[1], "rb")))
	{
		fprintf(stderr, "replay_crash FILE.inp\n");
		return 1;
	}
	if (fread(header, 1, 16, file) != 16 || memcmp(header, "DPCINP0", 7) || (header[7] != '1' && header[7] != '2') || fread(eeprom, 1, 128, file) != 128
			|| fread(table, 1, sizeof(table), file) != sizeof(table))
	{
		fprintf(stderr, "%s: not a run's inputs\n", argv[1]);
		return 1;
	}
	count = get32(header + 8);
	if (!(frames = malloc((size_t)count * 6u + 1u)) || fread(frames, 6, count, file) != count)
	{
		fprintf(stderr, "%s: %lu frames, fewer there\n", argv[1], (unsigned long)count);
		return 1;
	}
	fclose(file);
	version = header[13] ? DONPACHI_JAPAN : DONPACHI_US;
	printf("%s: %lu frames (%.1f minutes), slowdown %s, the %s version, high score table %s\n", argv[1],
		(unsigned long)count, count / 57.55 / 60.0, header[12] ? "on" : "off", header[13] ? "Japanese" : "US",
		header[14] ? "put back" : "the game's own");
	rom = getenv("DONPACHI_ROM") ? getenv("DONPACHI_ROM") : version == DONPACHI_JAPAN ? "rom" : "rom/donpachi.zip";
	if ((problem = rom_set_load_version(rom, version)) != NULL
			|| (program_set_vblank_lead(header[15] != 0), (problem = program_start()) != NULL))
	{
		fprintf(stderr, "%s\n", problem);
		return 1;
	}
	for (unsigned word = 0; word < CAVE_EEPROM_WORDS; word++)
		cave_board.eeprom.words[word] = (uint16_t)(eeprom[word * 2] << 8 | eeprom[word * 2 + 1]);
	program_reset();    /* (from the reset, with the run's EEPROM) */
	program_set_slowdown(header[12] != 0);
	hiscore_use_table(header[14] ? table : NULL);
	hiscore_reset();
	for (uint32_t frame = 0; frame < count; frame++)
	{
		const unsigned char *const at = frames + frame * 6u;
		uint8_t *const flags = cave_work_ram_byte(BIG_HIT_FLAGS);

		cave_board.in0 = (uint16_t)(at[0] | at[1] << 8);
		cave_board.in1 = (uint16_t)(at[2] | at[3] << 8);
		program_run_frame();
		if ((at[4] & CRASH_LOG_BIG_HIT) && ram_word(0x1018ECu) == 7u)
		{
			*flags |= 3u;
			big_hit_set = true;
		}
		else if (!(at[4] & CRASH_LOG_BIG_HIT) && big_hit_set)
		{
			*flags &= (uint8_t)~3u;
			big_hit_set = false;
		}
		cave_sound_drop();
		video_latch_sprites();
		hiscore_frame(header[14] != 0, false);
		(void)hiscore_take_put_back();
	}
	for (uint32_t address = 0x100000u; address < 0x110000u; address++)
		sum = sum * 31u + ram_byte(address);
	printf("played to the end: board frame %llu, stage %u, loop %u, game mode %u, no stop; work RAM sum %08X\n",
		(unsigned long long)cave_board.frame, ram_word(0x101A40u), ram_word(0x101A42u), ram_word(0x1018ECu),
		(unsigned)sum);
	if (getenv("ENDSHOT"))
	{
		static uint32_t board[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];

		video_draw(board);
		video_rotated(board, picture);
		save_ppm(getenv("ENDSHOT"), picture, VIDEO_HEIGHT, VIDEO_WIDTH);
	}
	return 0;
}
