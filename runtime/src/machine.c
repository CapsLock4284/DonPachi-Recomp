/**
 *    Name:        machine.c
 *    Description: DonPachi's board: the 68000's bus (cave/machine.h's map), the interrupt and its cause, the
 *                 inputs and the 93C46 EEPROM
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After MAME's src/mame/cave/cave.cpp (BSD-3-Clause): donpachi_map, donpachi_videoregs_r, irq_cause_r,
 *    update_irq_state, eeprom_w; the EEPROM after src/devices/machine/eepromser.cpp (93C46, 64 words of 16 bits).
 *    The sound chips: cave/sound.h (sound.c). B00000-B00003, B00010-B00013 and B00020-B0002F take the word's low byte
 *    (MAME's umask16(0x00ff)).
 */
#include "cave/machine.h"

#include <stdio.h>
#include <string.h>

cave_machine cave_board;

/* ---- the EEPROM: a 93C46 in its 16-bit mode, its lines from D00000 (bit 3 DI, bit 1 CS, bit 2 CLK) ---- */

enum
{
	EEPROM_IDLE,          /* CS low, or the command done */
	EEPROM_START,         /* waiting for the start bit */
	EEPROM_COMMAND,       /* the opcode's 2 bits and the address's 6 */
	EEPROM_READ,          /* shifting a word out */
	EEPROM_WRITE_DATA,    /* shifting a word in (WRITE, WRAL) */
	EEPROM_DONE           /* a write or erase waiting for CS to fall */
};

enum { OP_READ = 2, OP_WRITE = 1, OP_ERASE = 3, OP_EXTENDED = 0 };

static int eeprom_pending_op;     /* the write or erase CS's fall carries out: 1 WRITE, 3 ERASE, 4 ERAL, 5 WRAL */
static uint16_t eeprom_data;

static void eeprom_clock_rising(cave_eeprom *e)
{
	switch (e->state)
	{
	case EEPROM_START:
		if (e->di)
		{
			e->state = EEPROM_COMMAND;
			e->shift = 0;
			e->bits = 0;
		}
		break;
	case EEPROM_COMMAND:
		e->shift = e->shift << 1 | (e->di ? 1u : 0u);
		if (++e->bits == 8)
		{
			const int op = (int)(e->shift >> 6 & 3u);

			e->address = (int)(e->shift & 0x3Fu);
			e->bits = 0;
			e->shift = 0;
			if (op == OP_READ)
			{
				e->state = EEPROM_READ;
				e->out = false;          /* the dummy 0 before the data */
				eeprom_data = e->words[e->address];
			}
			else if (op == OP_WRITE)
			{
				e->state = EEPROM_WRITE_DATA;
				eeprom_pending_op = 1;
			}
			else if (op == OP_ERASE)
			{
				e->state = EEPROM_DONE;
				eeprom_pending_op = 3;
			}
			else
				switch (e->address >> 4)
				{
				case 3: e->write_enabled = true; e->state = EEPROM_IDLE; break;     /* EWEN */
				case 0: e->write_enabled = false; e->state = EEPROM_IDLE; break;    /* EWDS */
				case 2: e->state = EEPROM_DONE; eeprom_pending_op = 4; break;       /* ERAL */
				default: e->state = EEPROM_WRITE_DATA; eeprom_pending_op = 5; break; /* WRAL */
				}
		}
		break;
	case EEPROM_READ:
		e->out = (eeprom_data & 0x8000u) != 0;
		eeprom_data = (uint16_t)(eeprom_data << 1);
		if (++e->bits == 16)
		{
			/* (a longer read goes on with the next word) */
			e->bits = 0;
			e->address = (e->address + 1) & (int)(CAVE_EEPROM_WORDS - 1u);
			eeprom_data = e->words[e->address];
		}
		break;
	case EEPROM_WRITE_DATA:
		e->shift = e->shift << 1 | (e->di ? 1u : 0u);
		if (++e->bits == 16)
			e->state = EEPROM_DONE;
		break;
	default:
		break;
	}
}

static void eeprom_select_falling(cave_eeprom *e)
{
	if (e->state == EEPROM_DONE && e->write_enabled)
		switch (eeprom_pending_op)
		{
		case 1: e->words[e->address] = (uint16_t)e->shift; break;
		case 3: e->words[e->address] = 0xFFFFu; break;
		case 4: for (int i = 0; i < (int)CAVE_EEPROM_WORDS; i++) e->words[i] = 0xFFFFu; break;
		case 5: for (int i = 0; i < (int)CAVE_EEPROM_WORDS; i++) e->words[i] = (uint16_t)e->shift; break;
		default: break;
		}
	eeprom_pending_op = 0;
	e->state = EEPROM_IDLE;
	e->out = true;    /* ready */
}

static void eeprom_write_lines(uint8_t data)
{
	cave_eeprom *const e = &cave_board.eeprom;
	const bool di = (data & 0x08u) != 0, cs = (data & 0x02u) != 0, clk = (data & 0x04u) != 0;

	e->di = di;
	if (cs && !e->cs)
	{
		e->state = EEPROM_START;
		e->bits = 0;
	}
	else if (!cs && e->cs)
		eeprom_select_falling(e);
	e->cs = cs;
	if (cs && clk && !e->clk)
		eeprom_clock_rising(e);
	e->clk = clk;
}

/* ---- the interrupt ---- */

int cave_irq_level(void)
{
	return (cave_board.vblank_irq || cave_board.unknown_irq) ? 1 : 0;
}

void cave_vblank_start(void)
{
	cave_board.vblank_irq = true;
	cave_board.frame++;
	cave_sound_frame();
}

/* 900000-900007: the cause, bit 0 low for VBLANK, bit 1 low for the other; reading 900004 clears VBLANK's,
   900006 the other's (irq_cause_r) */
static uint16_t irq_cause(uint32_t word)
{
	uint16_t result = 0x0003u;

	if (cave_board.vblank_irq)
		result ^= 0x01u;
	if (cave_board.unknown_irq)
		result ^= 0x02u;
	if (word == 2)
		cave_board.vblank_irq = false;
	if (word == 3)
		cave_board.unknown_irq = false;
	if (word == 0)
		result |= 4u;    /* (bit 2: agallet's, idle here) */
	return result;
}

/* ---- the bus ---- */

static void combine(uint16_t *word, uint16_t data, uint16_t mem_mask)
{
	*word = (uint16_t)((*word & ~mem_mask) | (data & mem_mask));
}

uint16_t cave_read16(uint32_t address, uint16_t mem_mask)
{
	(void)mem_mask;
	address &= 0xFFFFFEu;
	if (address < CAVE_PROGRAM_SIZE)
		return cave_board.program ? (uint16_t)(cave_board.program[address] << 8 | cave_board.program[address + 1u]) : 0u;
	if (address >= CAVE_WORK_RAM && address < CAVE_WORK_RAM + CAVE_WORK_RAM_SIZE)
		return *(uint16_t *)cave_work_ram_at(address, 2u);
	if (address >= 0x200000u && address < 0x208000u)
		return cave_board.layer_ram[1][(address - 0x200000u) >> 1];
	if (address >= 0x300000u && address < 0x308000u)
		return cave_board.layer_ram[0][(address - 0x300000u) >> 1];
	if (address >= 0x400000u && address < 0x408000u)
		return cave_board.layer_ram[2][0x2000u + ((address & 0x3FFFu) >> 1)];
	if (address >= 0x500000u && address < 0x510000u)
		return cave_board.sprite_ram[(address - 0x500000u) >> 1];
	if (address >= 0x600000u && address < 0x600006u)
		return cave_board.layer_regs[1][(address - 0x600000u) >> 1];
	if (address >= 0x700000u && address < 0x700006u)
		return cave_board.layer_regs[0][(address - 0x700000u) >> 1];
	if (address >= 0x800000u && address < 0x800006u)
		return cave_board.layer_regs[2][(address - 0x800000u) >> 1];
	if (address >= 0x900000u && address < 0x900080u)
		return address < 0x900008u ? irq_cause((address - 0x900000u) >> 1) : 0u;
	if (address >= 0xA08000u && address < 0xA09000u)
		return cave_board.palette[(address - 0xA08000u) >> 1];
	if (address >= 0xB00000u && address < 0xB00004u)
		return cave_sound_status(0);    /* M6295 #0's status: bit n, voice n playing */
	if (address >= 0xB00010u && address < 0xB00014u)
		return cave_sound_status(1);    /* M6295 #1's */
	if (address == 0xC00000u)
		return cave_board.in0;
	if (address == 0xC00002u)
		return (uint16_t)((cave_board.in1 & ~CAVE_EEPROM_DO) | (cave_board.eeprom.out ? CAVE_EEPROM_DO : 0u));
	cave_board.unmapped_reads++;
	cave_board.last_unmapped_read = address;
	return 0xFFFFu;
}

void cave_write16(uint32_t address, uint16_t data, uint16_t mem_mask)
{
	address &= 0xFFFFFEu;
	if (address >= CAVE_WORK_RAM && address < CAVE_WORK_RAM + CAVE_WORK_RAM_SIZE)
	{
		combine((uint16_t *)cave_work_ram_at(address, 2u), data, mem_mask);
		return;
	}
	if (address >= 0x200000u && address < 0x208000u)
		combine(&cave_board.layer_ram[1][(address - 0x200000u) >> 1], data, mem_mask);
	else if (address >= 0x300000u && address < 0x308000u)
		combine(&cave_board.layer_ram[0][(address - 0x300000u) >> 1], data, mem_mask);
	else if (address >= 0x400000u && address < 0x408000u)
		combine(&cave_board.layer_ram[2][0x2000u + ((address & 0x3FFFu) >> 1)], data, mem_mask);
	else if (address >= 0x500000u && address < 0x510000u)
		combine(&cave_board.sprite_ram[(address - 0x500000u) >> 1], data, mem_mask);
	else if (address >= 0x600000u && address < 0x600006u)
		combine(&cave_board.layer_regs[1][(address - 0x600000u) >> 1], data, mem_mask);
	else if (address >= 0x700000u && address < 0x700006u)
		combine(&cave_board.layer_regs[0][(address - 0x700000u) >> 1], data, mem_mask);
	else if (address >= 0x800000u && address < 0x800006u)
		combine(&cave_board.layer_regs[2][(address - 0x800000u) >> 1], data, mem_mask);
	else if (address == 0x900078u)
		;    /* the watchdog (sprite013.cpp: "offset 0x68 or 0x78 is commonly watchdog"): the interrupt writes it
		        whatever D0 holds, a value the decompiled program does not have */
	else if (address >= 0x900000u && address < 0x900080u)
		combine(&cave_board.video_regs[(address - 0x900000u) >> 1], data, mem_mask);
	else if (address >= 0xA08000u && address < 0xA09000u)
		combine(&cave_board.palette[(address - 0xA08000u) >> 1], data, mem_mask);
	else if (address >= 0xB00000u && address < 0xB00004u)
	{
		if (mem_mask & 0x00FFu)
			cave_sound_command(0, (uint8_t)data);
	}
	else if (address >= 0xB00010u && address < 0xB00014u)
	{
		if (mem_mask & 0x00FFu)
			cave_sound_command(1, (uint8_t)data);
	}
	else if (address >= 0xB00020u && address < 0xB00030u)
	{
		if (mem_mask & 0x00FFu)
			cave_sound_bank((int)((address - 0xB00020u) >> 1), (uint8_t)data);
	}
	else if (address == 0xD00000u)
	{
		/* (a byte at D00000: the word's high half) */
		const uint8_t lines = (mem_mask & 0xFF00u) ? (uint8_t)(data >> 8) : (uint8_t)data;

		cave_board.eeprom_lines = lines;
		eeprom_write_lines(lines);
	}
	else
	{
		cave_board.unmapped_writes++;    /* (the program ROM, and what no device answers) */
		cave_board.last_unmapped_write = address;
	}
}

void cave_reset(void)
{
	const uint8_t *const program = cave_board.program;
	uint16_t words[CAVE_EEPROM_WORDS];

	memcpy(words, cave_board.eeprom.words, sizeof(words));
	memset(&cave_board, 0, sizeof(cave_board));
	cave_board.program = program;
	memcpy(cave_board.eeprom.words, words, sizeof(words));
	cave_board.eeprom.out = true;
	cave_board.in0 = 0xFFFFu;
	cave_board.in1 = 0xFFFFu;
	cave_sound_reset();
}

/* ---- for the checks ---- */

static uint64_t fnv(uint64_t hash, const void *data, size_t size)
{
	const uint8_t *bytes = data;

	for (size_t index = 0; index < size; index++)
		hash = (hash ^ bytes[index]) * 0x100000001B3ull;
	return hash;
}

uint64_t cave_state_hash(void)
{
	uint64_t hash = 0xCBF29CE484222325ull;

	for (uint32_t address = CAVE_WORK_RAM; address < CAVE_STACK_FLOOR; address++)
		hash = fnv(hash, cave_work_ram_byte(address), 1u);
	hash = fnv(hash, cave_board.layer_ram, sizeof(cave_board.layer_ram));
	hash = fnv(hash, cave_board.layer_regs, sizeof(cave_board.layer_regs));
	hash = fnv(hash, cave_board.sprite_ram, sizeof(cave_board.sprite_ram));
	hash = fnv(hash, cave_board.video_regs, sizeof(cave_board.video_regs));
	hash = fnv(hash, cave_board.palette, sizeof(cave_board.palette));
	return hash;
}

const char *cave_dump(const char *path)
{
	FILE *output = fopen(path, "wb");

	if (!output)
		return "cannot write the dump";
	/* (the work RAM as the 68000 sees it, from 100000 up) */
	for (uint32_t address = CAVE_WORK_RAM; address < CAVE_WORK_RAM + CAVE_WORK_RAM_SIZE; address++)
		fputc(*cave_work_ram_byte(address), output);
	fwrite(cave_board.layer_ram, 1, sizeof(cave_board.layer_ram), output);
	fwrite(cave_board.layer_regs, 1, sizeof(cave_board.layer_regs), output);
	fwrite(cave_board.sprite_ram, 1, sizeof(cave_board.sprite_ram), output);
	fwrite(cave_board.video_regs, 1, sizeof(cave_board.video_regs), output);
	fwrite(cave_board.palette, 1, sizeof(cave_board.palette), output);
	fclose(output);
	return NULL;
}
