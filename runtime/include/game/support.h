/**
 *    Name:        support.h
 *    Description: what DonPachi's decompiled 68000 program (src/) uses besides its own C: the work RAM, the
 *                 program's data, the board's devices on the bus, the frame and the interrupts, and the 68000's
 *                 arithmetic where the IR does not spell it out
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    tmntarcade-decomp's runtime/include/game/support.h (BSD-3-Clause, Sebastian Toyos: runtime/LICENSE.tmnt-runtime)
 *    with DonPachi's board (cave/machine.h): its work RAM at 100000, its memories' places. The program's data is
 *    read from the ROM set's program region (runtime/src/game_program.c), not from extracted assets.
 */
#ifndef GAME_SUPPORT_H
#define GAME_SUPPORT_H

#include "m68000/ops.h"
#include "cave/machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Functions that stop the game: the C compiler knows they do not come back */
#if defined(_MSC_VER)
#define GAME_NORETURN __declspec(noreturn)
#else
#define GAME_NORETURN _Noreturn
#endif

/* A game function the board puts its own in place of (runtime/replaced.txt): tools/decompile.py writes its
   definition as BOARD_ORIGINAL(name), which keeps the original as original_name */
#define BOARD_ORIGINAL(name) original_##name

/* A game function a port's console build (console/, GAME_CONSOLE) puts its own in place of (console/replaced.txt):
   tools/decompile.py writes its definition as CONSOLE_ORIGINAL(name), which is original_name there, for the
   console's to call, and the game's own name in every other build */
#ifdef GAME_CONSOLE
#define CONSOLE_ORIGINAL(name) original_##name
#else
#define CONSOLE_ORIGINAL(name) name
#endif

/* ---- the work RAM ---- */

#define WORK_RAM 0x100000u

#define RAM8(address)   (*(uint8_t *)cave_work_ram_at((address), 1u))
#define RAM16(address)  (*(uint16_t *)cave_work_ram_at((address), 2u))
#define RAM32(address)  (*(uint32_t *)cave_work_ram_at((address), 4u))
#define RAM8S(address)  (*(int8_t *)cave_work_ram_at((address), 1u))
#define RAM16S(address) (*(int16_t *)cave_work_ram_at((address), 2u))
#define RAM32S(address) (*(int32_t *)cave_work_ram_at((address), 4u))

/* A part of a variable, or a variable read with another width: 'offset' bytes from the host's start of it */
#define AS8(variable)  (*(uint8_t *)&(variable))
#define AS16(variable) (*(uint16_t *)&(variable))
#define AS32(variable) (*(uint32_t *)&(variable))
#define AT8(variable, offset)  (*(uint8_t *)((uint8_t *)&(variable) + (offset)))
#define AT16(variable, offset) (*(uint16_t *)((uint8_t *)&(variable) + (offset)))
#define AT32(variable, offset) (*(uint32_t *)((uint8_t *)&(variable) + (offset)))

/* A structure of the game's in work RAM at 'address', 'size' bytes (ram.h: OBJECT(address)): the host's place
   of it, where it runs the other way round, so that ram.h declares its fields from its end */
#define ram(address, size) cave_work_ram_at((address), (size))

/* ---- the program's data ---- */

/* The data at an address the program holds: the piece of the assets it falls in. Where none is (the program's
   code, which the game does not have), the program stops */
const uint8_t *data_at(uint32_t address);

static inline uint32_t data8(uint32_t address)
{
	return *data_at(address);
}

static inline uint32_t data16(uint32_t address)
{
	return (uint32_t)*data_at(address) << 8 | *data_at(address + 1u);
}

static inline uint32_t data32(uint32_t address)
{
	return data16(address) << 16 | data16(address + 2u);
}

/* A record of the program's data at an address the program holds (ram.h: ANIMATION_FRAME(address)): its 'size'
   bytes the other way round, as the work RAM's structures, so that ram.h declares its fields from its end too.
   From the piece of the assets it is in (or the work RAM, where the program set the pointer there); bytes past
   the pieces (a record the code reads only in part, runtime/src/game_program.c) read 0 */
const void *data_record(uint32_t address, uint32_t size);

#define DATA8(address)  data8(address)
#define DATA16(address) data16(address)
#define DATA32(address) data32(address)
#define DATA8S(address)  ((int8_t)data8(address))
#define DATA16S(address) ((int16_t)data16(address))
#define DATA32S(address) ((int32_t)data32(address))

/* A table the C reads by name (the decompiler's registry, src/table.c): its name, its address, the pointer the
   loader sets to its copy, and the bytes of its elements, which the copy has in the host's order */
typedef struct
{
	const char *name;
	uint32_t address;
	const void **data;
	uint32_t element;
} game_table;

extern const game_table game_tables[];
extern const size_t game_table_count;

/* A table read with another width than its elements': 'offset' bytes into it, as the program has it */
#define TABLE8(table, offset)  DATA8(table##_ADDRESS + (offset))
#define TABLE16(table, offset) DATA16(table##_ADDRESS + (offset))
#define TABLE32(table, offset) DATA32(table##_ADDRESS + (offset))

/* A place in the 64 KB page 'base' points into, 'link' bytes from its start; a place in the 256-byte page, 'low'
   bytes from its start (an address register whose low word or byte the code changes on its own) */
#define LINKED(base, link) (((base) & 0xFFFF0000u) | (link))
#define IN_PAGE(base, low) (((base) & ~0xFFu) | (low))

/* ---- the board's memories (addresses the code computes into them) ---- */

#define PALETTE_RAM 0xA08000u
#define SPRITE_RAM  0x500000u
#define LAYER0_RAM  0x300000u
#define LAYER1_RAM  0x200000u
#define LAYER2_RAM  0x400000u

/* A register of a device of the board, by its name: the bus hands it the read or the write */
#define io_read8(address)          mem_read8(address)
#define io_read16(address)         mem_read16(address)
#define io_read32(address)         mem_read32(address)
#define io_write8(address, data)   mem_write8((address), (uint8_t)(data))
#define io_write16(address, data)  mem_write16((address), (uint16_t)(data))
#define io_write32(address, data)  mem_write32((address), (uint32_t)(data))

/* ---- the frame, the status register and the interrupts ---- */

/* The main loop finds nothing to do (the profile's FRAME_WAIT): the board moves on to its next frame (frame
   timing, tmnt/frame.h: instructions take no time); then the program takes the interrupts the board raised */
void frame_wait(void);
void take_interrupts(void);

/* The board's time, for a program decompiled with its timing (the profile's TIMING_MODEL: tools/core's
   m68000/timing.py): every instruction of the C is followed by m68k_block, its 68000 cycles and its interrupt
   point (a branch's longer way, and the data's part of a multiply, divide or shift, add m68k_cycles before it).
   When the VBLANK came by the instruction's point, its interrupt is taken at the instruction's end, as the 68000
   does (it samples its interrupt lines at one step of each instruction, and a request reaches it as a bus cycle
   ends: MAME's core); a VBLANK that came after the point is taken after the next instruction. When that is in the
   middle of the program's work, the port has a frame there (the picture as it was) and the program then waits a
   frame more for its own: the arcade's slowdown. m68k_branch: a Bcc or DBcc, whose other way has its own point
   (plus that way's extra cycles) */
extern uint64_t m68k_time, m68k_vblank_at;
extern uint32_t m68k_extra;
void m68k_vblank_came(void);

static inline void m68k_cycles(uint32_t cycles)
{
	m68k_time += cycles;
	m68k_extra += cycles;
}

static inline void m68k_block(uint32_t cycles, uint32_t point)
{
	const uint64_t start = m68k_time - m68k_extra;

	m68k_time += cycles;
	m68k_extra = 0;
	if (start + point >= m68k_vblank_at)
		m68k_vblank_came();
}

static inline void m68k_branch(uint32_t cycles, uint32_t point, uint32_t other_way)
{
	const uint32_t extra = m68k_extra;
	const uint64_t start = m68k_time - extra;

	m68k_time += cycles;
	m68k_extra = 0;
	if (start + (extra ? other_way + extra : point) >= m68k_vblank_at)
		m68k_vblank_came();
}

/* MOVE to SR, ANDI and ORI to SR: the status register's mode and interrupt mask (its flags are the C's own);
   RTE puts back the status register the interrupt or exception found; STOP waits for an interrupt; RESET pulses
   the devices' reset line (the board connects none) */
void write_status(uint32_t value);

/* The status register's system byte now (SR & 0xA700: trace, supervisor, the interrupt mask), for an instruction
   that reads the status register (MOVE from SR, ANDI/ORI/EORI to SR) in a timed program's C */
uint16_t status_system(void);
void return_from_exception(void);
void stop(uint32_t value);
void reset_devices(void);

/* The code the board runs by address: the handlers of the vectors (src/table.c) */
typedef struct
{
	uint32_t address;
	void (*function)(void);
} game_code_entry;

extern const game_code_entry game_code[];      /* sorted by address */
extern const size_t game_code_count;
void game_entry(void);                          /* the code at the reset vector */

/* What the code leaves in its registers for later code to read, global variables of the C (src/game.h): an
   interrupt's handler finds them as the code it interrupts left them, and the runtime puts them back when it
   returns, as the VBLANK handler puts back the registers it saves (src/table.c) */
extern uint32_t *const game_context[];
extern const size_t game_context_count;

/* The default of a switch on a jump's target, with decompile.py --checks: the original would jump to 'address',
   which no case handles */
GAME_NORETURN void invalid_jump(uint32_t address);

/* Code no path reaches that sets its stack pointer to a value it reads, or keeps a frame of its own: the C,
   which has no stack, cannot go on there */
GAME_NORETURN void lost_stack(uint32_t address);

/* With tools/decompile.py --trace, every function calls game_trace when it starts and every block of code
   game_trace_block: the last ones are shown when the program stops */
void game_trace(uint32_t address);
void game_trace_block(uint32_t address);

#ifdef __cplusplus
}
#endif

/* where the C never goes: past a switch whose every case returns (C23's unreachable()) */
#ifndef unreachable
#if defined(_MSC_VER)
#define unreachable() __assume(0)
#else
#define unreachable() __builtin_unreachable()
#endif
#endif

#endif
