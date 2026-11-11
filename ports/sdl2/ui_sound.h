/**
 *    Name:        ui_sound.h
 *    Description: the port's own sounds (the menus' and the achievement pop-up's), borrowed from the game's: samples
 *                 of its effects chip, decoded from the ROM set's data and mixed over the game's sound
 *    Version:     0.1.0 (draft)
 *    Created:     11-11-2026
 *
 *    license:BSD-3-Clause
 *    The samples are the effects chip's (the second M6295's ROM region, as rom_set.c keeps it): found by the sounds
 *    the game plays (tests/harness/sfx_log): the ship select's cursor and choice, and the power-up item's chime.
 *    Decoded as the chip does (OKI ADPCM at 4.220 MHz / 2 / 132), at the game's own volume for each, to the port's
 *    48 kHz. One plays at a time: a new one cuts the last.
 */
#ifndef DONPACHI_UI_SOUND_H
#define DONPACHI_UI_SOUND_H

#include <stdint.h>

typedef enum
{
	UI_SOUND_MOVE,          /* the menu's cursor moved, or a value changed */
	UI_SOUND_CHOOSE,        /* an entry chosen */
	UI_SOUND_BACK,          /* back out of a page, or the menu closed */
	UI_SOUND_ACHIEVEMENT,   /* the achievement pop-up */
	UI_SOUNDS
} ui_sound;

void ui_sound_play(ui_sound sound);

/* The sound playing mixed into 'count' samples of the game's (48 kHz mono), its samples taken */
void ui_sound_mix(int16_t *samples, unsigned count);

/* A sound is playing (the menu, with the game paused, queues it with silence) */
int ui_sound_playing(void);

#endif
