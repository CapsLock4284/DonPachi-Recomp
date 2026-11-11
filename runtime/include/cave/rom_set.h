/**
 *    Name:        rom_set.h
 *    Description: the player's DonPachi ROM set (MAME's 'donpachi'), from its .zip or a folder of its files
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 *    The regions as ROM_START( donpachi ) in MAME's src/mame/cave/cave.cpp lays them out (BSD-3-Clause).
 */
#ifndef CAVE_ROM_SET_H
#define CAVE_ROM_SET_H

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

typedef struct
{
	uint8_t *program;
	uint8_t *sprites;
	uint8_t *layer[3];
	uint8_t *oki[2];
	uint16_t eeprom[64];
} donpachi_rom_set;

extern donpachi_rom_set rom_set;

/* Reads the set from 'source', a .zip or a folder (its files by MAME's names, or the first .zip in it that
   has them): NULL, or what is wrong */
const char *rom_set_load(const char *source);

/* The game's data, extracted: the set as read (its regions, in the order above), kept in one file the program
   reads instead of the ROM set from then on. Never part of the repository (it is the ROMs' contents).
   rom_set_save_data writes it (after rom_set_load); rom_set_load_data reads it, checked by its CRC32: NULL, or
   what is wrong (no file, another program's, damaged) */
const char *rom_set_save_data(const char *path);
const char *rom_set_load_data(const char *path);

#ifdef __cplusplus
}
#endif

#endif
