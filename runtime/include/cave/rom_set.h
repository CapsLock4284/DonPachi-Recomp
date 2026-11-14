/**
 *    Name:        rom_set.h
 *    Description: the player's DonPachi ROM set, from its .zip or a folder of its files: MAME's 'donpachi' (US,
 *                 Ver. 1.12) or 'donpachij' (Japan, Ver. 1.01: its own program and text chips, prg.u29 and u58.bin,
 *                 the others the US set's)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The regions as ROM_START( donpachi ) and ROM_START( donpachij ) in MAME's src/mame/cave/cave.cpp lay them out
 *    (BSD-3-Clause).
 */
#ifndef CAVE_ROM_SET_H
#define CAVE_ROM_SET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DONPACHI_PROGRAM_SIZE  0x080000u    /* big endian, as the 68000 sees it */
#define DONPACHI_SPRITES_SIZE  0x400000u    /* word swapped into the board's order */
#define DONPACHI_LAYER01_SIZE  0x100000u    /* layers 0 and 1: 8x8 tiles of 4 bits, packed */
#define DONPACHI_LAYER2_SIZE   0x040000u    /* layer 2, the text */
#define DONPACHI_OKI1_SIZE     0x240000u    /* the first 0x40000 bytes empty (the NMK112 banks them) */
#define DONPACHI_OKI2_SIZE     0x340000u

/* The versions: MAME's sets */
typedef enum
{
	DONPACHI_US,        /* 'donpachi', Ver. 1.12 1995/05/2x */
	DONPACHI_JAPAN,     /* 'donpachij', Ver. 1.01 1995/05/11 */
	DONPACHI_VERSIONS
} donpachi_version;

typedef struct
{
	donpachi_version version;
	uint8_t *program;
	uint8_t *sprites;
	uint8_t *layer[3];
	uint8_t *oki[2];
	uint16_t eeprom[64];
} donpachi_rom_set;

extern donpachi_rom_set rom_set;

/* Reads the US set from 'source', a .zip or a folder (its files by MAME's names, or donpachi.zip in it): NULL, or
   what is wrong. rom_set_load_version: the version's (the Japanese: from 'source', its loose files, donpachij.zip and
   donpachi.zip in the folder, or a .zip and donpachi.zip beside it: MAME's clone set leaves the shared files to its
   parent). Each file is known by its CRC32 and size, whatever its name or the .zip it is in (merged sets too) */
const char *rom_set_load(const char *source);
const char *rom_set_load_version(const char *source, donpachi_version version);

/* The Japanese set over the US one now loaded: its own program and text (prg.u29, u58.bin) read from 'source' (a
   .zip, donpachij.zip alone will do, or a folder: its loose files or its .zips), the US set's other regions kept (the
   chips the two share): NULL, or what is wrong (no US set loaded; the two files not there: rom_set_files_missing
   names them). So the Japanese version needs no more than donpachij.zip once the US set's data is there */
const char *rom_set_load_japan_over(const char *source);

/* The regions freed (rom_set empty again: another set to load) */
void rom_set_free(void);

/* The version's name for the screen ("USA", "JAPAN") and MAME's set ("donpachi", "donpachij") */
const char *rom_set_version_name(donpachi_version version);
const char *rom_set_mame_name(donpachi_version version);

#define ROM_SET_FILES 9    /* the files of the set (MAME's names) */

/* After rom_set_load (of the version it was asked for): how many of the set's files it found, and the names of
   those it did not (at most 'most' in 'names'; 'different': a file of that name was there, but not the right one:
   another version, or damaged): how many it did not find */
int rom_set_files_found(void);
int rom_set_files_missing(const char **names, bool *different, int most);

/* The game's data, extracted: the set as read (its regions, in the order above), kept in one file the program
   reads instead of the ROM set from then on. Never part of the repository (it is the ROMs' contents).
   rom_set_save_data writes it (after rom_set_load: its version in the header); rom_set_load_data reads it, checked by
   its CRC32 and that it is the US set's (rom_set_load_data_version: the version's): NULL, or what is wrong (no file,
   another program's or version's, damaged) */
const char *rom_set_save_data(const char *path);
const char *rom_set_load_data(const char *path);
const char *rom_set_load_data_version(const char *path, donpachi_version version);

#ifdef __cplusplus
}
#endif

#endif
