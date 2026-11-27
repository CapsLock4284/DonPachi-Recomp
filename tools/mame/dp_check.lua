-- Name:        dp_check.lua
-- Description: DonPachi on the real MAME, for tests/harness/mame_check: MAME's own pictures with the board's video
--              memories (drawn again by the port), and the inputs it was given (to play the port the same way and
--              set its sound beside MAME's)
-- Version:     0.3.1 (after DoDonPachi Recomp's tools/mame/ddp_check.lua 0.5.0; 0.2.0: waits.txt, the slowdown;
--              0.3.0: the random index in waits.txt, irq.txt; 0.3.1: rounds.txt)
-- Created:     26-11-2026
--
-- license:BSD-3-Clause
--
-- Run from the project's folder with a MAME of 0.254 or later (its Lua API), where donpachi.zip is in MAME's ROM
-- path, with empty folders for MAME's NVRAM and settings (the ROM set's own EEPROM, as the port's checks start with):
--
--     mkdir build\mame_dp\nvram, build\mame_dp\cfg
--     $env:DP_OUT = "build/mame_dp"
--     mame donpachi -rompath rom -autoboot_script tools\mame\dp_check.lua -nothrottle -window -skip_gameinfo
--          -nvram_directory build\mame_dp\nvram -cfg_directory build\mame_dp\cfg -wavwrite build\mame_dp\sound.wav
--
-- (mame: the full path of mame.exe, if it isn't on the PATH.) Then: tests/harness/bin/mame_check build/mame_dp
--
-- It writes into DP_OUT (build/mame_dp):
--     log.txt        a line a VBLANK: "vblank inputs signature mode" (the inputs given at that VBLANK, for the next
--                    one's interrupt to read, as the port gives them; the signature a sum of the ships' records, the scores, the stage and its scroll; the
--                    game's mode, $1018EC)
--     video_N.bin    the board's video memories at VBLANK N, every DP_VIDEO_EVERY VBLANKs (2,500) and the VBLANK
--                    before: "DPCVID01", then the sprite RAM ($500000, 64 KB), the layers' RAM ($300000 layer 0,
--                    $200000 layer 1, $400000 layer 2, 32 KB each), their registers ($700000, $600000, $800000, 6
--                    bytes each), the palette ($A08000, 4 KB), the video registers (128 bytes, from MAME's own copy:
--                    reading $900000 would answer the interrupt) and the work RAM ($100000, 64 KB), each as the 68000
--                    reads them (big endian); with screen_N.bin beside each (MAME's picture, 32-bit, little endian)
--                    and screen_size.txt
--     waits.txt      a line a frame of the game, as it makes its wait for the next ready ($1011FC bit 0, as
--                    tests/harness/slowdown_check's at_wait): "game_frame vblank cycle mode random", the VBLANKs so
--                    far, the 68000's cycle from the power on (MAME's time there, at the CPU's own instruction, times
--                    its clock), the game's mode and its random index ($101263: counted on each round of its wait for
--                    the frame, so the rounds tell how near the VBLANK the work ended): when each frame's work ended,
--                    to set beside the port's (mame_check SOUND=1)
--     irq.txt        each read of the interrupt's cause ($900000-$900007) and of the sound chips' status ($B00000,
--                    $B00010) for the first DP_IRQ_FRAMES VBLANKs (8,000): "vblank cycle what value" (what: irq0-3,
--                    oki0-1), where the board answers by the time
--     rounds.txt     with DP_ROUNDS=A-B (VBLANKs A to B, e.g. 250-260): each write of the random index ($101262,
--                    each round of the frame's wait and each random number taken), "vblank cycle value": how long
--                    MAME's 68000 takes for a round of the wait (the port's: 46 cycles)
-- and MAME writes sound.wav (-wavwrite): its own sound all through, to set beside the port's (mame_check SOUND=1).
-- It quits at DP_FRAMES VBLANKs (200,000: both loops and the ending, about 58 minutes of the game; 10-20 with
-- -nothrottle).
--
-- The player, by the VBLANK and the game's mode: a coin and Start in the attract mode, button 1 on the ship select
-- (the first ship), then sweeping left and right while firing (tests/harness/common.h's autoplayer, by the VBLANK
-- alone), the ship's respawn shield held on ($102196 = 2: no deaths) through the play. The port plays the inputs
-- logged here, with the same shield, so the two games stay together as long as the board's timing does.

local out = os.getenv("DP_OUT") or "build/mame_dp"
local frames = tonumber(os.getenv("DP_FRAMES") or "200000")
local video_every = tonumber(os.getenv("DP_VIDEO_EVERY") or "2500")

local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local screen = nil
for tag, s in pairs(machine.screens) do screen = s; break end

-- ---- the player ----
local select_frames = 0
local function player_inputs(frame)
	local mode = space:read_u16(0x1018ec)
	local held = 0
	if mode == 0x0b then
		select_frames = select_frames + 1
		if select_frames > 60 and math.floor(select_frames / 8) % 2 == 1 then held = 0x0010 end
	elseif mode == 7 then
		local target = 40 + (math.floor(frame / 3) % 160)
		if math.floor(frame / 480) % 2 == 1 then target = 200 - (math.floor(frame / 3) % 160) end
		local x = space:read_i16(0x102158 + 6) >> 6
		if x < target - 4 then held = held | 0x0008 elseif x > target + 4 then held = held | 0x0004 end
		if math.floor(frame / 40) % 3 == 0 or math.floor(frame / 2) % 2 == 1 then held = held | 0x0010 end
	elseif frame > 400 then
		local beat = frame % 40
		if beat < 5 then held = 0x0100 elseif beat >= 20 and beat < 25 then held = 0x0080 end
	end
	return held
end

-- the board's input bits (cave/machine.h: CAVE_UP 0x01 ... CAVE_COIN 0x100) and MAME's names for them
local wanted = {
	{ 0x0001, "P1 Up" }, { 0x0002, "P1 Down" }, { 0x0004, "P1 Left" }, { 0x0008, "P1 Right" },
	{ 0x0010, "P1 Button 1" }, { 0x0020, "P1 Button 2" }, { 0x0040, "P1 Button 3" },
	{ 0x0080, "1 Player Start" }, { 0x0100, "Coin 1" },
}
local fields = {}
for _, want in ipairs(wanted) do
	for _, port in pairs(machine.ioport.ports) do
		local field = port.fields[want[2]]
		if field then fields[#fields + 1] = { want[1], field }; break end
	end
	if not fields[#fields] or fields[#fields][1] ~= want[1] then
		emu.print_error("dp_check: no input named \"" .. want[2] .. "\" in this MAME")
		machine:exit()
		return
	end
end

local function set_inputs(held)
	for _, f in ipairs(fields) do
		f[2]:set_value((held & f[1]) ~= 0 and 1 or 0)
	end
end

local function signature()
	local sum = 0
	for address = 0x102158, 0x1021d6, 2 do sum = sum + space:read_u16(address) end
	sum = sum + space:read_u16(0x10177a) + space:read_u16(0x10177c) + space:read_u16(0x10177e) + space:read_u16(0x101780)
	sum = sum + space:read_u16(0x101a40) + space:read_u16(0x101a42) + space:read_u16(0x101a56)
	return sum & 0xffffffff
end

local function write_file(path, data)
	local file = io.open(path, "wb")
	if file then file:write(data); file:close() end
end

-- the video registers: MAME's own copy of them (the share named videoregs), as $900000-$900007 read the interrupt's
-- cause (and answer it). 128 bytes, big endian; zeros if this MAME has no such share
local videoregs = nil
if machine.memory and machine.memory.shares then
	for tag, share in pairs(machine.memory.shares) do
		if string.find(tag, "videoregs", 1, true) then videoregs = share; break end
	end
end
if not videoregs then
	emu.print_error("dp_check: no videoregs share in this MAME: video_N.bin's video registers are zeros")
end

local function video_registers()
	local bytes = {}
	for word = 0, 63 do
		local value = 0
		if videoregs then
			local ok, got = pcall(function() return videoregs:read_u16(word * 2) end)
			if ok and type(got) == "number" then value = got end
		end
		bytes[#bytes + 1] = string.char((value >> 8) & 0xff, value & 0xff)
	end
	return table.concat(bytes)
end

local function write_video(n)
	local parts = {
		"DPCVID01",
		space:read_range(0x500000, 0x50ffff, 8),
		space:read_range(0x300000, 0x307fff, 8),
		space:read_range(0x200000, 0x207fff, 8),
		space:read_range(0x400000, 0x407fff, 8),
		space:read_range(0x700000, 0x700005, 8),
		space:read_range(0x600000, 0x600005, 8),
		space:read_range(0x800000, 0x800005, 8),
		space:read_range(0xa08000, 0xa08fff, 8),
		video_registers(),
		space:read_range(0x100000, 0x10ffff, 8),
	}
	write_file(string.format("%s/video_%d.bin", out, n), table.concat(parts))
	if screen then
		local pixels, width, height = screen:pixels()
		write_file(string.format("%s/screen_%d.bin", out, n), pixels)
		write_file(out .. "/screen_size.txt", string.format("%d %d\n", width, height))
	end
end

local log = io.open(out .. "/log.txt", "w")
if not log then
	emu.print_error("dp_check: cannot write " .. out .. "/log.txt (make the folder first)")
	machine:exit()
	return
end
log:write("; dp_check.lua 0.3.1: MAME " .. emu.app_version() .. ", " .. emu.romname() .. "\n")

-- The inputs: the port gives inputs(N) after its VBLANK N, for the game's interrupt at the next VBLANK to read.
-- MAME's frame notifier for VBLANK N comes after that VBLANK's interrupt has read them already, so the inputs worked
-- out here are given for VBLANK N + 1 (as ddp_check.lua 0.2.0's): the log's line N names those given at N, as the
-- port gives its inputs after its VBLANK N for the next
local vblank = 0
local function on_frame()
	vblank = vblank + 1
	local mode = space:read_u16(0x1018ec)
	if mode == 7 then space:write_u8(0x102196, 2) end    -- (the ship's shield: no deaths)
	local held = player_inputs(vblank)
	set_inputs(held)
	log:write(string.format("%d %04x %08x %x\n", vblank, held, signature(), mode))
	if video_every > 0 and (vblank % video_every == 0 or vblank % video_every == video_every - 1) then
		write_video(vblank)
	end
	if vblank >= frames then
		log:close()
		if waits then waits:close() end
		if irq_file then irq_file:close() end
		emu.print_info("dp_check: done, " .. vblank .. " VBLANKs in " .. out)
		machine:exit()
	end
end

-- ---- the game's frames: its wait made ready ($1011FC bit 0) ----
local waits = io.open(out .. "/waits.txt", "w")
local game_frames = 0
-- DonPachi's 68000, 16 MHz (cave.cpp; the port's CAVE_CPU_CLOCK). MAME 0.289's Lua may have no clock on a device:
-- the board's own number then
local clock = 16000000
if type(cpu.clock) == "number" and cpu.clock > 0 then clock = cpu.clock end
local attos_per_cycle = 1000000000000000000 // clock
-- MAME's time now in the 68000's cycles from the power on: in a tap, the running CPU's own, to its instruction
local function cycle_now()
	local now = machine.time
	local ok, cycle = pcall(function() return now.seconds * clock + now.attoseconds // attos_per_cycle end)
	if ok and type(cycle) == "number" then return cycle end
	return math.floor(now:as_double() * clock + 0.5)
end
if waits then
	waits:write("; dp_check.lua 0.3.1: MAME " .. emu.app_version() .. ", " .. emu.romname() .. ", clock " .. clock .. "\n")
	dp_check_ready = space:install_write_tap(0x1011fc, 0x1011fd, "dp_check_ready", function(offset, data, mask)
		-- (the word's low byte, $1011FD, bit 0: the wait made ready)
		if (data & 1) ~= 0 and (mask & 1) ~= 0 then
			game_frames = game_frames + 1
			waits:write(string.format("%d %d %d %x %d\n", game_frames, vblank, cycle_now(), space:read_u16(0x1018ec),
				space:read_u8(0x101263)))
		end
	end)
end

-- ---- the reads the board answers by the time: the interrupt's cause, the sound chips' status ----
local irq_frames = tonumber(os.getenv("DP_IRQ_FRAMES") or "8000")
local irq_file = io.open(out .. "/irq.txt", "w")
local function read_logger(what_of)
	return function(offset, data, mask)
		if irq_file and vblank < irq_frames then
			irq_file:write(string.format("%d %d %s %x\n", vblank, cycle_now(), what_of(offset), data & mask))
		end
		return data
	end
end
if irq_file then
	irq_file:write("; dp_check.lua 0.3.1: MAME " .. emu.app_version() .. ", " .. emu.romname() .. "\n")
	dp_check_irq = space:install_read_tap(0x900000, 0x900007, "dp_check_irq",
		read_logger(function(offset) return "irq" .. ((offset - 0x900000) // 2) end))
	dp_check_oki = space:install_read_tap(0xb00000, 0xb00011, "dp_check_oki",
		read_logger(function(offset) return offset >= 0xb00010 and "oki1" or "oki0" end))
end

-- ---- each write of the random index, for a few VBLANKs (DP_ROUNDS=A-B) ----
local rounds_from, rounds_to = string.match(os.getenv("DP_ROUNDS") or "", "^(%d+)-(%d+)$")
if rounds_from then
	rounds_from, rounds_to = tonumber(rounds_from), tonumber(rounds_to)
	local rounds_file = io.open(out .. "/rounds.txt", "w")
	if rounds_file then
		rounds_file:write("; dp_check.lua 0.3.1: MAME " .. emu.app_version() .. ", " .. emu.romname() .. "\n")
		dp_check_rounds = space:install_write_tap(0x101262, 0x101263, "dp_check_rounds", function(offset, data, mask)
			if vblank >= rounds_from and vblank <= rounds_to then
				rounds_file:write(string.format("%d %d %x\n", vblank, cycle_now(), data & 0xff))
			elseif vblank > rounds_to and rounds_file then
				rounds_file:close()
				rounds_file = nil
			end
		end)
	end
end

set_inputs(0)
if emu.add_machine_frame_notifier then
	dp_check_frames = emu.add_machine_frame_notifier(on_frame)    -- (kept: the subscription ends when collected)
else
	emu.register_frame_done(on_frame)
end
