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

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
#include <malloc.h>
#define stack_alloca _alloca
#define NOINLINE __declspec(noinline)
#else
#include <alloca.h>
#define stack_alloca alloca
#define NOINLINE __attribute__((noinline))
#endif

#define SR_MASK      0xA700u    /* the system byte: trace, supervisor, the interrupt mask */
#define FRAME_CYCLES ((uint64_t)CAVE_CPU_CLOCK * CAVE_FRAME_USEC / 1000000u)    /* 278,016: a frame of the 68000 */
#define RESET_CYCLES 34u           /* the 68000's reset (its stack pointer and first PC read) before its first
                                      instruction, as MAME's core counts it */
#define WAIT_ROUND_MOST 64u        /* more than a round of the wait loop takes (DonPachi's: 46 cycles), less than any
                                      frame's work: a frame_wait sooner than this after the VBLANK's interrupt is in
                                      the wait that interrupt ended */
#define SR_S         0x2000u
#define MAX_DEPTH    16
#define MAX_TABLE    0x10000u   /* the bytes of a table's copy, at most */

static uint32_t status = 0x2700u;
static uint32_t saved[MAX_DEPTH];
static size_t depth;
static uint32_t interrupts;
static host_task *port_task, *game_task;
static jmp_buf restart;            /* (run_program's start, on the game task's own stack: program_reset) */
static bool reset_pending;
static bool started;               /* the program has begun (its first frame run): before, a reset is nothing to do */

/* the board's time (game/support.h): the 68000's cycles, and when the next VBLANK comes */
uint64_t m68k_time = RESET_CYCLES, m68k_vblank_at = FRAME_CYCLES;
uint32_t m68k_extra;    /* (the data's or a branch's cycles m68k_cycles added to the instruction m68k_block ends) */
static bool slowdown = true;       /* the port's setting: a VBLANK may come while the program works */
static bool timed;                 /* the program counts its cycles */
static uint64_t slow_frames;       /* the VBLANKs that came while it worked */
static bool lagged;                /* the last one did */
static uint64_t time_at_wait = RESET_CYCLES;    /* the time as the program last waited (another since: it counts
                                                   its cycles) */
static uint64_t last_frame_cycles; /* the cycles of the program's last frame's work: from its VBLANK to its wait */
static bool waiting;               /* in its wait loop (a timed program with slowdown: it goes round it) */
static uint64_t work_began;        /* when the frame's work began: the VBLANK that ended the wait */
static uint64_t handled_at;        /* when the VBLANK's interrupt was last done */

/* The save states (program_state_save): where the program waits for its frame, on its stack. next_frame keeps the
   registers there (frame_registers) before it hands the frame to the port, and the stack from there up to
   run_program's frame is the rest of what the program is doing: the C's locals, which hold the 68000's registers,
   and the calls it is in */
static jmp_buf frame_registers;
static uintptr_t stack_top;        /* (run_program's frame: the game task's stack is below it) */
static uintptr_t stack_bottom;     /* (next_frame's, at the frame just handed over: a little below its locals) */
static const program_state *state_pending;
#define STACK_MARGIN 512u          /* below next_frame's locals: its own frame, wherever the compiler puts it */

struct program_state
{
	cave_machine board;
	uint32_t status;
	uint32_t saved[MAX_DEPTH];
	size_t depth;
	uint32_t interrupts;
	uint64_t time, vblank_at, slow_frames, work_began, handled_at;
	bool waiting;
	jmp_buf registers;
	uintptr_t bottom, top;
	size_t stack_size;
	unsigned char stack[];         /* the game task's stack, bottom to top */
};

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

uint16_t status_system(void)
{
	return (uint16_t)status;
}

void write_status(uint32_t value)
{
	status = value & SR_MASK;
}

void return_from_exception(void)
{
	if (depth)
		status = saved[depth - 1];
}

void (*program_after_vblank)(void);

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
		if (timed)
			m68k_time += 44u;    /* (the 68000's own work as it takes an interrupt: 44 cycles) */
		handler();
		depth--;
		status = saved[depth];
		if (level == 1 && depth == 0)
			handled_at = m68k_time;    /* (a wait the program comes to straight after is the one it ended) */
		if (level == 1 && depth == 0 && program_after_vblank)
			program_after_vblank();
	}
}

/* ---- the frames ---- */

/* The program waits for its next frame: VBLANK starts, and the port has the frame. A reset asked for meanwhile
   (program_reset) starts the program again from its reset vector: back up the game task's own stack to
   run_program, past every call the program is in */
static NOINLINE void restore_state(void);
static void put_state_back(void);

static void next_frame(void);

/* The VBLANK came by an instruction's interrupt point (game/support.h's m68k_block): it comes now, at the
   instruction's end, as on the board. The port has the frame; in the middle of the program's work, the picture as it
   was (the program has not marked a new one ready), and the interrupt is taken here unless the status register
   masks it (then as the program lets it through, in take_interrupts): on the board its handler then runs
   the sound and counts the frame and the random index on, and the program, its work done, waits for the VBLANK
   after. With slowdown off, the VBLANK waits for the program instead, as the frame timing has it */
void m68k_vblank_came(void)
{
	bool was_waiting;

	timed = true;
	if (!slowdown)
	{
		m68k_vblank_at = m68k_time + FRAME_CYCLES;
		return;
	}
	m68k_vblank_at += FRAME_CYCLES;
	was_waiting = waiting;
	lagged = !was_waiting;
	if (was_waiting)
		work_began = m68k_time;    /* (the VBLANK it waited for: its next frame's work begins) */
	else
		slow_frames++;             /* (it came in the middle of the work: a frame of slowdown) */
	waiting = false;
	next_frame();
	take_interrupts();
}

static void next_frame(void)
{
	volatile unsigned char here = 0;

	cave_vblank_start();
	stack_bottom = ((uintptr_t)&here - STACK_MARGIN) & ~(uintptr_t)15;
	if (setjmp(frame_registers) == 0)
	{
		host_task_switch(port_task);
		if (state_pending)
			put_state_back();    /* (does not return: it goes on where the state was saved, in its next_frame) */
	}
	if (reset_pending)
	{
		reset_pending = false;
		longjmp(restart, 1);
	}
}

void (*program_at_wait)(void);

void frame_wait(void)
{
	if (m68k_time != time_at_wait)
		timed = true;    /* (cycles counted since the last wait: a timed program) */
	if (slowdown && timed)
	{
		/* the board's way: the program goes round its wait loop, the loop's own blocks counting its time, until
		   the VBLANK comes in it (m68k_block); every round does what the loop does (DonPachi's counts the random
		   index on: how many rounds a frame has to spare is part of its random numbers) */
		if (program_at_wait)
			program_at_wait();    /* (every round: what the program has made ready tells its frames apart) */
		if (m68k_time - handled_at < WAIT_ROUND_MOST)
			return;    /* (a round straight after the VBLANK's interrupt: the rest of the round it came in, the
			              round after it when it came after the round tested its flag, or the first round of a
			              wait it came just before, masked until the wait let it through; the frame's flag is
			              set, and the loop ends as the round tests it) */
		if (!waiting)
			last_frame_cycles = m68k_time - work_began;
		waiting = true;
		time_at_wait = m68k_time;
		return;
	}
	/* (frame timing: the time goes on to the VBLANK, unless the VBLANK came in the program's last cycles) */
	lagged = false;
	last_frame_cycles = m68k_time - time_at_wait;
	if (program_at_wait)
		program_at_wait();
	m68k_time = m68k_vblank_at;
	m68k_vblank_at += FRAME_CYCLES;
	time_at_wait = m68k_time;
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

/* The board as its reset leaves it (cave_reset: the RAMs, the video, the sound chips cleared, the EEPROM kept), the
   68000's status, and the frames counted on */
static void reset_board(void)
{
	const uint64_t frame = cave_board.frame;

	cave_reset();
	cave_board.frame = frame;
	status = 0x2700u;
	depth = 0;
	m68k_time = time_at_wait = work_began = handled_at = RESET_CYCLES;
	m68k_extra = 0;
	m68k_vblank_at = FRAME_CYCLES;
	waiting = lagged = false;
}

/* A save state put back (program_state_load), on the game task: its stack below what the state's stack takes, so
   that the copy does not write over the frames doing it, then the state's registers, as its next_frame kept them */
static void put_state_back(void)
{
	volatile unsigned char here = 0;
	const uintptr_t at = (uintptr_t)&here;
	const size_t room = (at > state_pending->bottom ? at - state_pending->bottom : 0) + 4096u;
	volatile unsigned char *below = stack_alloca(room);

	below[0] = 0;
	restore_state();
}

static NOINLINE void restore_state(void)
{
	const program_state *const state = state_pending;

	state_pending = NULL;
	memcpy((void *)state->bottom, state->stack, state->stack_size);
	memcpy(frame_registers, state->registers, sizeof(jmp_buf));
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
	/* (straight back to the registers, as the frames there are the state's, not those the C unwinds from here) */
	((_JUMP_BUFFER *)&frame_registers)->Frame = 0;
#endif
	longjmp(frame_registers, 1);
}

static void run_program(void)
{
	volatile unsigned char here = 0;

	stack_top = (uintptr_t)&here & ~(uintptr_t)15;
	started = true;
	if (setjmp(restart))
		reset_board();
	game_entry();
	stopped("the program came to its end, after", 0);
}

/* the 68000's time for the sound: where the program's commands to the chips fall in the frame (cave/sound.h) */
static uint64_t cycles_now(void)
{
	return m68k_time;
}

const char *program_start(void)
{
	const char *problem = load_tables();

	if (problem)
		return problem;
	cave_board.program = rom_set.program;
	memcpy(cave_board.eeprom.words, rom_set.eeprom, sizeof(cave_board.eeprom.words));
	cave_reset();
	cave_sound_clock(cycles_now, FRAME_CYCLES);
	status = 0x2700u;
	depth = 0;
	interrupts = 0;
	m68k_time = time_at_wait = work_began = handled_at = RESET_CYCLES;
	m68k_extra = 0;
	m68k_vblank_at = FRAME_CYCLES;
	slow_frames = 0;
	waiting = false;
	port_task = host_task_main();
	game_task = host_task_create(run_program);
	return NULL;
}

void program_reset(void)
{
	/* (before its first frame the program starts from its reset vector anyway: a reset then would cost the frame
	   it waits in first, and the frames after it would fall one later than after a reset of a running program, so
	   training's way to a place and a replay's start would not be the same from the power on) */
	if (started)
		reset_pending = true;
}

void program_run_frame(void)
{
	host_task_switch(game_task);
}

program_state *program_state_save(void)
{
	const size_t size = stack_top > stack_bottom ? (size_t)(stack_top - stack_bottom) : 0;
	program_state *state;

	if (!size || !(state = malloc(sizeof(*state) + size)))
		return NULL;
	state->board = cave_board;
	state->status = status;
	memcpy(state->saved, saved, sizeof(saved));
	state->depth = depth;
	state->interrupts = interrupts;
	state->time = m68k_time;
	state->vblank_at = m68k_vblank_at;
	state->slow_frames = slow_frames;
	state->work_began = work_began;
	state->handled_at = handled_at;
	state->waiting = waiting;
	memcpy(state->registers, frame_registers, sizeof(jmp_buf));
	state->bottom = stack_bottom;
	state->top = stack_top;
	state->stack_size = size;
	memcpy(state->stack, (const void *)stack_bottom, size);
	return state;
}

void program_state_load(const program_state *state)
{
	if (!state)
		return;
	/* the board and the status now; the stack and the registers on the game task, as it next goes on */
	cave_board = state->board;
	status = state->status;
	memcpy(saved, state->saved, sizeof(saved));
	depth = state->depth;
	interrupts = state->interrupts;
	m68k_time = time_at_wait = state->time;
	m68k_vblank_at = state->vblank_at;
	slow_frames = state->slow_frames;
	work_began = state->work_began;
	handled_at = state->handled_at;
	waiting = state->waiting;
	lagged = false;
	reset_pending = false;
	state_pending = state;
}

void program_state_free(program_state *state)
{
	free(state);
}

uint64_t program_frames(void)
{
	return cave_board.frame;
}

uint32_t program_interrupts(void)
{
	return interrupts;
}

void program_set_slowdown(bool on)
{
	slowdown = on;
}

bool program_slowdown(void)
{
	return slowdown;
}

bool program_timed(void)
{
	return timed;
}

uint64_t program_slow_frames(void)
{
	return slow_frames;
}

bool program_frame_lagged(void)
{
	return lagged;
}

uint64_t program_frame_cycles(void)
{
	return last_frame_cycles;
}
