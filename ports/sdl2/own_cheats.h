/**
 *    Name:        own_cheats.h
 *    Description: the port's own cheats for DonPachi: infinite lives and bombs, invincibility, MAX power and an
 *                 endless chain, for each player; on the CHEATS page with or without MAME's cheat file (whose
 *                 cheats come after them when it is there), and player 2 flown by the computer (ai_player.h).
 *                 And the developers' debug tools, still in the
 *                 program: the debug mode and the RAM editor (the DEBUG TOOLS page)
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 */
#ifndef OWN_CHEATS_H
#define OWN_CHEATS_H

#include <stdbool.h>

/* Adds them to the cheats' table (cave/cheats.h's cheats_add_own): before cheats_load. Their names: the debug tools'
   begin with OWN_CHEATS_DEBUG ("DEBUG "), for the menu's DEBUG TOOLS page (the CHEATS page has the others) */
#define OWN_CHEATS_DEBUG "DEBUG "
void own_cheats_add(void);

/* As the vblank interrupt returns (cave/program.h's program_after_vblank): the debug mode's flag, which the interrupt
   clears as it reads the buttons, set again while the cheat is on */
void own_cheats_after_vblank(void);

/* AI PLAYER 2 is on: the port gives player 2 the computer's inputs (ai_player.h) */
bool own_cheats_ai_on(void);

#endif
