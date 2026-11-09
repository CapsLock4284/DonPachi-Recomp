/**
 *    Name:        main.c
 *    Description: DonPachi with no window: runs the decompiled program for a number of frames from the
 *                 player's ROM set, for the checks
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    Usage: donpachi_headless [--rom PATH] [--frames N] [--shot FRAME ...] [--shot-dir DIR] [--cheat NAME ...]
 *      --rom       the ROM set: MAME's donpachi.zip, or a folder of its files (default: rom)
 *      --frames    how many frames to run (default 600, ten seconds)
 *      --shot      a frame to save as a picture, as the cabinet shows it (BMP, 240 x 320); several may follow
 *      --shot-dir  where the pictures go (default: the current folder)
 *      --input FROM:TO:PLAYER:BITS  holds inputs from frame FROM to TO (player 1 or 2; BITS hexadecimal, 1 =
 *                      pressed: 1 up, 2 down, 4 left, 8 right, 10 shot, 20 bomb, 40 button 3, 80 start, 100 coin)
 *      --hashes FILE   writes "FRAME HASH" for every frame, the board's memories (cave_state_hash), to compare
 *                      with tools/cref's, the original program on MAME's 68000
 *      --dump FRAME    writes that frame's memories to the shot folder (dump_FRAME.bin); several may follow
 *      --cheat NAME    turns on the cheat of the cheat file whose name holds NAME ("p1 infinite lives"), as many
 *                      times as there are cheats; NAME=VALUE picks a value where the cheat asks for one
 *      --cheats FILE   the cheat file, MAME's format (default: cheats/donpachi.xml)
 *      --list-cheats   lists the cheat file's cheats
 *      --wav FILE      writes the sound (48 kHz, 16-bit mono WAV)
 *      --eeprom FILE   the EEPROM from FILE, if there is one, and back to it at the end (as the window's port)
 *      --free-play     Start drops in a coin first when there is no credit (cave/free_play.h), as the window's port
 *      --arcade-timing the pictures drawn when the board's would be (program.h's program_after_vblank): a frame
 *                      later than the default, which draws each as soon as the game's frame is done
 */
#include "cave/cheats.h"
#include "cave/free_play.h"
#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "cave/video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <signal.h>
#include <unistd.h>

/* (a frame that never ends: the program waits on something the board does not do yet) */
static void on_alarm(int signal_number)
{
	(void)signal_number;
	fprintf(stderr, "stuck: frame %llu has not ended after 10 seconds\n", (unsigned long long)cave_board.frame);
	_exit(5);
}
#endif

#define SHOTS_MAX 512

static void write_bmp(const char *path, const uint32_t *pixels, int width, int height)
{
	FILE *output = fopen(path, "wb");
	const uint32_t row = ((uint32_t)width * 3u + 3u) & ~3u, size = 54u + row * (uint32_t)height;
	uint8_t header[54] = { 'B', 'M' };

	if (!output)
	{
		fprintf(stderr, "cannot write %s\n", path);
		return;
	}
	for (int byte = 0; byte < 4; byte++)
	{
		header[2 + byte] = (uint8_t)(size >> (8 * byte));
		header[18 + byte] = (uint8_t)((uint32_t)width >> (8 * byte));
		header[22 + byte] = (uint8_t)((uint32_t)height >> (8 * byte));
	}
	header[10] = 54;
	header[14] = 40;
	header[26] = 1;
	header[28] = 24;
	fwrite(header, 1, sizeof(header), output);
	for (int y = height - 1; y >= 0; y--)
	{
		uint8_t line[VIDEO_WIDTH * 3 + 4] = { 0 };

		for (int x = 0; x < width; x++)
		{
			const uint32_t pixel = pixels[y * width + x];

			line[x * 3] = (uint8_t)pixel;
			line[x * 3 + 1] = (uint8_t)(pixel >> 8);
			line[x * 3 + 2] = (uint8_t)(pixel >> 16);
		}
		fwrite(line, 1, row, output);
	}
	fclose(output);
}

#define CHEATS_MAX 16
#define INPUTS_MAX 512

typedef struct
{
	unsigned long long from, to;
	int player;
	unsigned bits;
} held_input;

/* the sound so far into 'wav' (its header is written at the end, when its size is known) */
static unsigned long wav_samples;

static void take_sound(FILE *wav)
{
	int16_t samples[4096];
	unsigned count;

	while ((count = cave_sound_take(samples, 4096u)) > 0)
		if (wav)
		{
			for (unsigned index = 0; index < count; index++)
			{
				fputc(samples[index] & 0xFF, wav);
				fputc((samples[index] >> 8) & 0xFF, wav);
			}
			wav_samples += count;
		}
}

static void put32(FILE *file, uint32_t value)
{
	for (int byte = 0; byte < 4; byte++)
		fputc((int)(value >> (8 * byte)) & 0xFF, file);
}

static void finish_wav(FILE *wav)
{
	const uint32_t data = (uint32_t)wav_samples * 2u;

	rewind(wav);
	fputs("RIFF", wav);
	put32(wav, 36u + data);
	fputs("WAVEfmt ", wav);
	put32(wav, 16u);
	put32(wav, 1u | 1u << 16);                  /* PCM, mono */
	put32(wav, CAVE_SOUND_RATE);
	put32(wav, CAVE_SOUND_RATE * 2u);
	put32(wav, 2u | 16u << 16);                 /* 2 bytes a sample, 16 bits */
	fputs("data", wav);
	put32(wav, data);
	fclose(wav);
}

/* --arcade-timing: the picture, drawn as the vblank interrupt returns, with the sprites latched as VBLANK began */
static uint32_t arcade_frame[VIDEO_WIDTH * VIDEO_HEIGHT], arcade_picture[VIDEO_WIDTH * VIDEO_HEIGHT];

static void draw_arcade_picture(void)
{
	video_draw(arcade_frame);
	video_rotated(arcade_frame, arcade_picture);
}

int main(int argc, char **argv)
{
	int arcade_timing = 0, free_play = 0;
	FILE *wav = NULL;
	const char *eeprom = NULL;
	static uint32_t frame[VIDEO_WIDTH * VIDEO_HEIGHT], picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	unsigned long long shots[SHOTS_MAX];
	int shot_count = 0;
	const char *shot_dir = ".";
	const char *rom = "rom";
	const char *cheat_file = "cheats/donpachi.xml", *cheat_names[CHEATS_MAX];
	int cheat_count = 0, list_cheats = 0;
	held_input inputs[INPUTS_MAX];
	FILE *hashes = NULL;
	unsigned long long dumps[SHOTS_MAX];
	int dump_count = 0;
	int input_count = 0;
	unsigned long long frames = 600;
	const char *problem;

	for (int index = 1; index < argc; index++)
	{
		if (!strcmp(argv[index], "--rom") && index + 1 < argc)
			rom = argv[++index];
		else if (!strcmp(argv[index], "--cheat") && index + 1 < argc && cheat_count < CHEATS_MAX)
			cheat_names[cheat_count++] = argv[++index];
		else if (!strcmp(argv[index], "--cheats") && index + 1 < argc)
			cheat_file = argv[++index];
		else if (!strcmp(argv[index], "--input") && index + 1 < argc && input_count < INPUTS_MAX)
		{
			held_input *const input = &inputs[input_count++];

			if (sscanf(argv[++index], "%llu:%llu:%d:%x", &input->from, &input->to, &input->player, &input->bits) != 4)
			{
				fprintf(stderr, "--input FROM:TO:PLAYER:BITS, as --input 1500:1510:1:80\n");
				return 2;
			}
		}
		else if (!strcmp(argv[index], "--hashes") && index + 1 < argc)
		{
			if (!(hashes = fopen(argv[++index], "w")))
			{
				fprintf(stderr, "cannot write %s\n", argv[index]);
				return 2;
			}
		}
		else if (!strcmp(argv[index], "--dump") && index + 1 < argc)
			while (index + 1 < argc && argv[index + 1][0] != '-' && dump_count < SHOTS_MAX)
				dumps[dump_count++] = strtoull(argv[++index], NULL, 10);
		else if (!strcmp(argv[index], "--wav") && index + 1 < argc)
		{
			if (!(wav = fopen(argv[++index], "wb")))
			{
				fprintf(stderr, "cannot write %s\n", argv[index]);
				return 1;
			}
			fseek(wav, 44L, SEEK_SET);
		}
		else if (!strcmp(argv[index], "--free-play"))
			free_play = 1;
		else if (!strcmp(argv[index], "--arcade-timing"))
			arcade_timing = 1;
		else if (!strcmp(argv[index], "--eeprom") && index + 1 < argc)
			eeprom = argv[++index];
		else if (!strcmp(argv[index], "--list-cheats"))
			list_cheats = 1;
		else if (!strcmp(argv[index], "--frames") && index + 1 < argc)
			frames = strtoull(argv[++index], NULL, 10);
		else if (!strcmp(argv[index], "--shot") && index + 1 < argc)
			while (index + 1 < argc && argv[index + 1][0] != '-' && shot_count < SHOTS_MAX)
				shots[shot_count++] = strtoull(argv[++index], NULL, 10);
		else if (!strcmp(argv[index], "--shot-dir") && index + 1 < argc)
			shot_dir = argv[++index];
		else
		{
			fprintf(stderr, "usage: %s [--rom PATH] [--frames N] [--shot FRAME ...] [--shot-dir DIR] [--cheat NAME ...] [--list-cheats]\n", argv[0]);
			return 2;
		}
	}
	if ((problem = rom_set_load(rom)) != NULL || (problem = program_start()) != NULL)
	{
		fprintf(stderr, "The game does not start: %s\n", problem);
		return 1;
	}
	if (eeprom && cave_eeprom_load(eeprom) < 0)
		fprintf(stderr, "%s is not an EEPROM of 128 bytes: the ROM set's is used\n", eeprom);
	if (list_cheats)
	{
		printf("the cheats of %s:\n", cheat_file);
		cheats_list(cheat_file);
		return 0;
	}
	for (int cheat = 0; cheat < cheat_count; cheat++)
		if ((problem = cheats_enable(cheat_file, cheat_names[cheat])) != NULL)
		{
			fprintf(stderr, "%s\n", problem);
			return 1;
		}
#ifndef _WIN32
	signal(SIGALRM, on_alarm);
#endif
	if (arcade_timing)
		program_after_vblank = draw_arcade_picture;
	while (program_frames() < frames)
	{
#ifndef _WIN32
		alarm(10);
#endif
		{
			uint16_t pressed[2] = { 0, 0 };

			for (int input = 0; input < input_count; input++)
				if (program_frames() >= inputs[input].from && program_frames() <= inputs[input].to)
					pressed[inputs[input].player == 2] |= (uint16_t)inputs[input].bits;
			cave_board.in0 = (uint16_t)~pressed[0];
			cave_board.in1 = (uint16_t)~pressed[1];
			if (free_play)
				free_play_inputs();
		}
		program_run_frame();
		cheats_apply();
		if (arcade_timing)
			video_latch_sprites();    /* (VBLANK: the sprites the next picture shows) */
		take_sound(wav);
		if (hashes)
			fprintf(hashes, "%llu %016llx\n", (unsigned long long)program_frames(), (unsigned long long)cave_state_hash());
		for (int dump = 0; dump < dump_count; dump++)
			if (dumps[dump] == program_frames())
			{
				char path[1024];

				snprintf(path, sizeof(path), "%s/dump_%05llu.bin", shot_dir, (unsigned long long)program_frames());
				cave_dump(path);
			}
		for (int shot = 0; shot < shot_count; shot++)
			if (shots[shot] == program_frames())
			{
				char path[1024];

				if (!arcade_timing)
				{
					video_latch_sprites();
					video_draw(frame);
					video_rotated(frame, picture);
				}
				snprintf(path, sizeof(path), "%s/frame_%05llu.bmp", shot_dir, (unsigned long long)program_frames());
				write_bmp(path, arcade_timing ? arcade_picture : picture, VIDEO_HEIGHT, VIDEO_WIDTH);
			}
		if (program_frames() % 60 == 0)
			printf("frame %llu: %u interrupts, %u unmapped reads (last %06X), %u unmapped writes (last %06X)\n",
				(unsigned long long)program_frames(), (unsigned)program_interrupts(),
				(unsigned)cave_board.unmapped_reads, (unsigned)cave_board.last_unmapped_read,
				(unsigned)cave_board.unmapped_writes, (unsigned)cave_board.last_unmapped_write);
	}
	if (hashes)
		fclose(hashes);
	if (wav)
		finish_wav(wav);
	if (eeprom && (problem = cave_eeprom_save(eeprom)) != NULL)
		fprintf(stderr, "%s: %s\n", eeprom, problem);
	printf("ran %llu frames\n", (unsigned long long)program_frames());
	{
		unsigned layers[3] = { 0 }, sprites = 0, colours = 0;

		for (int layer = 0; layer < 3; layer++)
			for (unsigned word = 0; word < CAVE_LAYER_RAM_WORDS; word++)
				layers[layer] += cave_board.layer_ram[layer][word] != 0;
		for (unsigned word = 0; word < CAVE_SPRITE_RAM_WORDS; word++)
			sprites += cave_board.sprite_ram[word] != 0;
		for (unsigned word = 0; word < CAVE_PALETTE_WORDS; word++)
			colours += cave_board.palette[word] != 0;
		printf("non-zero words: layer 0 %u, layer 1 %u, layer 2 %u, sprites %u, palette %u\n",
			layers[0], layers[1], layers[2], sprites, colours);
		printf("layer registers: 0 %04X %04X %04X, 1 %04X %04X %04X, 2 %04X %04X %04X\n",
			cave_board.layer_regs[0][0], cave_board.layer_regs[0][1], cave_board.layer_regs[0][2],
			cave_board.layer_regs[1][0], cave_board.layer_regs[1][1], cave_board.layer_regs[1][2],
			cave_board.layer_regs[2][0], cave_board.layer_regs[2][1], cave_board.layer_regs[2][2]);
		printf("game_mode %04X, stage %04X, scroll %04X, loop_counter %04X, EEPROM lines last %02X\n",
			*(uint16_t *)cave_work_ram_at(0x1018ECu, 2), *(uint16_t *)cave_work_ram_at(0x101A40u, 2),
			*(uint16_t *)cave_work_ram_at(0x101A56u, 2), *(uint16_t *)cave_work_ram_at(0x10125Eu, 2),
			cave_board.eeprom_lines);
	}
	return 0;
}
