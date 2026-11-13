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
 */
#include "own_cheats.h"

#include "cave/cheats.h"
#include "cave/machine.h"

#include <stddef.h>

#define GAME_MODE   0x1018ECu
#define PLAYING     7u
#define SHIELD_ON   2u          /* (the shield's frames, kept up: as the test tools' shielded player) */
#define LIVES_KEPT  3u
#define CHAIN_FULL  30u

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
	return word(GAME_MODE) == PLAYING;
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
}
