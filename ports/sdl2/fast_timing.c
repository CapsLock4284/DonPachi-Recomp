/**
 *    Name:        fast_timing.c
 *    Description: the picture with fast timing, as the board shows it (fast_timing.h)
 *    Version:     0.1.0 (draft)
 *    Created:     13-11-2026
 *
 *    license:BSD-3-Clause
 *    With fast timing the port draws the picture as soon as the program's frame is done, a frame before the board
 *    would show it, the sprites latched as the VBLANK comes (the sprite chip's way). The vblank interrupt that
 *    follows (level_1_interrupt at 0x0005EE), when the program has made its frame ready ($1011FC bit 0), sets each
 *    layer's scroll, the frame's value plus the screen's shake:
 *      layer 0 ($700000 / $700002): the words at $1013A8 / $1013AA, plus the shake entry's first two words (the entry
 *        at the address in $1013CC); layer 1 ($600000 / $600002): $1013AC / $1013AE plus the next two; layer 2
 *        ($800000 / $800002): $1013B0 / $1013B2 plus the next two
 *    and copies the palette's changed colours: for each quarter of it (0x400 bytes), the long word at $101724 + 4 x
 *    the quarter has a bit for each 32 bytes changed (bit 31: the last), copied from $1088F0 + the same place.
 *    Drawn without those, the background was a frame behind the sprites (about a pixel of the ground's scroll) and
 *    a palette's change a frame late. The interrupt's writes are all done within its first 32,000 cycles, inside the
 *    board's vblank (about 31 lines, 2 ms): with arcade timing the picture is drawn as it returns, the same picture.
 */
#include "fast_timing.h"

#include "cave/machine.h"

#include <string.h>

#define FRAME_READY     0x1011FCu
#define SCROLL          0x1013A8u
#define SHAKE           0x1013CCu
#define PALETTE_CHANGED 0x101724u
#define PALETTE_BUFFER  0x1088F0u

static uint16_t layer_regs[3][3], palette[CAVE_PALETTE_WORDS];
static bool changed;

static unsigned word(uint32_t address)
{
	return cave_read16(address, 0xFFFFu);
}

void fast_timing_begin(void)
{
	/* (the layers in the interrupt's order: 0 at $700000, 1 at $600000, 2 at $800000; the board's layer_regs too) */
	uint32_t shake;

	changed = (word(FRAME_READY) & 1u) != 0;
	if (!changed)
		return;
	memcpy(layer_regs, cave_board.layer_regs, sizeof(layer_regs));
	memcpy(palette, cave_board.palette, sizeof(palette));
	shake = (uint32_t)word(SHAKE) << 16 | word(SHAKE + 2u);
	for (unsigned layer = 0; layer < 3; layer++)
		for (unsigned axis = 0; axis < 2; axis++)
			cave_board.layer_regs[layer][axis] = (uint16_t)(word(SCROLL + 4u * layer + 2u * axis)
				+ word(shake + 4u * layer + 2u * axis));
	for (unsigned quarter = 0; quarter < 4; quarter++)
	{
		const uint32_t mask = (uint32_t)word(PALETTE_CHANGED + 4u * quarter) << 16
			| word(PALETTE_CHANGED + 4u * quarter + 2u);

		for (unsigned bit = 0; bit < 32; bit++)
			if (mask & (1u << bit))
				for (unsigned w = 0; w < 16; w++)
				{
					const uint32_t at = 0x400u * quarter + 32u * bit + 2u * w;

					cave_board.palette[at >> 1] = (uint16_t)word(PALETTE_BUFFER + at);
				}
	}
}

void fast_timing_end(void)
{
	if (!changed)
		return;
	memcpy(cave_board.layer_regs, layer_regs, sizeof(layer_regs));
	memcpy(cave_board.palette, palette, sizeof(palette));
	changed = false;
}
