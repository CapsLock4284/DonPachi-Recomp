/**
 *    Name:        machine.h
 *    Description: DonPachi's board (Cave, AT-C01DP-2) as the decompiled program sees it: its memory map, the
 *                 interrupt, the inputs and the EEPROM
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After MAME's src/mame/cave/cave.cpp (donpachi_map, irq_cause_r, eeprom_w, INPUT_PORTS_START( cave );
 *    BSD-3-Clause, Luca Elia and others) and src/devices/video/tmap038.cpp (the NEC 038 tilemaps).
 *    Shaped like tmntarcade-decomp's runtime/include/tmnt/machine.h (BSD-3-Clause, Sebastian Toyos).
 *
 *    One 68000 at 16 MHz and no sound CPU: the 68000 drives the two OKI M6295s and the NMK112 itself.
 *      000000-07FFFF  program ROM          100000-10FFFF  work RAM
 *      200000-207FFF  layer 1 (038)        300000-307FFF  layer 0 (038)    400000-403FFF  layer 2, 8x8 only
 *      500000-50FFFF  sprite RAM           600000/700000/800000  layers 1, 0, 2: their 3 registers
 *      900000-90007F  video registers (900000-900007 read: the interrupt's cause)
 *      A08000-A08FFF  palette (xGRB 555)   B00000/B00010  M6295 #0, #1    B00020-B0002F  NMK112 banks
 *      C00000  inputs 1     C00002  inputs 2 + the EEPROM's data out (bit 11)     D00000  EEPROM lines
 */
#ifndef CAVE_MACHINE_H
#define CAVE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "cave/sound.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAVE_CPU_CLOCK        16000000u
#define CAVE_FRAME_USEC       17376u        /* MAME: 17376 us a frame (57.55 Hz) */

#define CAVE_PROGRAM_SIZE     0x080000u
#define CAVE_WORK_RAM         0x100000u
#define CAVE_WORK_RAM_SIZE    0x10000u
#define CAVE_LAYER_RAM_WORDS  0x4000u       /* 32 KB a layer (tmap038: 16x16 RAM, line RAM, 8x8 RAM) */
#define CAVE_SPRITE_RAM_WORDS 0x8000u       /* 64 KB */
#define CAVE_PALETTE_WORDS    0x800u        /* A08000-A08FFF */
#define CAVE_VIDEO_REG_WORDS  0x40u
#define CAVE_EEPROM_WORDS     64u           /* 93C46, 16-bit words */
#define CAVE_STACK_FLOOR      0x10F800u     /* the 68000's stack, down from 110000 (reset's SP): the decompiled
                                               program keeps its own in C, so the checks leave these 2 KB out */

/* The inputs, MAME's IN0 and IN1: active low */
enum
{
	CAVE_UP = 0x0001, CAVE_DOWN = 0x0002, CAVE_LEFT = 0x0004, CAVE_RIGHT = 0x0008,
	CAVE_BUTTON1 = 0x0010, CAVE_BUTTON2 = 0x0020, CAVE_BUTTON3 = 0x0040, CAVE_START = 0x0080,
	CAVE_COIN = 0x0100,
	CAVE_SERVICE = 0x0200,          /* IN0: the test mode; IN1: the service coin */
	CAVE_EEPROM_DO = 0x0800         /* IN1, active high: the EEPROM's data out */
};

typedef struct
{
	/* the EEPROM's state: its lines, and the bits of the command or data being shifted */
	uint16_t words[CAVE_EEPROM_WORDS];
	bool cs, clk, di, out;
	bool write_enabled;
	uint32_t shift;
	int bits;
	int state;
	int address;
} cave_eeprom;

typedef struct
{
	uint8_t work_ram[CAVE_WORK_RAM_SIZE];    /* the other way round: cave_work_ram_at */
	uint16_t layer_ram[3][CAVE_LAYER_RAM_WORDS];
	uint16_t layer_regs[3][3];
	uint16_t sprite_ram[CAVE_SPRITE_RAM_WORDS];
	uint16_t video_regs[CAVE_VIDEO_REG_WORDS];
	uint16_t palette[CAVE_PALETTE_WORDS];
	bool vblank_irq;                          /* raised when VBLANK starts, cleared by reading 900004 */
	bool unknown_irq;
	cave_sound sound;                         /* the M6295s and the NMK112 (cave/sound.h) */
	uint8_t eeprom_lines;                     /* the last byte written to D00000 */
	cave_eeprom eeprom;
	uint16_t in0, in1;                        /* active low, as the port sets them */
	/* the ROM set's regions, big endian as the 68000 sees them (rom_set.c) */
	const uint8_t *program;
	uint64_t frame;                           /* VBLANKs since reset */
	uint32_t unmapped_reads, unmapped_writes;
	uint32_t last_unmapped_read, last_unmapped_write;    /* (their addresses, for the checks) */
} cave_machine;

extern cave_machine cave_board;

/* The host's place of the 'size' bytes of work RAM from 'address' (a big-endian value there is a little-endian
   one at this place) */
static inline void *cave_work_ram_at(uint32_t address, uint32_t size)
{
	return &cave_board.work_ram[CAVE_WORK_RAM_SIZE - size - ((address - CAVE_WORK_RAM) & (CAVE_WORK_RAM_SIZE - 1u))];
}

static inline uint8_t *cave_work_ram_byte(uint32_t address)
{
	return (uint8_t *)cave_work_ram_at(address, 1u);
}

/* The board after its reset: RAMs cleared, the inputs idle, the EEPROM as the ROM set has it */
void cave_reset(void);

uint16_t cave_read16(uint32_t address, uint16_t mem_mask);
void cave_write16(uint32_t address, uint16_t data, uint16_t mem_mask);

/* The interrupt level the 68000 sees: 1 while an interrupt is pending, else 0 */
int cave_irq_level(void);

/* The screen's VBLANK begins: the interrupt, and the frame's sound (cave_sound_frame) */
void cave_vblank_start(void);

/* A hash of what the program has made of the board's memories (work RAM, layers and their registers, sprites,
   palette, video registers), for comparing the decompiled program with the original on MAME's 68000 (tools/cref) */
uint64_t cave_state_hash(void);

/* The board's memories, one region after the other, for finding where two runs differ: NULL, or what is wrong */
const char *cave_dump(const char *path);

#ifdef __cplusplus
}
#endif

#endif
