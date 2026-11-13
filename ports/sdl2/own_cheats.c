/**
 *    Name:        own_cheats.c
 *    Description: the port's own cheats for DonPachi (own_cheats.h): on the CHEATS page without MAME's cheat file
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 *    Written from what the port knows of the game's memory (training.c, stats.c, widgets.c, TODO.md), each for a
 *    player in play (the game's mode 7), after every frame (cave/cheats.h's cheats_apply):
 *      $1018EC the game's mode; $101A2F / $101A31 the lives (bytes: the one in play counted)
 *      $102158 / $102198 the ships' records: +0 bit 15 flying, bit 10 at MAX power; +$21 (byte) the power, 0-4;
 *        +$22 (word) the bombs; +$3C (word) the power-ups toward the next level; +$3E (byte) the shield, the frames
 *        the ship can't be hit (a respawn's)
 *      $101A86 / $101A88 1 at MAX power (a power-up then gives the bonus); $101A8A / $101A8C (bytes) the bomb slots
 *      $101742 / $101760 the chain's timer: 30 after a kill, the chain broken at 0
 *    The debug tools (sudden-desu.net, "DonPachi debug tools and more"): the vblank interrupt reads the buttons into
 *    $101398 / $101399 (the high byte: player 1's buttons in bits 0-2, player 2's in 3-5) and clears bit 2; the
 *    program's debug mode is that bit set (the released game never has it): the main loop's pause and stage
 *    advance, the palette editor, object spawner and stage select ($035D78), the ship not shot down ($02BBF2). It is
 *    set again as the interrupt returns, while in play. $1018E8 = 1: the RAM editor
 */
#include "own_cheats.h"
#include "game_mode.h"

#include "cave/cheats.h"
#include "cave/machine.h"

#include <stddef.h>

#define SHIELD_ON   2u          /* (the shield's frames, kept up: as the test tools' shielded player) */
#define LIVES_KEPT  3u
#define CHAIN_FULL  30u
#define BUTTONS     0x101398u   /* the buttons' byte the interrupt makes: bit 2, the debug mode */
#define RAM_EDITOR  0x1018E8u

static const uint32_t records[2] = { 0x102158u, 0x102198u };
static const uint32_t lives_at[2] = { 0x101A2Fu, 0x101A31u };
static const uint32_t slots_at[2] = { 0x101A8Au, 0x101A8Cu };
static const uint32_t max_at[2] = { 0x101A86u, 0x101A88u };
static const uint32_t chain_timers[2] = { 0x101742u, 0x101760u };

static unsigned word(uint32_t address)
{
	return *(const uint16_t *)cave_work_ram_at(address, 2);
}

static void set_word(uint32_t address, unsigned value)
{
	*(uint16_t *)cave_work_ram_at(address, 2) = (uint16_t)value;
}

static unsigned byte(uint32_t address)
{
	return *cave_work_ram_byte(address);
}

static void set_byte(uint32_t address, unsigned value)
{
	*cave_work_ram_byte(address) = (uint8_t)value;
}

static bool playing(void)
{
	return game_mode_playing();    /* (one player or both) */
}

static bool flying(int player)
{
	return (word(records[player]) & 0x8000u) != 0;
}

/* the lives kept at 3 (a player with none is not in the game: left alone) */
static void lives(int player)
{
	const unsigned now = byte(lives_at[player]);

	if (playing() && now > 0u && now < LIVES_KEPT)
		set_byte(lives_at[player], LIVES_KEPT);
}

/* the bombs kept full: as many as the bomb slots */
static void bombs(int player)
{
	const unsigned slots = byte(slots_at[player]);

	if (playing() && flying(player) && word(records[player] + 0x22u) < slots)
		set_word(records[player] + 0x22u, slots);
}

/* the respawn's shield kept up: nothing hits the ship */
static void invincible(int player)
{
	if (playing() && flying(player) && byte(records[player] + 0x3Eu) < SHIELD_ON)
		set_byte(records[player] + 0x3Eu, SHIELD_ON);
}

/* MAX power, as the game's power-ups make it (training.c's way) */
static void max_power(int player)
{
	if (!playing() || !flying(player))
		return;
	set_byte(records[player] + 0x21u, 4u);
	set_word(records[player] + 0x3Cu, 0u);
	set_word(max_at[player], 1u);
	set_word(records[player], word(records[player]) | 0x400u);
}

/* the chain's timer kept full while a chain is going: it never breaks */
static void chain(int player)
{
	const unsigned timer = word(chain_timers[player]);

	if (playing() && timer > 0u && timer < CHAIN_FULL)
		set_word(chain_timers[player], CHAIN_FULL);
}

/* the debug tools: the debug mode's bit is set as the interrupt returns (own_cheats_after_vblank); the RAM editor's
   flag after every frame, and cleared once as it is turned off */
static int debug_mode_index = -1, ram_editor_index = -1, ai_index = -1;
static bool ram_editor_was_on;

static void debug_mode(void)
{
}

static void ram_editor(void)
{
	set_byte(RAM_EDITOR, 1u);
}

void own_cheats_after_vblank(void)
{
	if (debug_mode_index >= 0 && cheats_on(debug_mode_index) && playing())
		set_byte(BUTTONS, byte(BUTTONS) | 4u);
	if (ram_editor_index >= 0)
	{
		const bool on = cheats_on(ram_editor_index);

		if (!on && ram_editor_was_on)
			set_byte(RAM_EDITOR, 0u);
		ram_editor_was_on = on;
	}
}

/* the computer's player 2 (ai_player.h): its inputs are the port's, before each frame (own_cheats_ai_on) */
static void ai_player(void)
{
}

bool own_cheats_ai_on(void)
{
	return ai_index >= 0 && cheats_on(ai_index);
}

static void p1_lives(void) { lives(0); }
static void p2_lives(void) { lives(1); }
static void p1_bombs(void) { bombs(0); }
static void p2_bombs(void) { bombs(1); }
static void p1_invincible(void) { invincible(0); }
static void p2_invincible(void) { invincible(1); }
static void p1_max_power(void) { max_power(0); }
static void p2_max_power(void) { max_power(1); }
static void p1_chain(void) { chain(0); }
static void p2_chain(void) { chain(1); }

void own_cheats_add(void)
{
	static const struct
	{
		const char *name, *help;
		cheats_effect effect;
	} list[] = {
		{ "AI PLAYER 2", "PLAYER 2 FLOWN BY THE COMPUTER: IT JOINS BY ITSELF, DODGES, SHOOTS AND BOMBS WHEN "
			"CORNERED. THE P2 CHEATS WORK FOR IT.", ai_player },
		{ "P1 INFINITE LIVES", "PLAYER 1'S LIVES STAY AT 3.", p1_lives },
		{ "P1 INFINITE BOMBS", "PLAYER 1'S BOMBS REFILL AS SOON AS ONE IS USED.", p1_bombs },
		{ "P1 INVINCIBLE", "NOTHING CAN HIT PLAYER 1'S SHIP (ITS RESPAWN SHIELD STAYS ON).", p1_invincible },
		{ "P1 MAX POWER", "PLAYER 1'S SHOT AT FULL POWER (MAX).", p1_max_power },
		{ "P1 ENDLESS CHAIN", "PLAYER 1'S CHAIN (GET POINT) NEVER RUNS OUT ONCE STARTED.", p1_chain },
		{ "P2 INFINITE LIVES", "PLAYER 2'S LIVES STAY AT 3.", p2_lives },
		{ "P2 INFINITE BOMBS", "PLAYER 2'S BOMBS REFILL AS SOON AS ONE IS USED.", p2_bombs },
		{ "P2 INVINCIBLE", "NOTHING CAN HIT PLAYER 2'S SHIP (ITS RESPAWN SHIELD STAYS ON).", p2_invincible },
		{ "P2 MAX POWER", "PLAYER 2'S SHOT AT FULL POWER (MAX).", p2_max_power },
		{ "P2 ENDLESS CHAIN", "PLAYER 2'S CHAIN (GET POINT) NEVER RUNS OUT ONCE STARTED.", p2_chain },
	};

	for (size_t index = 0; index < sizeof(list) / sizeof(list[0]); index++)
		cheats_add_own(list[index].name, list[index].help, list[index].effect);
	ai_index = 0;
	/* (the table's own cheats are first, in this order: their indexes) */
	debug_mode_index = (int)(sizeof(list) / sizeof(list[0]));
	cheats_add_own(OWN_CHEATS_DEBUG "MODE", "THE DEVELOPERS' DEBUG MODE, IN PLAY: PAUSE, MOVING THROUGH A STAGE, "
		"STAGE SELECT, OBJECT SPAWNER, PALETTE EDITOR. THE SHIP CAN'T BE SHOT DOWN. THE CONTROLS: ON THE LEFT.",
		debug_mode);
	ram_editor_index = debug_mode_index + 1;
	cheats_add_own(OWN_CHEATS_DEBUG "RAM EDITOR", "THE DEVELOPERS' MEMORY VIEWER AND EDITOR, AT THE BOTTOM OF THE "
		"SCREEN. THE CONTROLS: ON THE LEFT.", ram_editor);
}
