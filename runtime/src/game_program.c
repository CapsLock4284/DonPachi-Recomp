/**
 *    Name:        game_program.c
 *    Description: the decompiled program's runtime: its data read from the program ROM, the tables it loads by
 *                 name, the status register and the interrupt, and its frames, a frame at a time for the port
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After tmntarcade-decomp's runtime/src/game_program.c (BSD-3-Clause, Sebastian Toyos:
 *    runtime/LICENSE.tmnt-runtime). DonPachi reads its data from the ROM set's program region, as the board does,
 *    not from extracted assets.
 */
#include "game/support.h"

#include "cave/machine.h"
#include "cave/program.h"
#include "cave/rom_set.h"
#include "host/tasks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR_MASK      0xA700u    /* the system byte: trace, supervisor, the interrupt mask */
#define SR_S         0x2000u
#define MAX_DEPTH    16
#define MAX_TABLE    0x10000u   /* the bytes of a table's copy, at most */

static uint32_t status = 0x2700u;
static uint32_t saved[MAX_DEPTH];
static size_t depth;
static uint32_t interrupts;
static host_task *port_task, *game_task;

/* ---- stopping, with where ---- */

static GAME_NORETURN void stopped(const char *what, uint32_t address)
{
	fprintf(stderr, "68000 stopped: %s %06X (frame %llu)\n", what, (unsigned)address & 0xFFFFFFu,
		(unsigned long long)cave_board.frame);
	exit(3);
}

void invalid_jump(uint32_t address)
{
	stopped("a jump no case of its switch has, to", address);
}

void lost_stack(uint32_t address)
{
	stopped("the code keeps a stack the C does not have, at", address);
}

void game_trace(uint32_t address)
{
	(void)address;
}

void game_trace_block(uint32_t address)
{
	(void)address;
}

/* ---- the program's data: the program ROM, or the work RAM where the program set a pointer there ---- */

const uint8_t *data_at(uint32_t address)
{
	address &= 0xFFFFFFu;
	if (address < DONPACHI_PROGRAM_SIZE)
		return &rom_set.program[address];
	if (address >= CAVE_WORK_RAM && address < CAVE_WORK_RAM + CAVE_WORK_RAM_SIZE)
		return cave_work_ram_byte(address);
	stopped("the program read data where it has none, at", address);
}

/* A record of the program's data, its bytes the other way round (game/support.h) */
#define RECORD_COPIES 32
#define RECORD_MAX    64u

const void *data_record(uint32_t address, uint32_t size)
{
	static uint8_t copies[RECORD_COPIES][RECORD_MAX];
	static unsigned next;
	uint8_t *copy;

	address &= 0xFFFFFFu;
	if (address >= CAVE_WORK_RAM && address + size <= CAVE_WORK_RAM + CAVE_WORK_RAM_SIZE)
		return cave_work_ram_at(address, size);
	if (size > RECORD_MAX || address + size > DONPACHI_PROGRAM_SIZE)
		stopped("the program read a record it cannot have at", address);
	copy = copies[next++ % RECORD_COPIES];
	for (uint32_t at = 0; at < size; at++)
		copy[size - 1u - at] = rom_set.program[address + at];
	return copy;
}

/* Every table of the registry (src/table.c): a copy from its address on, each element in the host's order */
static const char *load_tables(void)
{
	static char problem[160];

	for (size_t index = 0; index < game_table_count; index++)
	{
		const game_table *table = &game_tables[index];
		uint32_t length = DONPACHI_PROGRAM_SIZE - (table->address & 0xFFFFFFu);
		uint8_t *copy;

		if ((table->address & 0xFFFFFFu) >= DONPACHI_PROGRAM_SIZE)
		{
			snprintf(problem, sizeof(problem), "the program's table %s is outside its ROM", table->name);
			return problem;
		}
		if (length > MAX_TABLE)
			length = MAX_TABLE;
		copy = calloc(1, length + 4u);
		if (!copy)
			return "no memory for the program's tables";
		memcpy(copy, rom_set.program + (table->address & 0xFFFFFFu), length);
		if (table->element > 1)
			for (uint32_t first = 0; first + table->element <= length; first += table->element)
				for (uint32_t low = first, high = first + table->element - 1u; low < high; low++, high--)
				{
					const uint8_t byte = copy[low];

					copy[low] = copy[high];
					copy[high] = byte;
				}
		*table->data = copy;
	}
	return NULL;
}

/* ---- the code the board runs by address (the vectors) ---- */

static void (*code_at(uint32_t address))(void)
{
	size_t low = 0, high = game_code_count;

	address &= 0xFFFFFFu;
	while (low < high)
	{
		const size_t middle = (low + high) / 2;

		if (game_code[middle].address < address)
			low = middle + 1;
		else
			high = middle;
	}
	if (low < game_code_count && game_code[low].address == address)
		return game_code[low].function;
	stopped("no code of the program starts at", address);
}

/* ---- the status register and the interrupt ---- */

void write_status(uint32_t value)
{
	status = value & SR_MASK;
}

void return_from_exception(void)
{
	if (depth)
		status = saved[depth - 1];
}

void take_interrupts(void)
{
	for (;;)
	{
		const int level = cave_irq_level();
		void (*handler)(void);

		if (level == 0 || (uint32_t)level <= (status >> 8 & 7u))
			return;
		if (depth == MAX_DEPTH)
			stopped("too many interrupts nest, at frame end", 0);
		handler = code_at(data32(4u * (24u + (uint32_t)level)));
		saved[depth++] = status;
		status = (status & ~0x8700u) | SR_S | (uint32_t)level << 8;
		interrupts++;
		handler();
		depth--;
		status = saved[depth];
	}
}

/* ---- the frames ---- */

/* The program waits for its next frame: VBLANK starts, and the port has the frame */
static void next_frame(void)
{
	cave_vblank_start();
	host_task_switch(port_task);
}

void frame_wait(void)
{
	next_frame();
}

void stop(uint32_t value)
{
	write_status(value);
	while (cave_irq_level() == 0 || (uint32_t)cave_irq_level() <= (status >> 8 & 7u))
		next_frame();
	take_interrupts();
}

void reset_devices(void)
{
	/* (the board connects nothing to the 68000's RESET line) */
}

static void run_program(void)
{
	game_entry();
	stopped("the program came to its end, after", 0);
}

const char *program_start(void)
{
	const char *problem = load_tables();

	if (problem)
		return problem;
	cave_board.program = rom_set.program;
	memcpy(cave_board.eeprom.words, rom_set.eeprom, sizeof(cave_board.eeprom.words));
	cave_reset();
	status = 0x2700u;
	depth = 0;
	interrupts = 0;
	port_task = host_task_main();
	game_task = host_task_create(run_program);
	return NULL;
}

void program_run_frame(void)
{
	host_task_switch(game_task);
}

uint64_t program_frames(void)
{
	return cave_board.frame;
}

uint32_t program_interrupts(void)
{
	return interrupts;
}
