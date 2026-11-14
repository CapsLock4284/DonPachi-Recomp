/**
 *    Name:        cref_timed.cpp
 *    Description: DonPachi's original program on MAME's 68000 core at the board's own timing: the VBLANK interrupt
 *                 every CAVE_FRAME_USEC of 68000 cycles (278,016 at 16 MHz), whether or not the program is waiting
 *                 for it. A frame whose work runs past the next interrupt makes the program wait a whole frame more:
 *                 the arcade's slowdown. Logs each frame's work in cycles and what was on the screen
 *    Version:     0.2.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    After cref.cpp (the reference, frame timed). MAME's 68000 (BSD-3-Clause) built from m68000.lst.
 *
 *    Usage: cref_timed [--rom PATH] [--set japan] [--frames N] [--log FILE] [--hashes FILE] [--waits FILE] [--play]
 *      --set     japan: the Japanese set's program ('donpachij', Ver. 1.01: rom_set_load_version), the US one's by
 *                default (both wait for their frame at the same address, 002ED6)
 *      --frames  the board's frames (VBLANKs) to run (default 18000: about 5 minutes)
 *      --log     a line per VBLANK: "vblank game_frame busy_cycles lagged bullets objects records": busy_cycles the
 *                game's work since its last wait ended (to where it waits again; -1: still working), lagged 1 when
 *                this VBLANK came while it was still working
 *      --hashes  a line per VBLANK, "vblank lagged hash": cave_state_hash() as the VBLANK starts, in the middle of
 *                an instruction (tests/harness/slowdown_check's are at the end of the instruction that takes the
 *                interrupt: --waits is the exact comparison)
 *      --waits   a line per frame of the game, as it comes to its wait for the next one, its work done: the wait
 *                loop's first round (FRAME_WAIT) in a wait the program has made ready (0x1011FC bit 0), once until
 *                the VBLANK's interrupt ends it: "game_frame vblank hash" (the VBLANKs so far, cave_state_hash()).
 *                The port's tests/harness/slowdown_check writes the same (program_at_wait); slowdown_compare.py
 *                compares them. A VBLANK that came while the program had made no frame ready is a VBLANK lagged
 *      --play    plays: a coin and Start in the attract mode, the first ship, then sweeping and firing (shielded),
 *                as the test tools' simple player; the inputs for a VBLANK are set as its interrupt is taken, when
 *                the port's harness sets them
 *    DUMPFRAME=N (environment): the board's memories at game frame N's wait, in mame_dump.bin (the port's harness:
 *    port_dump.bin); DUMP=N: at VBLANK N
 */
#include "emu.h"
#include "m68000.h"

extern "C" {
#include "cave/machine.h"
#include "cave/rom_set.h"
}

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#define CREF_FRAME_WAIT 0x002ED6u    /* tools/game_profile.py's FRAME_WAIT: the wait for the next frame */

namespace {

class reference_cpu : public m68000_device
{
public:
	reference_cpu() : m68000_device(m_config, "maincpu", nullptr, CAVE_CPU_CLOCK) {}
	u32 pc() const { return m_ipc; }

private:
	static inline machine_config m_config;
};

reference_cpu *g_cpu = nullptr;
FILE *g_waits = nullptr;
int g_irq_level = 0;
uint64_t g_total = 0;                // the cycles done
uint64_t g_cycles_before_run = 0;    // the cycles done before the current run()
int g_run_length = 0;
uint64_t g_work_began = 0;           // where the game's work began (its wait ended), in cycles
bool g_waiting = false;              // the program is in its wait for the next frame
uint64_t g_game_frames = 0;
int64_t g_last_busy = -1;            // the last frame's work, in cycles (-1: none ended since the last VBLANK)

uint16_t ram(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

uint64_t now()
{
	return g_cycles_before_run + uint64_t(g_run_length > 0 ? g_run_length - g_cpu->cycles_left() : 0);
}

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
	sync_irq();
	return value;
}

void bus_write(offs_t address, u16 data, u16 mem_mask)
{
	cave_write16(address, data, mem_mask);
	sync_irq();
}

bool g_play = false;
unsigned player_inputs(uint64_t frame);
uint64_t g_acks = 0;

u16 acknowledge(offs_t address, u16 mem_mask)
{
	(void)mem_mask;
	g_acks++;
	if (g_play && ((address >> 1) & 7) == 1)
	{
		/* (the player's inputs for this VBLANK, from the game as the interrupt is taken: when the headless port's
		   harness sets them, its frame coming as the 68000 takes the interrupt) */
		g_play = false;
		cave_board.in0 = uint16_t(~player_inputs(cave_board.frame));
	}
	if (g_waiting && ((address >> 1) & 7) == 1)
	{
		/* (the VBLANK's interrupt ends the wait: the work of the next frame begins) */
		g_waiting = false;
		g_work_began = now();
	}
	return u16(0x18 + ((address >> 1) & 7));
}

void on_instruction(offs_t pc)
{
	const bool at_wait = (pc & 0xFFFFFF) == CREF_FRAME_WAIT;

	if (at_wait && !g_waiting && (ram(0x1011FCu) & 1u))
	{
		/* (a wait the program has made ready, its first round: a round the interrupt comes in after it tested its
		   flag, or before the interrupt is taken, is the wait the program is already in) */
		/* (the work done: from where the last wait ended to here) */
		g_waiting = true;
		g_last_busy = int64_t(now() - g_work_began);
		g_game_frames++;
		if (getenv("DUMPFRAME") && g_game_frames == std::strtoull(getenv("DUMPFRAME"), nullptr, 10))
			cave_dump("mame_dump.bin");
		if (g_waits)
			std::fprintf(g_waits, "%" PRIu64 " %" PRIu64 " %016" PRIx64 "\n", g_game_frames, cave_board.frame, cave_state_hash());
	}
}

/* the simple player (tests/harness/common.h's autoplayer): player 1's inputs, active high */
unsigned player_inputs(uint64_t frame)
{
	static uint64_t select_frames;
	const unsigned mode = ram(0x1018ECu);
	unsigned held = 0;

	if (mode == 0xBu)
	{
		if (++select_frames > 60 && (select_frames / 8) % 2)
			held = 0x0010u;    /* (button 1) */
	}
	else if (mode == 7u)
	{
		const int x = int16_t(ram(0x102158u + 6u)) >> 6;
		int target = 40 + int(frame / 3 % 160);

		if ((frame / 480) & 1)
			target = 200 - int(frame / 3 % 160);
		*cave_work_ram_byte(0x102196u) = 2;    /* (shielded) */
		if (x < target - 4)
			held |= 0x0008u;   /* (right) */
		else if (x > target + 4)
			held |= 0x0004u;   /* (left) */
		held |= (frame / 2) % 2 ? 0x0010u : 0u;
		if ((frame / 300) % 4 == 3)
			held |= 0x0010u;   /* (held: the laser) */
	}
	else if (frame % 120 < 4)
		held = 0x0100u;        /* (coin) */
	else if (frame % 120 > 60 && frame % 120 < 64)
		held = 0x0080u;        /* (start) */
	return held;
}

} // namespace

int main(int argc, char **argv)
{
	std::string rom = "rom";
	unsigned long long frames = 18000;
	FILE *log = nullptr, *hashes = nullptr;
	bool play = false;
	donpachi_version version = DONPACHI_US;

	for (int index = 1; index < argc; index++)
	{
		const std::string argument = argv[index];
		const bool value = index + 1 < argc;

		if (argument == "--rom" && value)
			rom = argv[++index];
		else if (argument == "--set" && value)
		{
			const std::string set = argv[++index];

			if (set != "japan" && set != "us")
			{
				std::fprintf(stderr, "--set us or --set japan\n");
				return 2;
			}
			version = set == "japan" ? DONPACHI_JAPAN : DONPACHI_US;
		}
		else if (argument == "--frames" && value)
			frames = std::strtoull(argv[++index], nullptr, 10);
		else if (argument == "--log" && value)
			log = std::fopen(argv[++index], "w");
		else if (argument == "--waits" && value)
			g_waits = std::fopen(argv[++index], "w");
		else if (argument == "--hashes" && value)
			hashes = std::fopen(argv[++index], "w");
		else if (argument == "--play")
			play = true;
		else
		{
			std::fprintf(stderr, "usage: cref_timed [--rom PATH] [--set japan] [--frames N] [--log FILE] [--hashes FILE] "
				"[--waits FILE] [--play]\n");
			return 2;
		}
	}
	if (const char *problem = rom_set_load_version(rom.c_str(), version))
	{
		std::fprintf(stderr, "The game does not start: %s\n", problem);
		return 1;
	}
	cave_board.program = rom_set.program;
	std::memcpy(cave_board.eeprom.words, rom_set.eeprom, sizeof(cave_board.eeprom.words));
	cave_reset();
	cave_board.in0 = cave_board.in1 = 0xFFFFu;

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

	const uint64_t frame_cycles = uint64_t(CAVE_CPU_CLOCK) * CAVE_FRAME_USEC / 1000000u;
	uint64_t lagged = 0, busiest = 0;

	for (uint64_t vblank = 1; vblank <= frames; vblank++)
	{
		/* (the frame's cycles, to the next VBLANK) */
		while (g_total < vblank * frame_cycles)
		{
			g_cycles_before_run = g_total;
			g_run_length = int(vblank * frame_cycles - g_total);
			g_total += uint64_t(cpu.run(g_run_length));
			g_cycles_before_run = g_total;
			g_run_length = 0;
		}
		/* VBLANK: the interrupt; the program's wait (if it is waiting) ends as its handler sets the frame's flag */
		const bool was_waiting = g_waiting;
		const bool lag = !was_waiting && vblank > 1;
		uint64_t bullets = 0, objects = 0, records = 0;

		g_play = play;
		for (uint32_t at = 0x104E94u; at < 0x104E94u + 75u * 0x40u; at += 0x40u)
			bullets += (ram(at) & 0x8000u) != 0;
		for (uint32_t at = 0x100102u; at < 0x100102u + 64u * 0x40u; at += 0x40u)
			objects += (ram(at) & 0x8000u) != 0;
		for (uint32_t at = 0x103814u; at < 0x103814u + 150u * 0x20u; at += 0x20u)
			records += (ram(at) & 0x8000u) != 0;
		if (log)
			std::fprintf(log, "%" PRIu64 " %" PRIu64 " %" PRId64 " %d %" PRIu64 " %" PRIu64 " %" PRIu64 " %u %d\n", vblank,
				g_game_frames, was_waiting ? g_last_busy : int64_t(-1), lag ? 1 : 0, bullets, objects, records,
				ram(0x1018ECu), int16_t(ram(0x101A40u)));
		if (lag && ram(0x1018ECu) == 7u)
			lagged++;
		if (was_waiting && g_last_busy > int64_t(busiest) && ram(0x1018ECu) == 7u)
			busiest = uint64_t(g_last_busy);
		cave_vblank_start();
		if (getenv("DUMP") && vblank == std::strtoull(getenv("DUMP"), nullptr, 10))
			cave_dump("mame_dump.bin");
		if (hashes)
			std::fprintf(hashes, "%" PRIu64 " %d %016" PRIx64 "\n", vblank, lag ? 1 : 0, cave_state_hash());
		sync_irq();
	}
	std::printf("vblanks %llu, game frames %" PRIu64 ", lagged in play %" PRIu64 ", the busiest frame %" PRIu64
		" cycles of %" PRIu64 "\n", frames, g_game_frames, lagged, busiest, frame_cycles);
	if (log)
		std::fclose(log);
	if (hashes)
		std::fclose(hashes);
	return 0;
}
