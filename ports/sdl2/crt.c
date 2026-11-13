/**
 *    Name:        crt.c
 *    Description: the arcade monitor's look (crt.h)
 *    Version:     0.1.0 (draft)
 *    Created:     12-11-2026
 *
 *    license:BSD-3-Clause
 *    The picture is 240 x 320 (the board's 320 x 240, its monitor standing on its side in the cabinet): the
 *    monitor's lines are the picture's columns, so they run up and down it, and its phosphor stripes (an aperture
 *    grille's red, green and blue) across it. All of it is laid out as the picture shows on the screen, turned with
 *    it (the mesh itself is never turned: SDL's software renderer draws a turned one with holes).
 */
#include "crt.h"

#include "cave/video.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PICTURE_W   VIDEO_HEIGHT    /* 240: the picture's width, the monitor's lines */
#define PICTURE_H   VIDEO_WIDTH     /* 320 */
#define MESH_X      24              /* the curved mesh's squares across and down */
#define MESH_Y      32

/* a level's look */
typedef struct
{
	float curve;        /* the tube's curve: how far the corners are pulled in (as a part of the half width) */
	float lines;        /* how dark the gaps between the lines are (0-1) */
	float stripes;      /* how much of the other two colours each phosphor stripe holds back (0-1) */
	float corners;      /* how dark the corners get (0-1) */
	Uint8 boost;        /* the picture made brighter by this much of itself (0-255): the light the pattern takes back */
	Uint8 glow;         /* the blurred copy added, this much of it (0-255) */
} look;

static const look looks[CRT_LEVELS] = {
	{ 0 },
	{ 0.015f, 0.35f, 0.12f, 0.10f, 60, 32 },     /* light */
	{ 0.035f, 0.55f, 0.25f, 0.22f, 120, 56 },    /* full */
};

static SDL_Texture *target, *pattern, *glow, *stripes, *bright;
static int target_w, target_h, target_turn, pattern_level;
static int stripes_w, stripes_h, stripes_turn, stripes_level;
static uint32_t glow_pixels[PICTURE_W * PICTURE_H], bright_pixels[PICTURE_W * PICTURE_H];

void crt_reset(void)
{
	if (target)
		SDL_DestroyTexture(target);
	if (pattern)
		SDL_DestroyTexture(pattern);
	if (glow)
		SDL_DestroyTexture(glow);
	if (stripes)
		SDL_DestroyTexture(stripes);
	if (bright)
		SDL_DestroyTexture(bright);
	target = pattern = glow = stripes = bright = NULL;
	target_w = target_h = target_turn = stripes_w = stripes_h = 0;
	pattern_level = stripes_level = CRT_OFF;
}

/* The pattern multiplied over the flat picture, w x h as it shows on the screen: the lines (240 bands across the
   upright picture, down it turned: bright in their middles, dark at their edges; only when a band is 2 pixels or
   more), the corners darker. Curved with it */
static bool make_pattern(SDL_Renderer *renderer, int w, int h, bool turned, const look *l)
{
	uint32_t *pixels = malloc((size_t)w * (size_t)h * sizeof(uint32_t));
	const int length = turned ? h : w;
	const float band = (float)length / PICTURE_W;
	const float lines = band >= 2.0f ? l->lines * (band >= 3.0f ? 1.0f : band - 2.0f) : 0.0f;
	float *beam;

	if (!pixels || !(beam = malloc((size_t)length * sizeof(float))))
	{
		free(pixels);
		return false;
	}
	for (int i = 0; i < length; i++)
	{
		/* the line's light across its band: a beam, brightest in its middle */
		const float at = fmodf(((float)i + 0.5f) / band, 1.0f) - 0.5f;

		beam[i] = 1.0f - lines * (1.0f - cosf(at * 2.0f * 3.14159265f)) * 0.5f;
	}
	for (int y = 0; y < h; y++)
	{
		const float dy = ((float)y + 0.5f) / (float)h * 2.0f - 1.0f;

		for (int x = 0; x < w; x++)
		{
			const float dx = ((float)x + 0.5f) / (float)w * 2.0f - 1.0f;
			const float light = beam[turned ? y : x];
			const uint32_t v = (uint32_t)(light * (1.0f - l->corners * (dx * dx + dy * dy) * 0.5f) * 255.0f + 0.5f);

			pixels[(size_t)y * (size_t)w + (size_t)x] = 0xFF000000u | v << 16 | v << 8 | v;
		}
	}
	free(beam);
	if (pattern)
		SDL_DestroyTexture(pattern);
	pattern = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (pattern)
	{
		SDL_UpdateTexture(pattern, NULL, pixels, w * (int)sizeof(uint32_t));
		SDL_SetTextureBlendMode(pattern, SDL_BLENDMODE_MOD);
	}
	free(pixels);
	return pattern != NULL;
}

/* The phosphor stripes, on the screen's pixels (the glass's, not curved with the picture): w x h, red, green and blue
   across the monitor's lines (down the upright picture: rows; turned: columns), a pixel each, two from 1440 pixels */
static bool make_stripes(SDL_Renderer *renderer, int w, int h, bool rows, const look *l)
{
	uint32_t *pixels = malloc((size_t)w * (size_t)h * sizeof(uint32_t));
	const int stripe = (rows ? h : w) >= 1440 ? 2 : 1;
	const uint32_t dim = (uint32_t)((1.0f - l->stripes) * 255.0f + 0.5f);
	const uint32_t colours[3] = { 0xFF000000u | 255u << 16 | dim << 8 | dim, 0xFF000000u | dim << 16 | 255u << 8 | dim,
		0xFF000000u | dim << 16 | dim << 8 | 255u };

	if (!pixels)
		return false;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			pixels[(size_t)y * (size_t)w + (size_t)x] = colours[(rows ? y : x) / stripe % 3];
	if (stripes)
		SDL_DestroyTexture(stripes);
	stripes = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (stripes)
	{
		SDL_UpdateTexture(stripes, NULL, pixels, w * (int)sizeof(uint32_t));
		SDL_SetTextureBlendMode(stripes, SDL_BLENDMODE_MOD);
		SDL_SetTextureScaleMode(stripes, SDL_ScaleModeNearest);
	}
	free(pixels);
	return stripes != NULL;
}

/* The glow: the picture blurred (a box 5 pixels wide, twice each way: nearly a bell), into glow_pixels */
static void blur(const uint32_t *pixels)
{
	static uint32_t pass[PICTURE_W * PICTURE_H];
	const uint32_t *from = pixels;
	uint32_t *to = pass;

	for (int round = 0; round < 4; round++)
	{
		const bool across = round % 2 == 0;
		const int length = across ? PICTURE_W : PICTURE_H, count = across ? PICTURE_H : PICTURE_W;

		for (int line = 0; line < count; line++)
		{
			uint32_t r = 0, g = 0, b = 0;
			const int step = across ? 1 : PICTURE_W, start = across ? line * PICTURE_W : line;

			/* a running sum of the 5 around each pixel (the edges repeated) */
			for (int i = -2; i <= 2; i++)
			{
				const uint32_t p = from[start + (i < 0 ? 0 : i) * step];

				r += p >> 16 & 255u;
				g += p >> 8 & 255u;
				b += p & 255u;
			}
			for (int i = 0; i < length; i++)
			{
				const uint32_t out = from[start + (i + 3 < length ? i + 3 : length - 1) * step];
				const uint32_t in = from[start + (i - 2 > 0 ? i - 2 : 0) * step];

				to[start + i * step] = 0xFF000000u | (r / 5u) << 16 | (g / 5u) << 8 | b / 5u;
				r += (out >> 16 & 255u) - (in >> 16 & 255u);
				g += (out >> 8 & 255u) - (in >> 8 & 255u);
				b += (out & 255u) - (in & 255u);
			}
		}
		from = to;
		to = to == pass ? glow_pixels : pass;
	}
	if (from != glow_pixels)
		memcpy(glow_pixels, from, sizeof(glow_pixels));
}

/* The picture brighter, by 'boost' / 255 of itself (each colour at most 255), into bright_pixels (done here, not by
   drawing it twice added up: SDL's software renderer leaves out the second) */
static void brighten(const uint32_t *pixels, unsigned boost)
{
	static uint8_t table[256];
	static unsigned made = 0x100u;

	if (made != boost)
	{
		for (unsigned v = 0; v < 256u; v++)
		{
			const unsigned up = v + v * boost / 255u;

			table[v] = (uint8_t)(up > 255u ? 255u : up);
		}
		made = boost;
	}
	for (int i = 0; i < PICTURE_W * PICTURE_H; i++)
	{
		const uint32_t p = pixels[i];

		bright_pixels[i] = 0xFF000000u | (uint32_t)table[p >> 16 & 255u] << 16 | (uint32_t)table[p >> 8 & 255u] << 8
			| table[p & 255u];
	}
}

/* The tube's curve: where a point of the flat picture (-1 to 1 each way from its centre) shows on the screen; the
   inverse of the usual curve's lookup (from the screen to the picture: each way stretched by 1 + curve * the other
   way's square), found by a few steps */
static void curved(float sx, float sy, float curve, float *qx, float *qy)
{
	float x = sx, y = sy;

	for (int step = 0; step < 6; step++)
	{
		x = sx / (1.0f + curve * y * y);
		y = sy / (1.0f + curve * x * x);
	}
	*qx = x;
	*qy = y;
}

bool crt_draw(SDL_Renderer *renderer, SDL_Texture *picture, const uint32_t *pixels, SDL_FRect unturned, int turn,
	int level)
{
	const look *const l = &looks[level > CRT_OFF && level < CRT_LEVELS ? level : CRT_LIGHT];
	const bool turned = turn == 90 || turn == 270;
	/* the picture as it shows on the screen, turned or not */
	const float shown_w = turned ? unturned.h : unturned.w, shown_h = turned ? unturned.w : unturned.h;
	const float cx = unturned.x + unturned.w / 2.0f, cy = unturned.y + unturned.h / 2.0f;
	const int w = (int)(shown_w + 0.5f), h = (int)(shown_h + 0.5f);
	static SDL_Vertex vertices[(MESH_X + 1) * (MESH_Y + 1)];
	static int indices[MESH_X * MESH_Y * 6];
	SDL_RendererInfo info;
	SDL_FRect inside;

	if (w < 16 || h < 16 || SDL_GetRendererInfo(renderer, &info) != 0 || !(info.flags & SDL_RENDERER_TARGETTEXTURE))
		return false;
	if (w != target_w || h != target_h || turn != target_turn || !target)
	{
		crt_reset();
		target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
		glow = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, PICTURE_W, PICTURE_H);
		bright = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, PICTURE_W, PICTURE_H);
		if (!target || !glow || !bright)
		{
			crt_reset();
			return false;
		}
		SDL_SetTextureScaleMode(target, SDL_ScaleModeLinear);
		SDL_SetTextureScaleMode(glow, SDL_ScaleModeLinear);
		SDL_SetTextureScaleMode(bright, SDL_ScaleModeLinear);
		SDL_SetTextureBlendMode(bright, SDL_BLENDMODE_NONE);
		SDL_SetTextureBlendMode(glow, SDL_BLENDMODE_ADD);
		target_w = w;
		target_h = h;
		target_turn = turn;
	}
	if (pattern_level != level)
	{
		if (!make_pattern(renderer, w, h, turned, l))
		{
			crt_reset();
			return false;
		}
		pattern_level = level;
	}

	/* flat, into the target, turned as it shows: the picture made brighter for the light the pattern takes back (soft
	   along the lines, as the beam is), its glow, then the pattern over all of it */
	if (SDL_SetRenderTarget(renderer, target) != 0)
		return false;
	inside.w = (float)(turned ? h : w);
	inside.h = (float)(turned ? w : h);
	inside.x = ((float)w - inside.w) / 2.0f;
	inside.y = ((float)h - inside.h) / 2.0f;
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	(void)picture;    /* (the frame's pixels are drawn from: brightened) */
	brighten(pixels, l->boost);
	SDL_UpdateTexture(bright, NULL, bright_pixels, PICTURE_W * (int)sizeof(uint32_t));
	SDL_RenderCopyExF(renderer, bright, NULL, &inside, (double)turn, NULL, SDL_FLIP_NONE);
	blur(pixels);
	SDL_UpdateTexture(glow, NULL, glow_pixels, PICTURE_W * (int)sizeof(uint32_t));
	SDL_SetTextureColorMod(glow, l->glow, l->glow, l->glow);
	SDL_RenderCopyExF(renderer, glow, NULL, &inside, (double)turn, NULL, SDL_FLIP_NONE);
	SDL_RenderCopy(renderer, pattern, NULL, NULL);
	SDL_SetRenderTarget(renderer, NULL);

	/* then curved onto the screen */
	for (int j = 0; j <= MESH_Y; j++)
		for (int i = 0; i <= MESH_X; i++)
		{
			SDL_Vertex *const v = &vertices[j * (MESH_X + 1) + i];
			const float u = (float)i / MESH_X, t = (float)j / MESH_Y;
			float qx, qy;

			curved(u * 2.0f - 1.0f, t * 2.0f - 1.0f, l->curve * 4.0f, &qx, &qy);
			v->position.x = cx + qx * shown_w / 2.0f;
			v->position.y = cy + qy * shown_h / 2.0f;
			v->color.r = v->color.g = v->color.b = v->color.a = 255;
			v->tex_coord.x = u;
			v->tex_coord.y = t;
		}
	for (int j = 0, n = 0; j < MESH_Y; j++)
		for (int i = 0; i < MESH_X; i++)
		{
			const int a = j * (MESH_X + 1) + i, b = a + 1, c = a + MESH_X + 1, d = c + 1;

			indices[n++] = a;
			indices[n++] = b;
			indices[n++] = c;
			indices[n++] = b;
			indices[n++] = d;
			indices[n++] = c;
		}
	SDL_SetTextureBlendMode(target, SDL_BLENDMODE_NONE);
	if (SDL_RenderGeometry(renderer, target, vertices, (MESH_X + 1) * (MESH_Y + 1), indices, MESH_X * MESH_Y * 6) != 0)
		return false;

	/* the stripes over it, pixel for pixel on the screen (the curve's outside is black: they leave it so) */
	{
		const SDL_Rect where = { (int)floorf(cx - shown_w / 2.0f), (int)floorf(cy - shown_h / 2.0f),
			(int)ceilf(shown_w) + 1, (int)ceilf(shown_h) + 1 };

		if (where.w != stripes_w || where.h != stripes_h || turn != stripes_turn || level != stripes_level || !stripes)
		{
			if (!make_stripes(renderer, where.w, where.h, !turned, l))
				return true;    /* (the picture is drawn: without its stripes) */
			stripes_w = where.w;
			stripes_h = where.h;
			stripes_turn = turn;
			stripes_level = level;
		}
		SDL_RenderCopy(renderer, stripes, NULL, &where);
	}
	return true;
}
