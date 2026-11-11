/**
 *    Name:        window_icon.h
 *    Description: the program's icon on a window (the title bar, the taskbar): icon_pixels.h, 32 x 32
 *    Version:     0.1.0 (draft)
 *    Created:     10-11-2026
 *
 *    license:BSD-3-Clause
 *    The art is the program's own (tools/make_icon.py); on Windows the .exe carries it too (icon/donpachi.rc).
 */
#ifndef DONPACHI_WINDOW_ICON_H
#define DONPACHI_WINDOW_ICON_H

#include "icon_pixels.h"

#include <SDL.h>

static inline void window_icon_set(SDL_Window *window)
{
	SDL_Surface *const icon = SDL_CreateRGBSurfaceFrom((void *)icon_pixels, 32, 32, 32, 32 * 4,
		0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);

	if (icon)
	{
		SDL_SetWindowIcon(window, icon);
		SDL_FreeSurface(icon);
	}
}

#endif
