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

#ifdef __cplusplus
extern "C" {
#endif

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
