/* mame_check: the port against the real MAME's donpachi, from what tools/mame/dp_check.lua wrote (its folder, DIR):
     mame_check DIR            MAME's own pictures all through the game against the port's drawing of the same
                               moments: each video_N.bin (the board's video memories MAME kept) put in the port's board
                               and drawn as the port draws, beside MAME's picture (screen_N.bin). The two games needn't
                               stay together: it compares the drawing (sprites, layers, scrolling, palette, priorities)
                               at every stage the run reaches. Each point is drawn from VBLANK N's memories and N-1's
                               (the sprites and the rest each from either, the sprites from either of the game's two
                               banks: MAME's memories are kept after its interrupt began), the one nearest MAME's
                               picture N compared. The first that differ (5) as mame_N.ppm, port_N.ppm and diff_N.ppm.
     SOUND=1 mame_check DIR    the port played with the inputs MAME was given (log.txt) and the same shield, its sound
                               written as DIR/port.wav (48 kHz mono, as the port plays it) to set beside MAME's own
                               (DIR/sound.wav: -wavwrite) with tests/harness/sound_compare.py; and where the two games
                               part (the signature each VBLANK: the ships, the scores, the stage), as from there the
                               sounds are of two different games: it stops 3,000 VBLANKs after (SOUND_AFTER to change
                               it), or at SOUND_UNTIL VBLANKs; SOUND_SHIFT=N takes MAME's VBLANK as the port's + N
                               and SOUND_DELAY=D its inputs as read D VBLANKs after they are logged, without trying
                               them (MAME 0.289 with dp_check.lua 0.1.0: delay 1, shift -3). And the slowdown: each
                               frame of the game's end (its wait made ready) against MAME's (waits.txt, dp_check.lua
                               0.2.0), lined up at the ship select, to where the games part: the ends' difference in
                               cycles and the frames at a different VBLANK (DIR/port_waits.txt, waits_compare.txt)
     MAKE=1 mame_check DIR     writes DIR from the port itself, as dp_check.lua and MAME would (the video states, log.txt
                               and sound.wav; DP_FRAMES, DP_VIDEO_EVERY), to check this tool and sound_compare.py:
                               drawn again all the same, the games together all through, the sounds alike
   The ROM set: common.h's (rom/donpachi.zip). */
#include "common.h"
#include "cave/sound.h"
#include "cave/video.h"
#include "game/support.h"    /* (m68k_time: the 68000's cycles) */

#include <stdbool.h>
#include <dirent.h>
#include <sys/wait.h>
#include <unistd.h>

/* ---- video_N.bin: the board's video memories, as the 68000 reads them (big endian) ---- */

static const char magic[8] = { 'D', 'P', 'C', 'V', 'I', 'D', '0', '1' };

static const struct
{
	uint32_t address, size;     /* 0: the video registers (MAME's own copy: not read through the 68000) */
} parts[] = {
	{ 0x500000u, 0x10000u },    /* the sprites */
	{ 0x300000u, 0x8000u },     /* layer 0 */
	{ 0x200000u, 0x8000u },     /* layer 1 */
	{ 0x400000u, 0x8000u },     /* layer 2 */
	{ 0x700000u, 6u },          /* the layers' registers: 0, 1, 2 */
	{ 0x600000u, 6u },
	{ 0x800000u, 6u },
	{ 0xA08000u, 0x1000u },     /* the palette */
	{ 0u, 0x80u },              /* the video registers */
	{ 0x100000u, 0x10000u },    /* the work RAM */
};
#define PARTS ((int)(sizeof(parts) / sizeof(parts[0])))
#define VIDEO_REGS_PART 8
#define WORK_RAM_PART   9

static size_t video_size(void)
{
	size_t size = sizeof(magic);

	for (int part = 0; part < PARTS; part++)
		size += parts[part].size;
	return size;
}

static uint8_t *read_file(const char *path, size_t *size)
{
	FILE *const file = fopen(path, "rb");
	uint8_t *data;
	long length;

	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = malloc(length > 0 ? (size_t)length : 1u);
	*size = fread(data, 1, (size_t)length, file);
	fclose(file);
	return data;
}

/* a part of a video_N.bin into the board: through the 68000's writes, the video registers straight in */
static void put_part(const uint8_t *data, int part)
{
	const uint8_t *at = data + sizeof(magic);

	for (int before = 0; before < part; before++)
		at += parts[before].size;
	if (part == VIDEO_REGS_PART)
	{
		for (uint32_t word = 0; word < parts[part].size / 2u && word < CAVE_VIDEO_REG_WORDS; word++)
			cave_board.video_regs[word] = (uint16_t)(at[2u * word] << 8 | at[2u * word + 1u]);
		return;
	}
	for (uint32_t offset = 0; offset + 1u < parts[part].size; offset += 2u)
		cave_write16(parts[part].address + offset, (uint16_t)(at[offset] << 8 | at[offset + 1u]), 0xFFFFu);
}

/* the picture drawn from these memories: the sprites latched from 'sprites' (the other bank when 'other_bank'),
   the rest from 'rest' */
static void draw_from(const uint8_t *sprites, const uint8_t *rest, bool other_bank, uint32_t *picture)
{
	for (int part = 0; part < PARTS; part++)
		if (part != WORK_RAM_PART)
			put_part(sprites, part);
	if (other_bank)
		cave_board.video_regs[4] ^= 1u;
	video_latch_sprites();
	for (int part = 0; part < PARTS; part++)
		if (part != WORK_RAM_PART)
			put_part(rest, part);
	video_draw(picture);
}

static bool mame_picture(const char *dir, unsigned n, uint32_t *picture)
{
	char path[1024];
	size_t size = 0;
	uint8_t *data;

	snprintf(path, sizeof(path), "%s/screen_%u.bin", dir, n);
	if (!(data = read_file(path, &size)))
		return false;
	if (size < (size_t)VIDEO_WIDTH * VIDEO_HEIGHT * 4u)
	{
		free(data);
		return false;
	}
	for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
		picture[i] = (uint32_t)data[4 * i] | (uint32_t)data[4 * i + 1] << 8 | (uint32_t)data[4 * i + 2] << 16;
	free(data);
	return true;
}

static unsigned differing(const uint32_t *a, const uint32_t *b)
{
	unsigned count = 0;

	for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
		count += (a[i] & 0xFFFFFFu) != (b[i] & 0xFFFFFFu);
	return count;
}

/* the signature dp_check.lua logs: the ships' records, the scores, the stage and its scroll */
static uint32_t signature(void)
{
	uint32_t sum = 0;

	for (uint32_t address = 0x102158u; address <= 0x1021D6u; address += 2u)
		sum += ram_word(address);
	sum += ram_word(0x10177Au) + ram_word(0x10177Cu) + ram_word(0x10177Eu) + ram_word(0x101780u);
	sum += ram_word(0x101A40u) + ram_word(0x101A42u) + ram_word(0x101A56u);
	return sum;
}

/* ---- the game's frames: as it makes its wait for the next ready (dp_check.lua's waits.txt) ---- */

static FILE *waits_out;            /* the port's: "game_frame vblank cycle mode random" */
static unsigned waits_frames;
static bool waits_arrived;
#define WAITS_KEPT 40000u
static unsigned long long waits_cycle[WAITS_KEPT];    /* (each frame's end, for the trials) */
static unsigned waits_vblank[WAITS_KEPT];

/* (the program's wait, each round: a wait it has made ready, once, as slowdown_check's at_wait; the cycle from the
   reset, MAME's from the power on: compared from a frame both have) */
static FILE *rounds_out;            /* ROUNDS=A-B: each round of the wait in VBLANKs A to B, as dp_check.lua's rounds.txt */
static unsigned rounds_from, rounds_to;

static void waits_at(void)
{
	if (rounds_out && cave_board.frame >= rounds_from && cave_board.frame <= rounds_to)
		fprintf(rounds_out, "%u %llu %x\n", (unsigned)cave_board.frame, (unsigned long long)m68k_time,
			ram_word(0x101262u) & 0xFFu);
	if (waits_arrived || !(ram_word(0x1011FCu) & 1u))
		return;
	waits_arrived = true;
	if (waits_frames < WAITS_KEPT)
	{
		waits_cycle[waits_frames] = m68k_time;
		waits_vblank[waits_frames] = (unsigned)cave_board.frame;
	}
	waits_frames++;
	if (waits_out)
		fprintf(waits_out, "%u %u %llu %x %u\n", waits_frames, (unsigned)cave_board.frame,
			(unsigned long long)m68k_time, ram_word(0x1018ECu), ram_word(0x101262u) & 0xFFu);
}

/* the reads the board answers by the time, as dp_check.lua's irq.txt: "vblank cycle what value" (the port's VBLANK
   counted as MAME's notifier counts: the interrupt's handler of VBLANK N reads before its notifier N) */
static FILE *irq_out;
static unsigned irq_frames = 8000u;

static void irq_read(uint32_t word, uint16_t value)
{
	static const char *const names[6] = { "irq0", "irq1", "irq2", "irq3", "oki0", "oki1" };

	if (irq_out && cave_board.frame < irq_frames && word < 6u)
		fprintf(irq_out, "%u %llu %s %x\n", (unsigned)cave_board.frame, (unsigned long long)m68k_time, names[word],
			value);
}

static void waits_begin(const char *dir, const char *name)
{
	char path[1024];

	snprintf(path, sizeof(path), "%s/%s", dir, name);
	waits_out = fopen(path, "w");
	waits_frames = 0;
	waits_arrived = false;
	program_at_wait = waits_at;
	snprintf(path, sizeof(path), "%s/%s", dir, strcmp(name, "waits.txt") ? "port_irq.txt" : "irq.txt");
	irq_out = fopen(path, "w");
	cave_irq_cause_read = irq_read;
	if (getenv("ROUNDS") && sscanf(getenv("ROUNDS"), "%u-%u", &rounds_from, &rounds_to) == 2)
	{
		snprintf(path, sizeof(path), "%s/%s", dir, strcmp(name, "waits.txt") ? "port_rounds.txt" : "rounds.txt");
		rounds_out = fopen(path, "w");
	}
}

static void waits_end(void)
{
	program_at_wait = NULL;
	cave_irq_cause_read = NULL;
	if (waits_out)
		fclose(waits_out);
	waits_out = NULL;
	if (irq_out)
		fclose(irq_out);
	irq_out = NULL;
	if (rounds_out)
		fclose(rounds_out);
	rounds_out = NULL;
}

typedef struct
{
	unsigned vblank, mode, random;    /* (random: 256 when not logged) */
	unsigned long long cycle;
} wait_row;

#define WAITS_MOST 400000u

static unsigned waits_read(const char *path, wait_row *rows)
{
	FILE *file = fopen(path, "r");
	char line[256];
	unsigned count = 0;

	if (!file)
		return 0;
	while (fgets(line, sizeof(line), file) && count < WAITS_MOST)
	{
		unsigned frame, vblank, mode, random = 256u;
		unsigned long long cycle;

		if (sscanf(line, "%u %u %llu %x %u", &frame, &vblank, &cycle, &mode, &random) >= 4 && frame == count + 1u)
		{
			rows[count].random = random;
			rows[count].vblank = vblank;
			rows[count].cycle = cycle;
			rows[count].mode = mode;
			count++;
		}
	}
	fclose(file);
	return count;
}

/* MAME's frames against the port's: lined up at the first of each on the ship select (mode $B), each frame's end
   from there in cycles, MAME's less the port's (the difference's changes tell where MAME's machine takes longer),
   and the VBLANKs from there (a frame MAME slowed down and the port didn't: MAME's a VBLANK more), up to MAME's
   VBLANK 'parted' (the games apart after it: 0, none). Written as DIR/waits_compare.txt */
static void waits_compare(const char *dir, unsigned parted)
{
	static wait_row mame[WAITS_MOST], port[WAITS_MOST];
	char path[1024];
	unsigned mame_count, port_count, m0 = 0, p0 = 0, compared = 0, vblank_apart = 0, first_apart = 0;
	unsigned random_apart = 0, first_random = 0;
	long long first_delta = 0, last_delta = 0, low = 0, high = 0;
	FILE *out;
	struct { long long change; unsigned frame; } jumps[12] = { { 0, 0 } };
	int jump_count = 0;

	snprintf(path, sizeof(path), "%s/waits.txt", dir);
	if (!(mame_count = waits_read(path, mame)))
	{
		printf("%s: none (dp_check.lua 0.2.0 or later writes it)\n", path);
		return;
	}
	snprintf(path, sizeof(path), "%s/port_waits.txt", dir);
	port_count = waits_read(path, port);
	while (m0 < mame_count && mame[m0].mode != 0xBu)
		m0++;
	while (p0 < port_count && port[p0].mode != 0xBu)
		p0++;
	if (m0 >= mame_count || p0 >= port_count)
	{
		printf("waits: no ship select in %s\n", m0 >= mame_count ? "MAME's" : "the port's");
		return;
	}
	snprintf(path, sizeof(path), "%s/waits_compare.txt", dir);
	out = fopen(path, "w");
	if (out)
		fprintf(out, "; frame from the ship select, MAME's VBLANK, the port's, the end's difference (MAME's less the "
			"port's, cycles from the first), its change from the frame before, the mode\n");
	printf("frames lined up at the ship select: MAME's frame %u (VBLANK %u), the port's %u (VBLANK %u): MAME's "
		"%+d frames and %+d VBLANKs from the power on\n", m0 + 1u, mame[m0].vblank, p0 + 1u, port[p0].vblank,
		(int)m0 - (int)p0, (int)mame[m0].vblank - (int)port[p0].vblank);
	for (unsigned k = 0; m0 + k < mame_count && p0 + k < port_count; k++)
	{
		const wait_row *const a = &mame[m0 + k], *const b = &port[p0 + k];
		const long long delta = (long long)(a->cycle - mame[m0].cycle) - (long long)(b->cycle - port[p0].cycle);
		const long long change = k ? delta - last_delta : 0;
		const int apart = (int)(a->vblank - mame[m0].vblank) - (int)(b->vblank - port[p0].vblank);

		if (parted && a->vblank >= parted)
			break;
		compared++;
		if (out)
			fprintf(out, "%u %u %u %lld %+lld %x%s\n", k, a->vblank, b->vblank, delta, change, a->mode,
				apart ? " <<" : "");
		if (apart && !vblank_apart++)
			first_apart = k;
		if (a->random < 256u && b->random < 256u && a->random != b->random && !random_apart++)
			first_random = k;
		if (!k)
			first_delta = low = high = delta;
		low = delta < low ? delta : low;
		high = delta > high ? delta : high;
		last_delta = delta;
		/* (the 12 largest changes) */
		{
			const long long size = change < 0 ? -change : change;
			const bool full = jump_count == 12;
			int at = full ? 11 : jump_count++;

			if (full && size <= (jumps[11].change < 0 ? -jumps[11].change : jumps[11].change))
				continue;
			jumps[at].change = change;
			jumps[at].frame = k;
			while (at > 0 && (jumps[at].change < 0 ? -jumps[at].change : jumps[at].change)
				> (jumps[at - 1].change < 0 ? -jumps[at - 1].change : jumps[at - 1].change))
			{
				const long long c = jumps[at].change;
				const unsigned f = jumps[at].frame;

				jumps[at] = jumps[at - 1];
				jumps[at - 1].change = c;
				jumps[at - 1].frame = f;
				at--;
			}
		}
	}
	if (out)
		fclose(out);
	printf("frames compared: %u%s; the ends' difference (MAME's less the port's, cycles): %lld at the last, from %lld "
		"to %lld\n", compared, parted ? " (to where the games part)" : "", last_delta - first_delta, low - first_delta,
		high - first_delta);
	if (vblank_apart)
		printf("at a different VBLANK: %u frames, the first %u from the ship select (MAME's VBLANK %u, the port's %u)\n",
			vblank_apart, first_apart, mame[m0 + first_apart].vblank, port[p0 + first_apart].vblank);
	else
		printf("every frame at the same VBLANK: the same frames slowed down\n");
	if (random_apart)
		printf("the random index (its wait's rounds) different: %u frames, the first %u from the ship select (MAME's "
			"VBLANK %u: %u, the port's %u)\n", random_apart, first_random, mame[m0 + first_random].vblank,
			mame[m0 + first_random].random, port[p0 + first_random].random);
	else if (mame[m0].random < 256u)
		printf("the random index the same at every frame\n");
	printf("the largest changes from one frame to the next:");
	for (int j = 0; j < jump_count; j++)
		printf(" %u:%+lld", jumps[j].frame, jumps[j].change);
	printf("\n%s/waits_compare.txt written\n", dir);
}

/* ---- MAKE=1 ---- */

static void write_video(const char *dir, unsigned n)
{
	static uint32_t picture[VIDEO_WIDTH * VIDEO_HEIGHT];
	char path[1024];
	FILE *file;

	snprintf(path, sizeof(path), "%s/video_%u.bin", dir, n);
	if (!(file = fopen(path, "wb")))
		return;
	fwrite(magic, 1, sizeof(magic), file);
	for (int part = 0; part < PARTS; part++)
	{
		if (part == VIDEO_REGS_PART)
			for (uint32_t word = 0; word < parts[part].size / 2u; word++)
			{
				/* (the sprite bank turned over, as MAME's are kept: after its interrupt has turned it) */
				const uint16_t value = word >= CAVE_VIDEO_REG_WORDS ? 0u
					: word == 4u ? cave_board.video_regs[word] ^ 1u : cave_board.video_regs[word];

				fputc(value >> 8, file);
				fputc(value & 255, file);
			}
		else
			for (uint32_t offset = 0; offset + 1u < parts[part].size; offset += 2u)
			{
				const uint16_t value = cave_read16(parts[part].address + offset, 0xFFFFu);

				fputc(value >> 8, file);
				fputc(value & 255, file);
			}
	}
	fclose(file);
	video_latch_sprites();
	video_draw(picture);
	snprintf(path, sizeof(path), "%s/screen_%u.bin", dir, n);
	if ((file = fopen(path, "wb")) != NULL)
	{
		for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
		{
			fputc(picture[i] & 255, file);
			fputc(picture[i] >> 8 & 255, file);
			fputc(picture[i] >> 16 & 255, file);
			fputc(255, file);
		}
		fclose(file);
	}
}

static void wav_close(FILE *wav, uint32_t bytes)
{
	const uint32_t riff = bytes + 36u;

	fseek(wav, 4, SEEK_SET);
	fwrite(&riff, 4, 1, wav);
	fseek(wav, 40, SEEK_SET);
	fwrite(&bytes, 4, 1, wav);
	fclose(wav);
}

static const uint8_t wav_header[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0,
	0, 1, 0, 1, 0, 0x80, 0xBB, 0, 0, 0, 0x77, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0 };

static int make(const char *dir)
{
	const unsigned frames = getenv("DP_FRAMES") ? (unsigned)atoi(getenv("DP_FRAMES")) : 30000u;
	const unsigned every = getenv("DP_VIDEO_EVERY") ? (unsigned)atoi(getenv("DP_VIDEO_EVERY")) : 2500u;
	unsigned written = 0;
	static int16_t out[16384];
	uint32_t bytes = 0;
	char path[1024];
	FILE *log, *wav;

	snprintf(path, sizeof(path), "%s/log.txt", dir);
	log = fopen(path, "w");
	snprintf(path, sizeof(path), "%s/sound.wav", dir);
	wav = fopen(path, "wb");
	if (!log || !wav)
		return 1;
	fwrite(wav_header, 1, 44, wav);
	waits_begin(dir, "waits.txt");
	start_game();
	cave_sound_drop();
	for (unsigned vblank = 1; vblank <= frames; vblank++)
	{
		unsigned held, taken;

		program_run_frame();
		waits_arrived = false;
		taken = cave_sound_take(out, sizeof(out) / sizeof(out[0]));
		fwrite(out, 2, taken, wav);
		bytes += 2u * taken;
		held = autoplayer((long)vblank, 1);    /* (it holds the shield on in play, as dp_check.lua) */
		fprintf(log, "%u %04x %08x %x\n", vblank, held, signature(), ram_word(0x1018ECu));
		cave_board.in0 = (uint16_t)~held;
		if (every && (vblank % every == 0 || vblank % every == every - 1u))
		{
			write_video(dir, vblank);
			written++;
		}
	}
	fclose(log);
	waits_end();
	wav_close(wav, bytes);
	printf("%u video states, log.txt, waits.txt and sound.wav written in %s, %u VBLANKs\n", written, dir, frames);
	return 0;
}

/* ---- SOUND=1: the port played as MAME was, its sound written ---- */

/* MAME's script gives the inputs in its frame notifier: at the screen's own VBLANK, 90 us after the vblank
   interrupt (cave.cpp's m_time_vblank_irq), when the interrupt's handler has read the last ones. What the program
   reads later in the frame sees the new ones there. The port's tools give theirs between frames, at the interrupt;
   with at_screen the replay gives each where MAME did (program_at_screen_vblank) */
static uint16_t replay_pending;
static bool replay_at_screen;

static void replay_screen_vblank(void)
{
	cave_board.in0 = replay_pending;
}

static wait_row mame_waits[WAITS_MOST];
static unsigned mame_waits_count;

/* the board's IN0 (active low) for MAME's VBLANK m: the inputs logged 'delay' VBLANKs before, the coin 'coin' more */
static uint16_t replay_in0(const uint16_t *inputs, unsigned count, long m, int delay, int coin)
{
	const long given = m - delay, coin_given = given - coin;
	unsigned held = given >= 0 && (unsigned long)given < count ? inputs[given] & ~(unsigned)CAVE_COIN : 0u;

	if (coin_given >= 0 && (unsigned long)coin_given < count)
		held |= inputs[coin_given] & CAVE_COIN;
	return (uint16_t)~held;
}

static void replay_give(uint16_t in0)
{
	if (replay_at_screen)
		replay_pending = in0;
	else
		cave_board.in0 = in0;
}

#define LOG_MOST 400000u
#define PART_RUN 1500u    /* VBLANKs without the same signature: the games parted */

static int sound(const char *dir)
{
	static uint16_t inputs[LOG_MOST];
	static uint32_t sigs[LOG_MOST];
	static int16_t out[16384];
	unsigned count = 0, parted = 0, matched = 0;
	uint32_t bytes = 0;
	char path[1024], line[256];
	FILE *log, *wav;
	int offset = 0, delay = 0, coin = 0, best = -1;
	unsigned last_same = 0;
	unsigned until = getenv("SOUND_UNTIL") ? (unsigned)atoi(getenv("SOUND_UNTIL")) : ~0u;
	const unsigned after = getenv("SOUND_AFTER") ? (unsigned)atoi(getenv("SOUND_AFTER")) : 3000u;

	snprintf(path, sizeof(path), "%s/log.txt", dir);
	if (!(log = fopen(path, "r")))
	{
		printf("%s: none\n", path);
		return 1;
	}
	while (fgets(line, sizeof(line), log) && count < LOG_MOST)
	{
		unsigned vblank, held, sig, mode;

		if (sscanf(line, "%u %x %x %x", &vblank, &held, &sig, &mode) == 4 && vblank == count + 1u)
		{
			inputs[count] = (uint16_t)held;
			sigs[count] = sig;
			count++;
		}
	}
	fclose(log);
	printf("%u VBLANKs in MAME's log\n", count);
	/* MAME's frames' ends (dp_check.lua 0.2.0 on), for the trials */
	snprintf(path, sizeof(path), "%s/waits.txt", dir);
	mame_waits_count = waits_read(path, mame_waits);
	{
		/* How MAME was given its inputs: the delay (0-3 VBLANKs after dp_check.lua logs them), the coin's own more
		   (0-3: MAME 0.289 reads the coin 2 VBLANKs later than the controls), and whether they come at the screen's
		   VBLANK (as MAME's notifier) or between frames. With MAME's waits.txt: the way that keeps the port's frames
		   ending as MAME's longest (each at the same VBLANK, within 150 cycles of the first's difference), the VBLANKs
		   counted alike from the power on; without it, the way and the shift (-3 to 3) that keep the signature
		   longest. Up to 30,000 VBLANKs; a tie to the earlier tried. Tried in a child process: the board's sound keeps
		   a little of its state from one start to the next (its resampler and filter), and the sound written must be
		   from the program's one start, as the port's own */
		int link[2];
		pid_t child;

		if (getenv("SOUND_SHIFT"))
			goto shift_given;
		if (pipe(link) != 0 || (child = fork()) < 0)
			return 1;
		if (child == 0)
		{
			int found[5] = { 0, 0, 0, 0, -1 };    /* shift, delay, coin, screen, agree */
			const bool by_waits = mame_waits_count > 0;
			const int shift_most = by_waits ? 0 : 3, delay_most = by_waits ? 3 : 2, coin_most = by_waits ? 3 : 0;

			close(link[0]);
			for (int try_screen = 0; try_screen <= 1; try_screen++)
			for (int try_coin = 0; try_coin <= coin_most; try_coin++)
			for (int try_delay = 0; try_delay <= delay_most; try_delay++)
				for (int shift = 0; shift <= shift_most; shift = shift > 0 ? -shift : 1 - shift)
				{
					int agree = 0;
					unsigned checked = 0;
					long long first = 0;
					bool stop = false;

					replay_at_screen = try_screen != 0;
					program_at_screen_vblank = replay_at_screen ? replay_screen_vblank : NULL;
					waits_frames = 0;
					waits_arrived = false;
					program_at_wait = by_waits ? waits_at : NULL;
					start_game();
					replay_pending = cave_board.in0;
					for (unsigned v = 0; v < 30000u && v < count && !stop; v++)
					{
						const long m = (long)v + shift;

						program_run_frame();
						waits_arrived = false;
						cave_sound_drop();
						if (ram_word(0x1018ECu) == 7u)
							*cave_work_ram_byte(0x102196u) = 2;
						replay_give(replay_in0(inputs, count, m, try_delay, try_coin));
						if (by_waits)
							for (; checked < waits_frames && checked < WAITS_KEPT; checked++)
							{
								const long long apart = checked < mame_waits_count
									? (long long)(mame_waits[checked].cycle - waits_cycle[checked]) : 0;

								if (!checked)
									first = apart;
								if (checked >= mame_waits_count || mame_waits[checked].vblank != waits_vblank[checked]
									|| apart - first > 150 || apart - first < -150)
								{
									stop = true;
									break;
								}
								agree++;
							}
						else if (m >= 0 && (unsigned long)m < count && signature() == sigs[m])
							agree++;
						else if (m >= 0)
							break;
					}
					if (agree > found[4])
					{
						found[0] = shift;
						found[1] = try_delay;
						found[2] = try_coin;
						found[3] = try_screen;
						found[4] = agree;
					}
					if (shift == -shift_most)
						break;
				}
			program_at_wait = NULL;
			if (write(link[1], found, sizeof(found)) != (ssize_t)sizeof(found))
				_exit(1);
			_exit(0);
		}
		close(link[1]);
		{
			int answer[5] = { 0, 0, 0, 0, -1 };

			if (read(link[0], answer, sizeof(answer)) != (ssize_t)sizeof(answer))
				printf("the trials gave no answer: none taken\n");
			close(link[0]);
			waitpid(child, NULL, 0);
			offset = answer[0];
			delay = answer[1];
			coin = answer[2];
			replay_at_screen = answer[3] != 0;
			best = answer[4];
		}
		printf("MAME's VBLANK = the port's %+d; its inputs read %d VBLANK%s after dp_check.lua logs them, the coin %d "
			"more, given %s (%s kept for %d %s from the start)\n", offset, delay, delay == 1 ? "" : "s", coin,
			replay_at_screen ? "at the screen's VBLANK" : "between frames",
			mame_waits_count ? "the frames' ends" : "the signature", best, mame_waits_count ? "frames" : "VBLANKs");
		if (0)
		{
shift_given:
			offset = atoi(getenv("SOUND_SHIFT"));
			delay = getenv("SOUND_DELAY") ? atoi(getenv("SOUND_DELAY")) : 0;
			coin = getenv("SOUND_COIN") ? atoi(getenv("SOUND_COIN")) : 0;
			replay_at_screen = getenv("SOUND_SCREEN") && atoi(getenv("SOUND_SCREEN"));
			printf("MAME's VBLANK = the port's %+d; its inputs read %d VBLANKs after they are logged, the coin %d more, "
				"given %s (SOUND_SHIFT, SOUND_DELAY, SOUND_COIN, SOUND_SCREEN)\n", offset, delay, coin,
				replay_at_screen ? "at the screen's VBLANK" : "between frames");
		}
	}
	snprintf(path, sizeof(path), "%s/port.wav", dir);
	if (!(wav = fopen(path, "wb")))
		return 1;
	fwrite(wav_header, 1, 44, wav);
	waits_begin(dir, "port_waits.txt");
	program_at_screen_vblank = replay_at_screen ? replay_screen_vblank : NULL;
	start_game();
	replay_pending = cave_board.in0;
	cave_sound_drop();
	for (unsigned v = 0; v + (unsigned)(offset > 0 ? offset : 0) < count && v < until; v++)
	{
		const long m = (long)v + offset;
		unsigned taken;

		program_run_frame();
		waits_arrived = false;
		taken = cave_sound_take(out, sizeof(out) / sizeof(out[0]));
		fwrite(out, 2, taken, wav);
		bytes += 2u * taken;
		if (ram_word(0x1018ECu) == 7u)
			*cave_work_ram_byte(0x102196u) = 2;    /* (the shield, as dp_check.lua holds it) */
		replay_give(replay_in0(inputs, count, m, delay, coin));
		if (m >= 0 && (unsigned long)m < count && !parted)
		{
			/* (a VBLANK or a few apart now and then is no parting: on a slowed VBLANK MAME's signature is read half
			   way through the frame's work, the port's after it; the games have parted when the signature has not
			   been the same for PART_RUN VBLANKs, from the last that was) */
			if (signature() == sigs[m])
			{
				matched++;
				last_same = v;
			}
			else if (v > 380u && v - last_same >= PART_RUN)
			{
				parted = (unsigned)(last_same + offset) + 2u;
				if (last_same + after < until)
					until = last_same + after;
				printf("the games part at MAME's VBLANK %u (the port's %u): the sound after it is of two games\n",
					parted, last_same + 2u);
			}
		}
	}
	waits_end();
	program_at_screen_vblank = NULL;
	wav_close(wav, bytes);
	printf("%s: %u samples (%.1f minutes)%s\n", path, bytes / 2u, bytes / 2.0 / 48000.0 / 60.0,
		parted ? "" : "; the games together all through");
	if (parted)
		printf("PARTED %u\n", parted);
	waits_compare(dir, parted);
	return 0;
}

/* ---- the pictures ---- */

static int by_number(const void *a, const void *b)
{
	const unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;

	return x < y ? -1 : x > y;
}

static void write_ppm(const char *path, const uint32_t *pixels, int width, int height)
{
	save_ppm(path, pixels, width, height);
}

int main(int argc, char **argv)
{
	const char *const dir = argc > 1 ? argv[1] : "build/mame_dp";
	static uint32_t mame[VIDEO_WIDTH * VIDEO_HEIGHT], port[VIDEO_WIDTH * VIDEO_HEIGHT], best[VIDEO_WIDTH * VIDEO_HEIGHT];
	static uint32_t diff[VIDEO_WIDTH * VIDEO_HEIGHT];
	unsigned *points = NULL, count = 0, same = 0, differ = 0, kept = 0, skipped = 0;
	DIR *folder;
	struct dirent *entry;

	if (getenv("MAKE"))
		return make(dir);
	if (getenv("SOUND"))
		return sound(dir);
	if (!(folder = opendir(dir)))
	{
		printf("%s: no such folder\n", dir);
		return 1;
	}
	while ((entry = readdir(folder)) != NULL)
	{
		unsigned n;
		char tail[8];

		if (sscanf(entry->d_name, "video_%u.%7s", &n, tail) == 2 && !strcmp(tail, "bin"))
		{
			unsigned *const more = realloc(points, sizeof(unsigned) * (count + 1u));

			if (!more)
				break;
			points = more;
			points[count++] = n;
		}
	}
	closedir(folder);
	if (!count)
	{
		printf("%s: no video_N.bin (dp_check.lua writes them)\n", dir);
		return 1;
	}
	qsort(points, count, sizeof(unsigned), by_number);
	start_game();
	printf("%u video states in %s\n", count, dir);
	for (unsigned at = 0; at < count; at++)
	{
		const unsigned n = points[at];
		char path[1024];
		size_t size_now = 0, size_before = 0;
		uint8_t *now, *before = NULL;
		unsigned least = ~0u;
		const char *how = "";

		if (at + 1u < count && points[at + 1u] == n + 1u)
			continue;    /* (N-1: drawn as N's "before") */
		snprintf(path, sizeof(path), "%s/video_%u.bin", dir, n);
		now = read_file(path, &size_now);
		if (!now || size_now != video_size() || memcmp(now, magic, sizeof(magic)))
		{
			printf("  %u: not a video state (%zu bytes)\n", n, size_now);
			free(now);
			skipped++;
			continue;
		}
		if (n > 0)
		{
			snprintf(path, sizeof(path), "%s/video_%u.bin", dir, n - 1u);
			before = read_file(path, &size_before);
			if (before && (size_before != video_size() || memcmp(before, magic, sizeof(magic))))
			{
				free(before);
				before = NULL;
			}
		}
		if (!mame_picture(dir, n, mame))
		{
			printf("  %u: no picture of MAME's (or not %dx%d)\n", n, VIDEO_WIDTH, VIDEO_HEIGHT);
			free(now);
			free(before);
			skipped++;
			continue;
		}
		{
			const uint8_t *const sources[2] = { before ? before : now, now };
			static const char *const names[8] = { "N-1's memories, the other sprite bank", "N-1's memories",
				"N-1's sprites, N's rest", "N-1's sprites (the other bank), N's rest", "N's sprites, N-1's rest",
				"N's sprites (the other bank), N-1's rest", "N's memories", "N's memories, the other sprite bank" };
			static const int sprites_of[8] = { 0, 0, 0, 0, 1, 1, 1, 1 }, rest_of[8] = { 0, 0, 1, 1, 0, 0, 1, 1 };
			static const bool other[8] = { true, false, false, true, false, true, false, true };

			for (int way = before ? 0 : 6; way < 8; way++)
			{
				unsigned now_differ;

				draw_from(sources[sprites_of[way]], sources[rest_of[way]], other[way], port);
				now_differ = differing(port, mame);
				if (now_differ < least)
				{
					least = now_differ;
					how = names[way];
					memcpy(best, port, sizeof(best));
				}
			}
		}
		{
			/* the area, from the work RAM kept with it ($101A40 the stage, $101A42 the loop) */
			const uint8_t *const ram = now + video_size() - parts[WORK_RAM_PART].size;
			const unsigned stage = (unsigned)(ram[0x1A40] << 8 | ram[0x1A41]);
			const unsigned loop = (unsigned)(ram[0x1A42] << 8 | ram[0x1A43]);
			char where[16];

			if (stage < 5u && loop < 2u)
				snprintf(where, sizeof(where), "area %u", loop * 5u + stage + 1u);
			else
				snprintf(where, sizeof(where), "-");
			if (least == 0)
			{
				same++;
				printf("  %u (%s): the same (%s)\n", n, where, how);
			}
			else
			{
				differ++;
				printf("  %u (%s): %u pixels differ (nearest: %s)\n", n, where, least, how);
				if (kept < 5u)
				{
					kept++;
					for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
						diff[i] = mame[i] != (best[i] & 0xFFFFFFu) ? 0xFFFFFFu : (mame[i] >> 2 & 0x3F3F3Fu);
					snprintf(path, sizeof(path), "mame_%u.ppm", n);
					write_ppm(path, mame, VIDEO_WIDTH, VIDEO_HEIGHT);
					snprintf(path, sizeof(path), "port_%u.ppm", n);
					write_ppm(path, best, VIDEO_WIDTH, VIDEO_HEIGHT);
					snprintf(path, sizeof(path), "diff_%u.ppm", n);
					write_ppm(path, diff, VIDEO_WIDTH, VIDEO_HEIGHT);
				}
			}
		}
		free(now);
		free(before);
	}
	printf("\n%u pictures the same, %u differ%s\n", same, differ, skipped ? " (some not compared: above)" : "");
	free(points);
	return differ ? 1 : 0;
}
