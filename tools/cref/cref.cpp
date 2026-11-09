/**
 *    Name:        cref.cpp
 *    Description: DonPachi's original program on MAME's 68000 core, against the runtime's board model: the reference
 *                 the decompiled program is compared with, a frame at a time
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    After tools/core/konami/reference/kref.cpp (BSD-3-Clause, Sebastian Toyos), its "frame" timing: the board's
 *    next frame begins where the program waits for one (the profile's FRAME_WAIT, CREF_FRAME_WAIT here), as in the
 *    decompiled program, so that the two runs' frames are the same frames. MAME's 68000 (BSD-3-Clause,
 *    tools/core/konami/reference/mame) built from m68000.lst as MAME's build does.
 *
 *    Usage: cref [--rom PATH] [--frames N] [--hashes FILE] [--dump FRAME ... --dump-dir DIR] [--input F:T:P:BITS ...]
 *                [--cheat NAME ...] [--cheats FILE] [--trace-writes FROM:TO:FRAME]
 *      The options are the headless port's (ports/headless): the same inputs and cheats give the same game.
 *      --hashes          "FRAME HASH" per frame, cave_state_hash: compare with the headless port's --hashes
 *      --dump            the frame's memories (DIR/ref_FRAME.bin), as the headless port's dump_FRAME.bin
 *      --trace-writes    every write the program makes from address FROM to TO (hexadecimal) in frame FRAME,
 *                        with the instruction that makes it: "PC ADDRESS DATA MASK"
 */
#include "emu.h"
#include "m68000.h"

extern "C" {
#include "cave/cheats.h"
#include "cave/machine.h"
#include "cave/rom_set.h"
}

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#define CREF_FRAME_WAIT 0x002ED6u    /* tools/game_profile.py's FRAME_WAIT */

namespace {

class reference_cpu : public m68000_device
{
public:
	reference_cpu() : m68000_device(m_config, "maincpu", nullptr, CAVE_CPU_CLOCK) {}
	u32 pc() const { return m_ipc; }

private:
	static inline machine_config m_config;
};

struct held_input
{
	unsigned long long from, to;
	int player;
	unsigned bits;
};

reference_cpu *g_cpu = nullptr;
int g_irq_level = 0;
uint64_t g_instructions = 0, g_instructions_at_frame = 0;
uint32_t g_pc = 0;
std::vector<held_input> g_inputs;
uint32_t g_trace_from = 1, g_trace_to = 0;
uint64_t g_trace_frame = ~0ull;
FILE *g_trace = nullptr;

struct frames_done {};

void sync_irq()
{
	const int level = cave_irq_level();
	if (level == g_irq_level)
		return;
	if (g_irq_level)
		g_cpu->set_input_line(g_irq_level, CLEAR_LINE);
	if (level)
		g_cpu->set_input_line(level, ASSERT_LINE);
	g_irq_level = level;
}

u16 bus_read(offs_t address, u16 mem_mask)
{
	const u16 value = cave_read16(address, mem_mask);
	sync_irq();    // (reading 900004 clears the VBLANK interrupt)
	return value;
}

void bus_write(offs_t address, u16 data, u16 mem_mask)
{
	if (g_trace && cave_board.frame == g_trace_frame && (address & 0xFFFFFE) >= g_trace_from && (address & 0xFFFFFE) <= g_trace_to)
		std::fprintf(g_trace, "%06X %06X %04X %04X\n", g_pc & 0xFFFFFF, address & 0xFFFFFE, data, mem_mask);
	cave_write16(address, data, mem_mask);
	sync_irq();
}

// The interrupt acknowledge: every level takes its autovector
u16 acknowledge(offs_t address, u16 mem_mask)
{
	(void)mem_mask;
	return u16(0x18 + ((address >> 1) & 7));
}

void set_inputs()
{
	uint16_t pressed[2] = { 0, 0 };

	for (const held_input &input : g_inputs)
		if (cave_board.frame >= input.from && cave_board.frame <= input.to)
			pressed[input.player == 2] |= uint16_t(input.bits);
	cave_board.in0 = uint16_t(~pressed[0]);
	cave_board.in1 = uint16_t(~pressed[1]);
}

std::function<void()> g_frame_wait;

void on_instruction(offs_t pc)
{
	g_pc = pc;
	g_instructions++;
	if ((pc & 0xFFFFFF) == CREF_FRAME_WAIT)
	{
		g_instructions_at_frame = g_instructions;
		g_frame_wait();
	}
	else if (g_instructions - g_instructions_at_frame > 200000000ull)
		throw std::runtime_error("the program no longer reaches its frame wait");
}

} // namespace

int main(int argc, char **argv)
{
	std::string rom = "rom", dump_dir = ".", cheat_file = "cheats/donpachi.xml";
	unsigned long long frames = 600;
	FILE *hashes = nullptr;
	std::vector<unsigned long long> dumps;
	std::vector<std::string> cheats;

	for (int index = 1; index < argc; index++)
	{
		const std::string argument = argv[index];
		const bool value = index + 1 < argc;

		if (argument == "--rom" && value)
			rom = argv[++index];
		else if (argument == "--frames" && value)
			frames = std::strtoull(argv[++index], nullptr, 10);
		else if (argument == "--hashes" && value)
			hashes = std::fopen(argv[++index], "w");
		else if (argument == "--dump" && value)
			while (index + 1 < argc && argv[index + 1][0] != '-')
				dumps.push_back(std::strtoull(argv[++index], nullptr, 10));
		else if (argument == "--dump-dir" && value)
			dump_dir = argv[++index];
		else if (argument == "--input" && value)
		{
			held_input input{};
			if (std::sscanf(argv[++index], "%llu:%llu:%d:%x", &input.from, &input.to, &input.player, &input.bits) != 4)
			{
				std::fprintf(stderr, "--input FROM:TO:PLAYER:BITS\n");
				return 2;
			}
			g_inputs.push_back(input);
		}
		else if (argument == "--cheat" && value)
			cheats.push_back(argv[++index]);
		else if (argument == "--cheats" && value)
			cheat_file = argv[++index];
		else if (argument == "--trace-writes" && value)
		{
			unsigned from, to;
			unsigned long long frame;
			if (std::sscanf(argv[++index], "%x:%x:%llu", &from, &to, &frame) != 3)
			{
				std::fprintf(stderr, "--trace-writes FROM:TO:FRAME (addresses in hexadecimal)\n");
				return 2;
			}
			g_trace_from = from;
			g_trace_to = to;
			g_trace_frame = frame;
			g_trace = stdout;
		}
		else
		{
			std::fprintf(stderr, "usage: cref [--rom PATH] [--frames N] [--hashes FILE] [--dump FRAME ... --dump-dir DIR]"
				" [--input F:T:P:BITS ...] [--cheat NAME ...] [--cheats FILE] [--trace-writes FROM:TO:FRAME]\n");
			return 2;
		}
	}
	if (const char *problem = rom_set_load(rom.c_str()))
	{
		std::fprintf(stderr, "The game does not start: %s\n", problem);
		return 1;
	}
	for (const std::string &cheat : cheats)
		if (const char *problem = cheats_enable(cheat_file.c_str(), cheat.c_str()))
		{
			std::fprintf(stderr, "%s\n", problem);
			return 1;
		}
	cave_board.program = rom_set.program;
	std::memcpy(cave_board.eeprom.words, rom_set.eeprom, sizeof(cave_board.eeprom.words));
	cave_reset();
	set_inputs();

	reference_cpu cpu;
	g_cpu = &cpu;
	for (int space : { int(AS_PROGRAM), int(AS_OPCODES) })
	{
		cpu.space(space).read16 = bus_read;
		cpu.space(space).write16 = bus_write;
	}
	cpu.space(AS_OPCODES).instructions = true;
	cpu.space(m68000_base_device::AS_CPU_SPACE).read16 = acknowledge;
	cpu.space(m68000_base_device::AS_CPU_SPACE).write16 = bus_write;
	cpu.instruction_hook = on_instruction;
	cpu.log_handler = [](const std::string &message) { std::fputs(message.c_str(), stderr); };
	cpu.start();
	cpu.reset();

	// the frame, where the program waits for one: as the decompiled program's frame_wait() and its port
	g_frame_wait = [&]() {
		cave_vblank_start();
		cheats_apply();
		if (hashes)
			std::fprintf(hashes, "%" PRIu64 " %016" PRIx64 "\n", cave_board.frame, cave_state_hash());
		for (unsigned long long dump : dumps)
			if (dump == cave_board.frame)
				cave_dump((dump_dir + "/ref_" + std::to_string(1000000 + dump).substr(2) + ".bin").c_str());
		if (cave_board.frame >= frames)
			throw frames_done();
		set_inputs();
		sync_irq();
	};
	int status = 0;
	try
	{
		for (;;)
			cpu.run(1 << 20);
	}
	catch (const frames_done &)
	{
	}
	catch (const std::exception &error)
	{
		std::fprintf(stderr, "68000 stopped at PC %06X in frame %" PRIu64 ": %s\n", cpu.pc(), cave_board.frame, error.what());
		status = 2;
	}
	if (hashes)
		std::fclose(hashes);
	std::printf("frames %" PRIu64 ", instructions %" PRIu64 ", unmapped reads %u, writes %u\n", cave_board.frame,
		g_instructions, unsigned(cave_board.unmapped_reads), unsigned(cave_board.unmapped_writes));
	return status;
}
