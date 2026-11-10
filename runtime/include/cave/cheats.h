/**
 *    Name:        cheats.h
 *    Description: MAME's cheats for the game (a cheat file of MAME's format, cheats/donpachi.xml), the kind that
 *                 write the work RAM: chosen by name, applied after every frame
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    What is read of MAME's format: a <cheat desc="..."> with <script state="run"> (every frame) or state="on"
 *    (once, when it starts) whose <action>s write the RAM, maincpu.pb@ADDRESS=VALUE (pb a byte, pw a word, pd a
 *    long, hexadecimal), VALUE a number or "param" (a <parameter>'s <item value="...">). A cheat that patches the
 *    program (maincpu.mb, mw, md, mq: the ROM's code, which the decompiled game does not run) or uses MAME's
 *    expressions is refused, with a message.
 */
#ifndef CAVE_CHEATS_H
#define CAVE_CHEATS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The cheat file's cheats, read (for the menu: all off): NULL, or what is wrong. cheats_enable reads it too */
const char *cheats_load(const char *path);

/* Its cheats: their names; usable (they only write the RAM, and need no value: the menu offers those); on */
int cheats_count(void);
const char *cheats_name(int index);
bool cheats_usable(int index);
bool cheats_on(int index);
void cheats_set(int index, bool on);
bool cheats_any_on(void);
void cheats_off(void);

/* Turns on the cheat whose name holds 'request' (any case; "NAME=VALUE" picks a parameter's value) from the cheat
   file 'path': NULL, or what is wrong */
const char *cheats_enable(const char *path, const char *request);

/* Lists the file's cheats on the standard output */
void cheats_list(const char *path);

/* The cheats turned on: their writes, after a frame */
void cheats_apply(void);

#ifdef __cplusplus
}
#endif

#endif
