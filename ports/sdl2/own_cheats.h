/**
 *    Name:        own_cheats.h
 *    Description: the port's own cheats for DonPachi: infinite lives and bombs, invincibility, MAX power and an
 *                 endless chain, for each player; on the CHEATS page with or without MAME's cheat file (whose
 *                 cheats come after them when it is there)
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 */
#ifndef OWN_CHEATS_H
#define OWN_CHEATS_H

/* Adds them to the cheats' table (cave/cheats.h's cheats_add_own): before cheats_load */
void own_cheats_add(void);

#endif
