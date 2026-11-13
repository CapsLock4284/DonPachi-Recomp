/**
 *    Name:        crt.h
 *    Description: the arcade monitor's look for the game's picture: a curved tube, its lines, its phosphor stripes,
 *                 their glow and the darker corners ([video] crt: off, light or full)
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 *    SDL2's renderer has no shaders, so it is done with what every renderer has (Direct3D, OpenGL, Metal, the
 *    software one; SDL 2.0.18 or later): the picture, a blurred copy of it added for the glow, and a pattern of the
 *    lines, the stripes and the corners multiplied over them, drawn flat into a texture of the picture's size on the
 *    screen; then that texture drawn as a curved mesh of triangles (SDL_RenderGeometry), turned with the picture.
 */
#ifndef CRT_H
#define CRT_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

enum { CRT_OFF, CRT_LIGHT, CRT_FULL, CRT_LEVELS };

/* Draws the picture ('picture' the texture of 'pixels', the game's 240 x 320 frame as the port shows it, upright)
   on the renderer's output with the monitor's look: 'unturned' the picture's rectangle on the screen before it is
   turned, 'turn' 0, 90 or 270 (degrees clockwise, about the rectangle's centre), 'level' CRT_LIGHT or CRT_FULL.
   False when the renderer can't (no render targets or geometry): then nothing is drawn, and the caller draws the
   plain picture */
bool crt_draw(SDL_Renderer *renderer, SDL_Texture *picture, const uint32_t *pixels, SDL_FRect unturned, int turn,
	int level);

/* Lets go of its textures (the renderer going, or its targets lost): they are made again as they are needed */
void crt_reset(void);

#endif
