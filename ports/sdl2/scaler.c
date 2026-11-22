/**
 *    Name:        scaler.c
 *    Description: pixel-art scalers for the game's picture (scaler.h)
 *    Version:     0.2.0 (draft)
 *    Created:     14-11-2026
 *
 *    license:BSD-3-Clause
 *    Each reads the frame through a copy of it with a border of 3 pixels (the edge pixels repeated), so no rule
 *    needs to look out for the frame's edges. The neighbours of a pixel E are named as the algorithms' papers do:
 *
 *              P
 *           A  B  C
 *        Q  D  E  F  R
 *           G  H  I
 *              S
 */
#include "scaler.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define BORDER 3

static const char *const names[SCALERS] = { "OFF", "SCALE2X", "SCALE3X", "XBR 2X", "XBR 3X", "XBR 4X", "MMPX 2X" };
static const char *const keys[SCALERS] = { "off", "scale2x", "scale3x", "xbr2x", "xbr3x", "xbr4x", "mmpx" };
static const int factors[SCALERS] = { 1, 2, 3, 2, 3, 4, 2 };

int scaler_factor(int scaler)
{
	return scaler > SCALER_OFF && scaler < SCALERS ? factors[scaler] : 1;
}

const char *scaler_name(int scaler)
{
	return scaler >= 0 && scaler < SCALERS ? names[scaler] : names[0];
}

const char *scaler_key(int scaler)
{
	return scaler >= 0 && scaler < SCALERS ? keys[scaler] : keys[0];
}

/* ---- the frame with its border ---- */

static uint32_t *padded;     /* (w + 6) x (h + 6) */
static int padded_w, padded_h;

/* the frame copied into the middle of 'padded', its edge pixels repeated out to the border. The pixel at (x, y) of
   the frame is then padded[(y + BORDER) * pw + x + BORDER] */
static bool pad(const uint32_t *in, int w, int h)
{
	const int pw = w + 2 * BORDER, ph = h + 2 * BORDER;

	if (pw != padded_w || ph != padded_h)
	{
		free(padded);
		if (!(padded = malloc((size_t)pw * (size_t)ph * sizeof(uint32_t))))
		{
			padded_w = padded_h = 0;
			return false;
		}
		padded_w = pw;
		padded_h = ph;
	}
	for (int y = 0; y < ph; y++)
	{
		const int sy = y < BORDER ? 0 : y >= h + BORDER ? h - 1 : y - BORDER;
		const uint32_t *src = in + (size_t)sy * (size_t)w;
		uint32_t *row = padded + (size_t)y * (size_t)pw;

		for (int x = 0; x < BORDER; x++)
			row[x] = src[0];
		memcpy(row + BORDER, src, (size_t)w * sizeof(uint32_t));
		for (int x = w + BORDER; x < pw; x++)
			row[x] = src[w - 1];
	}
	return true;
}

/* ---- Scale2x and Scale3x (EPX, as AdvanceMAME has it): a corner takes its two neighbours' colour where they are
   the same and the other two differ ---- */

static void scale2x(int w, int h, uint32_t *out)
{
	const int pw = padded_w, ow = w * 2;

	for (int y = 0; y < h; y++)
	{
		const uint32_t *p = padded + (size_t)(y + BORDER) * (size_t)pw + BORDER;
		uint32_t *o0 = out + (size_t)(y * 2) * (size_t)ow, *o1 = o0 + ow;

		for (int x = 0; x < w; x++, p++)
		{
			const uint32_t B = p[-pw], D = p[-1], E = p[0], F = p[1], H = p[pw];

			if (B != H && D != F)
			{
				o0[2 * x] = D == B ? D : E;
				o0[2 * x + 1] = B == F ? F : E;
				o1[2 * x] = D == H ? D : E;
				o1[2 * x + 1] = H == F ? F : E;
			}
			else
				o0[2 * x] = o0[2 * x + 1] = o1[2 * x] = o1[2 * x + 1] = E;
		}
	}
}

static void scale3x(int w, int h, uint32_t *out)
{
	const int pw = padded_w, ow = w * 3;

	for (int y = 0; y < h; y++)
	{
		const uint32_t *p = padded + (size_t)(y + BORDER) * (size_t)pw + BORDER;
		uint32_t *o0 = out + (size_t)(y * 3) * (size_t)ow, *o1 = o0 + ow, *o2 = o1 + ow;

		for (int x = 0; x < w; x++, p++)
		{
			const uint32_t A = p[-pw - 1], B = p[-pw], C = p[-pw + 1], D = p[-1], E = p[0], F = p[1];
			const uint32_t G = p[pw - 1], H = p[pw], I = p[pw + 1];
			uint32_t *r0 = o0 + 3 * x, *r1 = o1 + 3 * x, *r2 = o2 + 3 * x;

			if (B != H && D != F)
			{
				r0[0] = D == B ? D : E;
				r0[1] = (D == B && E != C) || (B == F && E != A) ? B : E;
				r0[2] = B == F ? F : E;
				r1[0] = (D == B && E != G) || (D == H && E != A) ? D : E;
				r1[1] = E;
				r1[2] = (B == F && E != I) || (H == F && E != C) ? F : E;
				r2[0] = D == H ? D : E;
				r2[1] = (D == H && E != I) || (H == F && E != G) ? H : E;
				r2[2] = H == F ? F : E;
			}
			else
				r0[0] = r0[1] = r0[2] = r1[0] = r1[1] = r1[2] = r2[0] = r2[1] = r2[2] = E;
		}
	}
}

/* ---- xBR (Hyllian's "lv2" rules): at each of a pixel's four corners, an edge is looked for along the diagonal
   (by how much the colours change across it against along it); where there is one, the corner is cut by a line at
   45 degrees, or at 30 or 60 degrees where the edge runs on shallow or steep (two pixels along for one across), and
   the part beyond the line takes the colour of the neighbour on the other side, softened along the line.
   The colours are compared by their brightness (luma) ---- */

#define XBR_EQUAL       80      /* luma (0-255) differences under this are "about the same" (the shaders' 15 of 48) */

static uint8_t *luma;           /* the padded frame's luma, 0-255 */
static int luma_w, luma_h;

/* the four corners, each as the bottom right one turned: a point (x, y) of that corner's own frame (right, down)
   is at (x * ax + y * ay, x * bx + y * by) of the picture's */
typedef struct
{
	int ax, ay, bx, by;
} turning;

static const turning corners[4] = {
	{ 1, 0, 0, 1 },         /* bottom right */
	{ 0, 1, -1, 0 },        /* top right: (x, y) -> (y, -x) */
	{ -1, 0, 0, -1 },       /* top left */
	{ 0, -1, 1, 0 },        /* bottom left: (x, y) -> (-y, x) */
};

/* for each factor (2-4), corner, set of lines (a bit each: 1 the 45 degree inner line, where the edge is only
   nearly there; 2 the 45 degree line; 4 the 30 degree one; 8 the 60 degree one) and sub-pixel of the output: how
   far (0-256) the sub-pixel is past the furthest of those lines, so how much of the colour beyond it it takes */
static uint16_t xbr_weights[5][4][16][16];
static uint16_t xbr_cells[5][4][16];    /* (a bit for each sub-pixel the lines reach: the others stay the pixel's) */
static bool xbr_ready;

static int clamp_256(float v)
{
	return v <= 0.0f ? 0 : v >= 1.0f ? 256 : (int)(v * 256.0f + 0.5f);
}

static void xbr_prepare(void)
{
	for (int factor = 2; factor <= 4; factor++)
	{
		const float delta = 0.5f / (float)factor;    /* (the line softened over one output pixel) */

		for (int corner = 0; corner < 4; corner++)
			for (int sy = 0; sy < factor; sy++)
				for (int sx = 0; sx < factor; sx++)
				{
					/* the sub-pixel's centre, from the pixel's centre, in the corner's own frame (the turning's
					   inverse: a rotation's is its transpose); then 0-1 across the pixel */
					const float u = ((float)sx + 0.5f) / (float)factor - 0.5f, v = ((float)sy + 0.5f) / (float)factor - 0.5f;
					const turning *t = &corners[corner];
					const float x = u * (float)t->ax + v * (float)t->bx + 0.5f, y = u * (float)t->ay + v * (float)t->by + 0.5f;
					const int line[4] = {
						clamp_256((x + y + delta - 1.75f) / (2.0f * delta)),
						clamp_256((x + y + delta - 1.5f) / (2.0f * delta)),
						clamp_256((0.5f * x + y + delta - 1.0f) / (2.0f * delta)),
						clamp_256((2.0f * x + y + delta - 2.0f) / (2.0f * delta)),
					};

					for (int mask = 0; mask < 16; mask++)
					{
						int most = 0;

						for (int l = 0; l < 4; l++)
							if ((mask >> l & 1) && line[l] > most)
								most = line[l];
						xbr_weights[factor][corner][mask][sy * factor + sx] = (uint16_t)most;
						if (most)
							xbr_cells[factor][corner][mask] |= (uint16_t)(1u << (sy * factor + sx));
					}
				}
	}
	xbr_ready = true;
}

static bool make_luma(void)
{
	const size_t count = (size_t)padded_w * (size_t)padded_h;

	if (luma_w != padded_w || luma_h != padded_h)
	{
		free(luma);
		if (!(luma = malloc(count)))
		{
			luma_w = luma_h = 0;
			return false;
		}
		luma_w = padded_w;
		luma_h = padded_h;
	}
	for (size_t i = 0; i < count; i++)
	{
		const uint32_t c = padded[i];

		/* (0.299, 0.587 and 0.114 of 1024) */
		luma[i] = (uint8_t)((306u * ((c >> 16) & 0xFFu) + 601u * ((c >> 8) & 0xFFu) + 117u * (c & 0xFFu) + 512u) >> 10);
	}
	return true;
}

/* a mixed with b: 'w' of 256 of b */
static uint32_t mix(uint32_t a, uint32_t b, unsigned w)
{
	const uint32_t rb = ((a & 0xFF00FFu) * (256u - w) + (b & 0xFF00FFu) * w) >> 8;
	const uint32_t g = ((a & 0x00FF00u) * (256u - w) + (b & 0x00FF00u) * w) >> 8;

	return (a & 0xFF000000u) | (rb & 0xFF00FFu) | (g & 0x00FF00u);
}

/* how far apart two colours are (the sum of their channels' differences) */
static int colour_distance(uint32_t a, uint32_t b)
{
	return abs((int)((a >> 16) & 0xFFu) - (int)((b >> 16) & 0xFFu)) + abs((int)((a >> 8) & 0xFFu) - (int)((b >> 8) & 0xFFu))
		+ abs((int)(a & 0xFFu) - (int)(b & 0xFFu));
}

#define DIFF(a, b)  abs((a) - (b))
#define EQ(a, b)    (DIFF(a, b) < XBR_EQUAL)

/* the neighbours each corner's rules read, as steps through the padded frame, in the corner's own frame:
   B C D F G H I F4 I4 H5 I5 (made when the frame's size is known) */
enum { NB, NC, ND, NF, NG, NH, NI, NF4, NI4, NH5, NI5, NEIGHBOURS };
static int steps[4][NEIGHBOURS];
static int steps_w;

static void make_steps(void)
{
	static const int at[NEIGHBOURS][2] = { { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 },
		{ 2, 0 }, { 2, 1 }, { 0, 2 }, { 1, 2 } };

	for (int corner = 0; corner < 4; corner++)
	{
		const turning *t = &corners[corner];

		for (int n = 0; n < NEIGHBOURS; n++)
		{
			const int x = at[n][0], y = at[n][1];

			steps[corner][n] = (x * t->ax + y * t->ay) + (x * t->bx + y * t->by) * padded_w;
		}
	}
	steps_w = padded_w;
}

/* one corner of the pixel at 'at' (the padded frame's index): its set of lines (0: none), and in 'colour' the
   colour beyond them */
static int xbr_corner(int at, const int *step, uint32_t *colour)
{
	const uint8_t *const l = luma + at;
	const int e = l[0], f = l[step[NF]], h = l[step[NH]];
	int b, c, d, g, i, wd1, wd2, mask;

	if (e == f || e == h)
		return 0;
	b = l[step[NB]];
	c = l[step[NC]];
	d = l[step[ND]];
	g = l[step[NG]];
	i = l[step[NI]];
	{
		const int f4 = l[step[NF4]], i4 = l[step[NI4]], h5 = l[step[NH5]], i5 = l[step[NI5]];

		wd1 = DIFF(e, c) + DIFF(e, g) + DIFF(i, h5) + DIFF(i, f4) + 4 * DIFF(h, f);
		wd2 = DIFF(h, d) + DIFF(h, i5) + DIFF(f, i4) + DIFF(f, b) + 4 * DIFF(e, i);
		if (wd1 > wd2)
			return 0;
		mask = 1;    /* (the inner line: the edge only nearly there, wd1 <= wd2) */
		if (wd1 < wd2 && ((!EQ(f, b) && !EQ(h, d)) || (EQ(e, i) && !EQ(f, i4) && !EQ(h, i5)) || EQ(e, g) || EQ(e, c)))
		{
			const int fg = DIFF(f, g), hc = DIFF(h, c);

			mask |= 2;
			if (2 * fg <= hc && e != g && d != g)
				mask |= 4;
			if (fg >= 2 * hc && e != c && b != c)
				mask |= 8;
		}
	}
	{
		const uint32_t E = padded[at], F = padded[at + step[NF]], H = padded[at + step[NH]];

		*colour = colour_distance(E, F) <= colour_distance(E, H) ? F : H;
	}
	return mask;
}

/* rows 'from' to 'to' (not included) of the picture, its luma made */
static void xbr_rows(int factor, int w, int from, int to, uint32_t *out)
{
	const int pw = padded_w, ow = w * factor;

	for (int y = from; y < to; y++)
		for (int x = 0; x < w; x++)
		{
			const int at = (y + BORDER) * pw + x + BORDER;
			const uint32_t E = padded[at];
			uint32_t *o = out + (size_t)(y * factor) * (size_t)ow + (size_t)(x * factor);
			uint32_t colours[4];
			int masks[4], any = 0;

			unsigned reached = 0;

			for (int corner = 0; corner < 4; corner++)
			{
				any |= masks[corner] = xbr_corner(at, steps[corner], &colours[corner]);
				reached |= xbr_cells[factor][corner][masks[corner]];    /* (mask 0: no bits) */
			}
			for (int sy = 0; sy < factor; sy++)
				for (int sx = 0; sx < factor; sx++)
					o[sy * ow + sx] = E;
			if (!any)
				continue;
			/* (only the sub-pixels a line reaches: the rest are the pixel's own, written above) */
			for (int cell = 0; reached; cell++, reached >>= 1)
			{
				if (!(reached & 1u))
					continue;
				uint32_t result[2];

				/* the bottom right and top left corners together, and the other two: the one that changes the
				   pixel more is kept (two cuts on one sub-pixel are rare: a pixel a line on both sides) */
				for (int pair = 0; pair < 2; pair++)
				{
					uint32_t r = E;

					for (int k = pair; k < 4; k += 2)
						if (masks[k])
						{
							const unsigned weight = xbr_weights[factor][k][masks[k]][cell];

							if (weight)
								r = mix(r, colours[k], weight);
						}
					result[pair] = r;
				}
				o[(cell / factor) * ow + cell % factor] = result[0] == E ? result[1] : result[1] == E ? result[0]
					: colour_distance(E, result[0]) <= colour_distance(E, result[1]) ? result[1] : result[0];
			}
		}
}

/* ---- MMPX (McGuire and Gagiu): Scale2x's corners with rules that keep the pixel art's own look: thin lines,
   sharp corners and single pixels kept, the brighter colour giving way to the darker where two lines cross, and
   steps two pixels long smoothed too. No new colours: every output pixel is one of the input's ---- */

/* the brightness MMPX breaks ties with: the channels' sum, plus one */
static bool layer_planes;    /* (scaler_run_layer: an empty pixel, 0, the brightest) */

static unsigned mmpx_luma(uint32_t c)
{
	if (layer_planes && c == 0u)
		return 0x10000u;
	return ((c >> 16) & 0xFFu) + ((c >> 8) & 0xFFu) + (c & 0xFFu) + 1u;
}

#define ALL_EQ2(b, a0, a1)              ((b) == (a0) && (b) == (a1))
#define ALL_EQ3(b, a0, a1, a2)          (ALL_EQ2(b, a0, a1) && (b) == (a2))
#define ALL_EQ4(b, a0, a1, a2, a3)      (ALL_EQ3(b, a0, a1, a2) && (b) == (a3))
#define ANY_EQ3(b, a0, a1, a2)          ((b) == (a0) || (b) == (a1) || (b) == (a2))
#define NONE_EQ2(b, a0, a1)             ((b) != (a0) && (b) != (a1))
#define NONE_EQ4(b, a0, a1, a2, a3)     (NONE_EQ2(b, a0, a1) && (b) != (a2) && (b) != (a3))

static void mmpx_rows(int w, int from, int to, uint32_t *out)
{
	const int pw = padded_w, ow = w * 2;

	for (int y = from; y < to; y++)
		for (int x = 0; x < w; x++)
		{
			const uint32_t *p = padded + (size_t)(y + BORDER) * (size_t)pw + (size_t)(x + BORDER);
#define SRC(dx, dy)     p[(dy) * pw + (dx)]
			const uint32_t A = SRC(-1, -1), B = SRC(0, -1), C = SRC(1, -1), D = SRC(-1, 0), E = SRC(0, 0);
			const uint32_t F = SRC(1, 0), G = SRC(-1, 1), H = SRC(0, 1), I = SRC(1, 1);
			uint32_t J = E, K = E, L = E, M = E;    /* the output: J K on top, L M under */
			uint32_t *o = out + (size_t)(y * 2) * (size_t)ow + (size_t)(x * 2);

			if (((A ^ E) | (B ^ E) | (C ^ E) | (D ^ E) | (F ^ E) | (G ^ E) | (H ^ E) | (I ^ E)) != 0u)
			{
				const uint32_t P = SRC(0, -2), Q = SRC(-2, 0), R = SRC(2, 0), S = SRC(0, 2);
				const unsigned Bl = mmpx_luma(B), Dl = mmpx_luma(D), El = mmpx_luma(E), Fl = mmpx_luma(F);
				const unsigned Hl = mmpx_luma(H);

				/* 1:1 slopes: a corner cut where its two neighbours match and the centre isn't a sharp corner */
				if ((D == B && D != H && D != F) && (El >= Dl || E == A) && ANY_EQ3(E, A, C, G)
						&& (El < Dl || A != D || E != P || E != Q))
					J = D;
				if ((B == F && B != D && B != H) && (El >= Bl || E == C) && ANY_EQ3(E, A, C, I)
						&& (El < Bl || C != B || E != P || E != R))
					K = B;
				if ((H == D && H != F && H != B) && (El >= Hl || E == G) && ANY_EQ3(E, A, G, I)
						&& (El < Hl || G != H || E != S || E != Q))
					L = H;
				if ((F == H && F != B && F != D) && (El >= Fl || E == I) && ANY_EQ3(E, C, G, I)
						&& (El < Fl || I != H || E != R || E != S))
					M = F;

				/* where two diagonal lines cross */
				if (E != F && ALL_EQ4(E, C, I, D, Q) && ALL_EQ2(F, B, H) && F != SRC(3, 0))
					K = M = F;
				if (E != D && ALL_EQ4(E, A, G, F, R) && ALL_EQ2(D, B, H) && D != SRC(-3, 0))
					J = L = D;
				if (E != H && ALL_EQ4(E, G, I, B, P) && ALL_EQ2(H, D, F) && H != SRC(0, 3))
					L = M = H;
				if (E != B && ALL_EQ4(E, A, C, H, S) && ALL_EQ2(B, D, F) && B != SRC(0, -3))
					J = K = B;

				/* the tips of bright triangles on a dark ground */
				if (Bl < El && ALL_EQ4(E, G, H, I, S) && NONE_EQ4(E, A, D, C, F))
					J = K = B;
				if (Hl < El && ALL_EQ4(E, A, B, C, P) && NONE_EQ4(E, D, G, I, F))
					L = M = H;
				if (Fl < El && ALL_EQ4(E, A, D, G, Q) && NONE_EQ4(E, B, C, I, H))
					K = M = F;
				if (Dl < El && ALL_EQ4(E, C, F, I, R) && NONE_EQ4(E, B, A, G, H))
					J = L = D;

				/* 2:1 slopes: a step two pixels long, the cut carried on by one */
				if (H != B)
				{
					if (H != A && H != E && H != C)
					{
						if (ALL_EQ3(H, G, F, R) && NONE_EQ2(H, D, SRC(2, -1)))
							L = M;
						if (ALL_EQ3(H, I, D, Q) && NONE_EQ2(H, F, SRC(-2, -1)))
							M = L;
					}
					if (B != I && B != G && B != E)
					{
						if (ALL_EQ3(B, A, F, R) && NONE_EQ2(B, D, SRC(2, 1)))
							J = K;
						if (ALL_EQ3(B, C, D, Q) && NONE_EQ2(B, F, SRC(-2, 1)))
							K = J;
					}
				}
				if (F != D)
				{
					if (D != I && D != E && D != C)
					{
						if (ALL_EQ3(D, A, H, S) && NONE_EQ2(D, B, SRC(1, 2)))
							J = L;
						if (ALL_EQ3(D, G, B, P) && NONE_EQ2(D, H, SRC(1, -2)))
							L = J;
					}
					if (F != E && F != A && F != G)
					{
						if (ALL_EQ3(F, C, H, S) && NONE_EQ2(F, B, SRC(-1, 2)))
							K = M;
						if (ALL_EQ3(F, I, B, P) && NONE_EQ2(F, H, SRC(-1, -2)))
							M = K;
					}
				}
			}
#undef SRC
			o[0] = J;
			o[1] = K;
			o[ow] = L;
			o[ow + 1] = M;
		}
}

/* ---- the work shared out: xBR and MMPX take a few milliseconds a frame on one core, so the picture's rows are
   split between up to 8 (this thread and helpers that wait for each frame, one fewer than the PC's cores) ---- */

#define HELPERS_MOST 7

typedef struct
{
	SDL_Thread *thread;
	SDL_sem *go, *done;
	int scaler, w, from, to;
	uint32_t *out;
} helper;

static helper helpers[HELPERS_MOST];
static int helper_count = -1;    /* (-1: not started yet) */

static void run_rows(int scaler, int w, int from, int to, uint32_t *out)
{
	if (scaler == SCALER_MMPX)
		mmpx_rows(w, from, to, out);
	else
		xbr_rows(factors[scaler], w, from, to, out);
}

static int helper_main(void *data)
{
	helper *const self = data;

	for (;;)
	{
		SDL_SemWait(self->go);
		if (self->scaler < 0)
			return 0;
		run_rows(self->scaler, self->w, self->from, self->to, self->out);
		SDL_SemPost(self->done);
	}
}

static void start_helpers(void)
{
	const int cores = SDL_GetCPUCount();

	helper_count = 0;
	for (int i = 0; i < HELPERS_MOST && i < cores - 1; i++)
	{
		helper *const h = &helpers[helper_count];

		if (!(h->go = SDL_CreateSemaphore(0)) || !(h->done = SDL_CreateSemaphore(0))
				|| !(h->thread = SDL_CreateThread(helper_main, "scaler", h)))
			break;    /* (fewer helpers: the rest of the work stays on this thread) */
		helper_count++;
	}
}

static void shared_out(int scaler, int w, int h, uint32_t *out)
{
	int parts, from = 0;

	if (helper_count < 0)
		start_helpers();
	parts = helper_count + 1;
	for (int i = 0; i < helper_count; i++)
	{
		helper *const helper_i = &helpers[i];
		const int to = h * (i + 1) / parts;

		helper_i->scaler = scaler;
		helper_i->w = w;
		helper_i->from = from;
		helper_i->to = to;
		helper_i->out = out;
		SDL_SemPost(helper_i->go);
		from = to;
	}
	run_rows(scaler, w, from, h, out);
	for (int i = 0; i < helper_count; i++)
		SDL_SemWait(helpers[i].done);
}

void scaler_quit(void)
{
	for (int i = 0; i < helper_count; i++)
	{
		helpers[i].scaler = -1;
		SDL_SemPost(helpers[i].go);
		SDL_WaitThread(helpers[i].thread, NULL);
		SDL_DestroySemaphore(helpers[i].go);
		SDL_DestroySemaphore(helpers[i].done);
	}
	helper_count = -1;
	free(padded);
	free(luma);
	padded = NULL;
	luma = NULL;
	padded_w = padded_h = luma_w = luma_h = steps_w = 0;
}

bool scaler_by_layer(int scaler)
{
	return scaler == SCALER_SCALE2X || scaler == SCALER_SCALE3X || scaler == SCALER_MMPX;
}

void scaler_run_layer(int scaler, const uint32_t *in, int w, int h, uint32_t *out)
{
	layer_planes = true;
	scaler_run(scaler, in, w, h, out);
	layer_planes = false;
}

void scaler_run(int scaler, const uint32_t *in, int w, int h, uint32_t *out)
{
	if (scaler <= SCALER_OFF || scaler >= SCALERS || !pad(in, w, h))
	{
		memcpy(out, in, (size_t)w * (size_t)h * sizeof(uint32_t));
		return;
	}
	switch (scaler)
	{
	case SCALER_SCALE2X:
		scale2x(w, h, out);
		break;
	case SCALER_SCALE3X:
		scale3x(w, h, out);
		break;
	case SCALER_MMPX:
		shared_out(scaler, w, h, out);
		break;
	default:
		if (!xbr_ready)
			xbr_prepare();
		if (steps_w != padded_w)
			make_steps();
		if (make_luma())
			shared_out(scaler, w, h, out);
		break;
	}
}
