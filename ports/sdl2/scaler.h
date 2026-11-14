/**
 *    Name:        scaler.h
 *    Description: pixel-art scalers for the game's picture ([video] filter): Scale2x and Scale3x, xBR at 2x, 3x and
 *                 4x, MMPX at 2x. Each makes a larger copy of the 240 x 320 frame whose edges are smoothed while its
 *                 pixels stay sharp; the window then shows that copy as it shows the plain picture
 *    Version:     0.1.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 *    SDL2's renderer has no shaders, so they run on the processor, over the finished frame just before it is shown
 *    (a few milliseconds at most: no frame later; xBR and MMPX share their rows out between up to 4 cores). Our own code, after the published algorithms: Scale2x / Scale3x
 *    (AdvanceMAME's EPX rules), xBR (Hyllian's edge rules, "lv2": his shaders are under the MIT license) and MMPX
 *    (McGuire and Gagiu, 2021, the paper's rules; their code is under the MIT license). See NOTICES.txt
 */
#ifndef SCALER_H
#define SCALER_H

#include <stdint.h>

enum
{
	SCALER_OFF,
	SCALER_SCALE2X,
	SCALER_SCALE3X,
	SCALER_XBR2X,
	SCALER_XBR3X,
	SCALER_XBR4X,
	SCALER_MMPX,
	SCALERS
};

/* how many times larger a scaler makes the picture (1 for SCALER_OFF) */
int scaler_factor(int scaler);

/* the scaler's name, as the settings file and the menu have it ("OFF", "SCALE2X", "XBR 4X", ...) */
const char *scaler_name(int scaler);

/* its key in the settings file ("off", "scale2x", "xbr4x", "mmpx", ...) */
const char *scaler_key(int scaler);

/* 'in' (w x h pixels, ARGB) made 'scaler_factor' times larger into 'out' (its rows w * factor pixels long) */
void scaler_run(int scaler, const uint32_t *in, int w, int h, uint32_t *out);

/* Stops its helper threads and lets go of its memory (the program ending) */
void scaler_quit(void);

#endif
