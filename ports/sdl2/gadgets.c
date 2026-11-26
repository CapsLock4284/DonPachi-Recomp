/**
 *    Name:        gadgets.c
 *    Description: the gadget panels (gadgets.h)
 *    Version:     0.1.0 (draft)
 *    Created:     26-11-2026 (from DoDonPachi Recomp's)
 *
 *    license:BSD-3-Clause
 *    Laid out in the panel's units (the game's pixels: WIDGET_H tall, 'width' wide) and drawn at k pixels to each
 *    (gadget_canvas.h). Each gadget is a box framed in brushed steel with a blue header strip; they are stacked down
 *    each panel, the room left over shared out between them.
 */
#include "gadgets.h"
#include "achievements.h"
#include "gadget_canvas.h"
#include "widgets.h"

#include "cave/board.h"
#include "cave/machine.h"
#include "cave/rom_set.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The SESSION gadget under INPUT (owner, 26-11-2026: as DoDonPachi Recomp's; 0 takes it out, the left panel as
   before) */
#ifndef SESSION_LINE
#define SESSION_LINE 1
#endif

/* ---- the look ---- */

#define BACK_TOP    0x0B1A36u
#define BACK_FOOT   0x02060Fu
#define INNER       0x0A1A38u
#define WELL        0x0F2648u   /* a well in a gadget: a bar's track, a box */
#define WELL_EDGE   0x2A4A7Eu
#define HEADER_TEXT 0xF2F7FFu
#define HEADER_NOTE 0xBFD6FFu
#define LABEL       0x9DB4DAu
#define NUMBER      0xE6EFFFu
#define DIM         0x5D7AA8u
#define GOLD        0xFFC83Cu
#define ORANGE      0xFF8A2Au
#define RED         0xFF4F4Fu
#define PINK        0xFF6BD8u
#define CYAN        0x3FA9FFu
#define SKY         0x6CD8FFu
#define TEAL        0x2FD3C7u
#define GREEN       0x3DDC5Cu
#define YELLOW      0xFFE040u
#define EXTEND      0xC8FFC8u

#define MARGIN      3.0f        /* around the panel's gadgets */
#define GAP         3.0f        /* between them, at least */
#define FRAME       1.1f        /* the steel's width */
#define RADIUS      3.0f
#define HEADER      8.5f        /* the header strip's height */
#define PAD         2.6f        /* inside a gadget's body */
#define TITLE       5.0f        /* the header's text */
#define NOTE        3.6f        /* the header's note at its right */
#define SMALL       3.9f        /* a label */
#define VALUE       4.6f        /* a value beside it */

static const uint32_t steel[4] = { 0xE3E9F1u, 0x9AA7B9u, 0x6F7D91u, 0xC7D0DCu };
static const float steel_at[4] = { 0.0f, 0.38f, 0.52f, 1.0f };
static const uint32_t strip[3] = { 0x3B7CF0u, 0x1A4BB0u, 0x123A8Eu };
static const float strip_at[3] = { 0.0f, 0.55f, 1.0f };
static const uint32_t places[5] = { GOLD, 0xFF5CE0u, ORANGE, GREEN, SKY };    /* (the classic panel's #1 ... #5) */

typedef struct
{
	float x, y, w, h;           /* the body: inside the frame, under the header */
} body;

/* The panels' frames, header strips and background are drawn once for a width and k (the chrome: kept, and copied
   under each frame's panels), their text and contents each frame: true while the chrome is drawn */
static bool chrome_pass;

/* Each gadget is drawn again, and given to the graphics card, only when what it shows has changed: first recorded
   (gcanvas.hash: its drawing as a hash, nothing drawn), then, when the hash isn't last frame's, its place put back
   to the chrome and the gadget drawn there. A side is drawn whole (fresh false) when its chrome is new, its picture
   isn't the one drawn last time, the help or an achievement's box has been over it, or gadgets_invalidate */
#define PARTS 6

static struct
{
	uint32_t hash[2][PARTS];
	bool fresh[2];
	const uint32_t *pixels[2];  /* the pictures last drawn */
	bool toast;                 /* an achievement's box over the right panel last frame */
	bool help;                  /* the help drawn over the right panel */
} kept;

static int side_now;               /* the side being drawn: 0 left, 1 right */
static const uint32_t *chrome_now; /* its chrome */
static gadgets_dirty *dirty_now;   /* the places drawn again, or NULL */

typedef void (*part_fn)(gcanvas *c, const widgets_view *v, float x, float y, float w, float h);

static void part(gcanvas *c, int id, part_fn draw, const widgets_view *v, float x, float y, float w, float h)
{
	uint32_t hash = 2166136261u;
	gcanvas record = *c;
	const float place[4] = { x, y, w, h };

	if (chrome_pass)
	{
		draw(c, v, x, y, w, h);
		return;
	}
	record.hash = &hash;
	gc_record(&record, place, sizeof(place));
	draw(&record, v, x, y, w, h);
	if (kept.fresh[side_now] && kept.hash[side_now][id] == hash)
		return;
	kept.hash[side_now][id] = hash;
	if (!kept.fresh[side_now] || !chrome_now)
	{
		draw(c, v, x, y, w, h);    /* (the side drawn whole: its chrome copied already) */
		return;
	}
	{
		/* (its place, a pixel more each way, back to the chrome: then drawn) */
		int left = (int)floorf(x * c->k) - 1, top = (int)floorf(y * c->k) - 1;
		int right = (int)ceilf((x + w) * c->k) + 1, bottom = (int)ceilf((y + h) * c->k) + 1;

		left = left < 0 ? 0 : left;
		top = top < 0 ? 0 : top;
		right = right > c->w ? c->w : right;
		bottom = bottom > c->h ? c->h : bottom;
		if (left >= right || top >= bottom)
			return;
		for (int row = top; row < bottom; row++)
			memcpy(c->pixels + (size_t)row * (size_t)c->w + (size_t)left,
				chrome_now + (size_t)row * (size_t)c->w + (size_t)left, (size_t)(right - left) * sizeof(uint32_t));
		draw(c, v, x, y, w, h);
		if (dirty_now)
		{
			int *const count = &dirty_now->count[side_now];

			if (*count < GADGETS_RECTS)
			{
				const gadgets_rect rect = { left, top, right - left, bottom - top };

				dirty_now->rect[side_now][(*count)++] = rect;
			}
			else
			{
				const gadgets_rect whole = { 0, 0, c->w, c->h };

				dirty_now->rect[side_now][0] = whole;
				*count = 1;
			}
		}
	}
}

static void background(gcanvas *c, float width, float height)
{
	static const uint32_t colours[2] = { BACK_TOP, BACK_FOOT };
	static const float at[2] = { 0.0f, 1.0f };

	gc_gradient(c, 0.0f, 0.0f, width, height, 0.0f, colours, at, 2);
}

/* The panel drawn larger where it has the room: the gadgets as they are laid out need 'natural' of its height (their
   gaps at their least); the canvas zoomed so that they fill up to 1.3 times, as far as the width keeps 118 units (the
   narrowest panel's room); the panel's width and height returned in the zoomed units */
static gcanvas zoomed(const gcanvas *c, float natural, float *width, float *height)
{
	gcanvas z = *c;
	float zoom = ((float)WIDGET_H - 2.0f * MARGIN) / natural;

	if (zoom > 1.3f)
		zoom = 1.3f;
	if (zoom > *width / 118.0f)
		zoom = *width / 118.0f;
	if (zoom < 1.0f)
		zoom = 1.0f;
	z.k = c->k * zoom;
	*width /= zoom;
	*height = (float)WIDGET_H / zoom;
	return z;
}

/* a gadget's frame and header ('note' at the header's right, or NULL), its body returned */
static body gadget(gcanvas *c, float x, float y, float w, float h, const char *title, const char *note)
{
	const float ix = x + FRAME, iy = y + FRAME, iw = w - 2.0f * FRAME, ih = h - 2.0f * FRAME;
	body b;

	if (chrome_pass)
	{
		gc_box(c, x - 0.3f, y + 0.6f, w + 0.6f, h + 0.6f, RADIUS + 0.3f, 0x000000u, 120u);    /* (its shadow) */
		gc_gradient(c, x, y, w, h, RADIUS, steel, steel_at, 4);
		gc_box(c, ix, iy, iw, ih, RADIUS - FRAME * 0.6f, INNER, 255u);
		gc_gradient(c, ix, iy, iw, HEADER, RADIUS - FRAME * 0.6f, strip, strip_at, 3);
		gc_box(c, ix, iy + HEADER - 1.5f, iw, 1.5f, 0.0f, strip[2], 255u);    /* (its foot square) */
		gc_brushed(c, ix, iy, iw, HEADER, 0.75f, 0xFFFFFFu, 14u);
		gc_box(c, ix, iy + HEADER, iw, 0.5f, 0.0f, 0x061230u, 255u);
	}
	else
	{
		const float note_w = note ? gc_text_width(GC_FONT_TEXT, NOTE, note) + 2.0f : 0.0f;
		const float size = gc_fit(GC_FONT_BOLD, TITLE, title, iw - 6.0f - note_w);

		gc_text(c, GC_FONT_BOLD, size, ix + 3.0f, gc_middle(GC_FONT_BOLD, size, iy, HEADER), title, HEADER_TEXT,
			GC_LEFT);
		if (note)
			gc_text(c, GC_FONT_TEXT, NOTE, ix + iw - 3.0f, gc_middle(GC_FONT_TEXT, NOTE, iy, HEADER), note,
				HEADER_NOTE, GC_RIGHT);
	}
	b.x = ix + PAD;
	b.y = iy + HEADER + 0.5f + PAD * 0.8f;
	b.w = iw - 2.0f * PAD;
	b.h = ih - HEADER - 0.5f - PAD * 1.6f;
	return b;
}

/* a label at the left and its value at the right, on a line from y, 'height' tall */
static void row(gcanvas *c, const body *b, float y, const char *label, uint32_t label_colour, const char *value,
	uint32_t value_colour)
{
	gc_text(c, GC_FONT_TEXT, SMALL, b->x, gc_middle(GC_FONT_TEXT, SMALL, y, VALUE), label, label_colour, GC_LEFT);
	if (value)
		gc_text(c, GC_FONT_TEXT, VALUE, b->x + b->w, gc_middle(GC_FONT_TEXT, VALUE, y, VALUE), value, value_colour,
			GC_RIGHT);
}

/* a bar's track and its filled part ('part' 0-1) shaded across */
static void bar(gcanvas *c, float x, float y, float w, float h, float part, const uint32_t *colours, const float *at,
	int count)
{
	gc_box(c, x, y, w, h, h * 0.5f, WELL_EDGE, 255u);
	gc_box(c, x + 0.3f, y + 0.3f, w - 0.6f, h - 0.6f, h * 0.5f - 0.3f, WELL, 255u);
	if (part > 0.0f)
	{
		/* (the shading laid over the whole track, shown as far as the part: its colour says where it is) */
		gcanvas cut = *c;

		cut.clip = (int)((x + 0.3f + (w - 0.6f) * (part > 1.0f ? 1.0f : part)) * c->k + 0.5f);
		gc_gradient_across(&cut, x + 0.3f, y + 0.3f, w - 0.6f, h - 0.6f, h * 0.5f - 0.3f, colours, at, count);
	}
}

/* a score as the game shows it, its thousands parted with commas ('sign': a + before it) */
static void points_text(char *out, size_t size, uint32_t value, bool sign)
{
	char digits[16], grouped[24];
	int length, at = 0;

	snprintf(digits, sizeof(digits), "%lu", (unsigned long)value);
	length = (int)strlen(digits);
	for (int i = 0; i < length; i++)
	{
		grouped[at++] = digits[i];
		if ((length - 1 - i) % 3 == 0 && i < length - 1)
			grouped[at++] = ',';
	}
	grouped[at] = '\0';
	snprintf(out, size, "%s%s", sign ? "+" : "", grouped);
}

/* ---- the bee: the game's own sprite ($0C2E, 32 x 32, palette $3B), turned as the picture is ---- */

#define BEE_CODE    0x0C2Eu
#define BEE_PALETTE 0x3Bu       /* (16 colours a palette: the sprite's attribute as shown, $3B30) */

static unsigned bee_pen(int x, int y)
{
	const uint32_t pixel = (BEE_CODE << 8) + (uint32_t)(y * 32 + x);
	const uint8_t byte = rom_set.sprites[(pixel >> 1) & (DONPACHI_SPRITES_SIZE - 1u)];

	return (pixel & 1u) ? (byte & 15u) : (byte >> 4);
}

/* xGRB 555, as the board's palette has it, to 0x00RRGGBB */
static uint32_t bee_colour(unsigned pen)
{
	const uint16_t value = cave_board.palette[(BEE_PALETTE * 16u + pen) & (CAVE_PALETTE_WORDS - 1u)];
	const uint32_t g = value >> 10 & 31u, r = value >> 5 & 31u, b = value & 31u;

	if (!cave_board.palette[(BEE_PALETTE * 16u + 2u) & (CAVE_PALETTE_WORDS - 1u)])
		return pen < 12u ? GOLD : 0xC0E000u;    /* (its palette not set up yet) */
	return (r << 3 | r >> 2) << 16 | (g << 3 | g >> 2) << 8 | (b << 3 | b >> 2);
}

/* the bee drawn at a side in pixels, its colours or grey and dim ('lit' false): kept (the colour and its cover, 0 to
   4, of each pixel) while the side and the bee's palette stay the same */
typedef struct
{
	int side;
	uint32_t palette;
	uint32_t *pixels;           /* colour << 8 | cover */
} bee_icon;

static const uint32_t *bee_pixels(int side, bool lit)
{
	static bee_icon icons[2];
	bee_icon *const icon = &icons[lit ? 1 : 0];
	uint32_t palette = 2166136261u;

	for (unsigned pen = 0; pen < 16u; pen++)
		palette = (palette ^ cave_board.palette[(BEE_PALETTE * 16u + pen) & (CAVE_PALETTE_WORDS - 1u)]) * 16777619u;
	if (icon->pixels && icon->side == side && icon->palette == palette)
		return icon->pixels;
	free(icon->pixels);
	icon->pixels = malloc((size_t)side * (size_t)side * sizeof(uint32_t));
	icon->side = side;
	icon->palette = palette;
	if (!icon->pixels)
		return NULL;
	for (int py = 0; py < side; py++)
		for (int px = 0; px < side; px++)
		{
			/* (the picture's (u, v) is the sprite's (29 - v, 3 + u) of its 26 x 26 middle: the board's screen lies
			   on its side; 2 x 2 samples each pixel, as many of them as are the bee's making its cover) */
			unsigned r = 0, g = 0, b = 0, count = 0;

			for (int s = 0; s < 4; s++)
			{
				const float u = ((float)px + 0.25f + 0.5f * (float)(s & 1)) / (float)side;
				const float v = ((float)py + 0.25f + 0.5f * (float)(s >> 1)) / (float)side;
				const int sx = 28 - (int)(v * 26.0f), sy = 3 + (int)(u * 26.0f);
				const unsigned pen = bee_pen(sx < 0 ? 0 : sx > 31 ? 31 : sx, sy < 0 ? 0 : sy > 31 ? 31 : sy);

				if (pen)
				{
					const uint32_t colour = bee_colour(pen);

					r += colour >> 16 & 255u;
					g += colour >> 8 & 255u;
					b += colour & 255u;
					count++;
				}
			}
			if (count)
			{
				r /= count;
				g /= count;
				b /= count;
				if (!lit)
				{
					const unsigned grey = (r * 30u + g * 59u + b * 11u) / 100u * 45u / 100u + 20u;

					r = g = b = grey;
				}
			}
			icon->pixels[py * side + px] = (r << 16 | g << 8 | b) << 8 | count;
		}
	return icon->pixels;
}

/* the bee in a square (units), its colours, or grey and dim ('lit' false) */
static void bee(gcanvas *c, float x, float y, float size, bool lit)
{
	const int left = (int)(x * c->k + 0.5f), top = (int)(y * c->k + 0.5f), side = (int)(size * c->k + 0.5f);
	const uint32_t *icon;

	if (c->hash)
	{
		/* (recorded: its place, size and colours; the palette's 16 colours with them) */
		const float numbers[4] = { x, y, size, c->k };

		gc_record(c, numbers, sizeof(numbers));
		gc_record(c, &lit, sizeof(lit));
		gc_record(c, &cave_board.palette[(BEE_PALETTE * 16u) & (CAVE_PALETTE_WORDS - 1u)], 16u * sizeof(uint16_t));
		return;
	}
	if (!rom_set.sprites || side <= 0 || !(icon = bee_pixels(side, lit)))
		return;
	for (int py = 0; py < side; py++)
		for (int px = 0; px < side; px++)
		{
			const uint32_t made = icon[py * side + px];
			const unsigned count = made & 255u, r = made >> 24, g = made >> 16 & 255u, b = made >> 8 & 255u;
			const int dx = left + px, dy = top + py;

			if (count >= 2u && dx >= 0 && dx < c->w && dy >= 0 && dy < c->h)
			{
				uint32_t *const pixel = c->pixels + (size_t)dy * (size_t)c->w + (size_t)dx;
				const unsigned a = count * 64u - 1u, keep = 255u - a;
				const uint32_t old = *pixel;

				*pixel = (((old >> 16 & 255u) * keep + r * a) / 255u) << 16
					| (((old >> 8 & 255u) * keep + g * a) / 255u) << 8 | (((old & 255u) * keep + b * a) / 255u);
			}
		}
}

/* ---- the left panel ---- */

/* GET POINT: a player's chain (its hits, the time left to add one: 30 frames after a kill, its points, the best) */
static float chain_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 15.0f + 2.0f + 4.0f + 2.0f + 2.0f * (VALUE + 1.6f);
}

static void draw_chain_of(gcanvas *c, const widgets_view *v, float x, float y, float w, float h, int player)
{
	static const uint32_t outline[2] = { ORANGE, CYAN };
	const bool on = v->own && v->in_play[player];
	const unsigned hits = on ? v->chain[player].hits : 0u, timer = on ? v->chain[player].timer : 0u;
	char text[24];
	body b;
	float size, line, at;

	b = gadget(c, x, y, w, h, player ? "GET POINT \x80 2P" : "GET POINT \x80 1P", on ? "CHAIN" : NULL);
	if (chrome_pass)
		return;
	snprintf(text, sizeof(text), "%u", hits);
	size = gc_fit(GC_FONT_BOLD, 15.0f, text, b.w - gc_text_width(GC_FONT_BOLD, 6.0f, "HIT") - 3.0f);
	line = b.y + 13.0f;
	gc_text_outlined(c, GC_FONT_BOLD, size, b.x, line, text, on && hits ? HEADER_TEXT : DIM,
		on && hits ? outline[player] : WELL_EDGE, 0.45f, GC_LEFT);
	gc_text(c, GC_FONT_BOLD, 6.0f, b.x + gc_text_width(GC_FONT_BOLD, size, text) + 2.0f, line, "HIT",
		on && hits ? GOLD : DIM, GC_LEFT);
	{
		/* the time left: green, yellow, red as it runs out; its frames counted down in it */
		const uint32_t colour = timer > 15u ? GREEN : timer > 7u ? YELLOW : RED;
		const uint32_t colours[2] = { colour, colour };
		static const float stops[2] = { 0.0f, 1.0f };

		at = b.y + 17.0f;
		bar(c, b.x, at, b.w, 4.0f, (float)(timer > 30u ? 30u : timer) / 30.0f, colours, stops, 2);
		if (timer)
		{
			snprintf(text, sizeof(text), "%u", timer);
			gc_text(c, GC_FONT_BOLD, 3.4f, b.x + b.w * 0.5f, gc_middle(GC_FONT_BOLD, 3.4f, at, 4.0f), text, INNER,
				GC_CENTRE);
		}
	}
	at += 6.0f;
	points_text(text, sizeof(text), on ? v->chain[player].points : 0u, false);
	row(c, &b, at, "PTS", on ? LABEL : DIM, text, on ? NUMBER : DIM);
	at += VALUE + 1.6f;
	snprintf(text, sizeof(text), "%u", on ? v->chain[player].best : 0u);
	row(c, &b, at, "MAX", on ? LABEL : DIM, text, on ? GOLD : DIM);
}

static void draw_chain_1(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	draw_chain_of(c, v, x, y, w, h, 0);
}

static void draw_chain_2(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	draw_chain_of(c, v, x, y, w, h, 1);
}

static float rank_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 4.0f + 1.4f + 3.6f;
}

static void draw_rank(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	static const uint32_t colours[3] = { GREEN, YELLOW, RED };
	static const float at[3] = { 0.0f, 0.5f, 1.0f };
	char note[24];
	body b;

	snprintf(note, sizeof(note), "%u / 63", v->rank);
	b = gadget(c, x, y, w, h, "RANK", v->rank_known ? note : "- / 63");
	if (chrome_pass)
		return;
	bar(c, b.x, b.y, b.w, 4.0f, v->rank_known ? (float)v->rank / 63.0f : 0.0f, colours, at, 3);
	gc_text(c, GC_FONT_TEXT, 3.4f, b.x, b.y + 4.0f + 1.4f + 3.0f, "EASY", DIM, GC_LEFT);
	gc_text(c, GC_FONT_TEXT, 3.4f, b.x + b.w, b.y + 4.0f + 1.4f + 3.0f, "HARD", DIM, GC_RIGHT);
}

/* the loop's areas against their best scores */
static float areas_height(bool two_lines)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 5.0f * (two_lines ? 12.4f : 8.4f);
}

/* how an area went against its best: "+312,000", "-88,400" */
static void difference(char *out, size_t size, uint32_t made, uint32_t against)
{
	char value[24];

	points_text(value, sizeof(value), made > against ? made - against : against - made, false);
	snprintf(out, size, "%s%s", made >= against ? "+" : "-", value);
}

static void draw_areas(gcanvas *c, const widgets_view *v, float x, float y, float w, float h, bool two_lines)
{
	char title[32];
	body b;

	snprintf(title, sizeof(title), "AREAS \x80 LOOP %d", v->loop_shown + 1);
	b = gadget(c, x, y, w, h, title, two_lines ? NULL : "VS YOUR BEST");
	if (chrome_pass)
		return;
	for (int at = 0; at < 5; at++)
	{
		const float line = two_lines ? 12.4f : 8.4f, top = b.y + (float)at * line, base = top + 5.8f;
		const int state = v->areas[at].state;
		const uint32_t made = v->areas[at].made, best = v->areas[at].best;
		char label[8], score[40], note[48];
		uint32_t label_colour = LABEL, score_colour = NUMBER, note_colour = LABEL;

		if (at > 0)
			gc_box(c, b.x, top - 0.6f, b.w, 0.25f, 0.0f, 0x16305Au, 255u);
		snprintf(label, sizeof(label), "%d", v->loop_shown * 5 + at + 1);
		note[0] = '\0';
		if (state == WIDGET_AREA_LIVE)
		{
			label_colour = YELLOW;
			points_text(score, sizeof(score), made, false);
			if (!v->areas[at].counts)
				snprintf(note, sizeof(note), "UNRANKED");
			else if (best)
			{
				difference(note, sizeof(note), made, best);
				note_colour = made > best ? GOLD : LABEL;
			}
		}
		else if (state == WIDGET_AREA_DONE)
		{
			points_text(score, sizeof(score), made, false);
			if (v->areas[at].new_best)
				score_colour = GOLD;
			if (!v->areas[at].counts)
			{
				snprintf(note, sizeof(note), "UNRANKED");
				note_colour = DIM;
			}
			else if (!best)
			{
				snprintf(note, sizeof(note), "NEW BEST");
				note_colour = GOLD;
			}
			else
			{
				char diff[32];

				difference(diff, sizeof(diff), made, best);
				snprintf(note, sizeof(note), "%s%s", v->areas[at].new_best ? "BEST " : "", diff);
				note_colour = v->areas[at].new_best ? GOLD : RED;
			}
		}
		else
		{
			label_colour = score_colour = DIM;
			if (best)
			{
				char value[24];

				points_text(value, sizeof(value), best, false);
				snprintf(score, sizeof(score), "BEST %s", value);
			}
			else
				snprintf(score, sizeof(score), "-");
		}
		gc_text(c, GC_FONT_BOLD, 5.0f, b.x, base, label, label_colour, GC_LEFT);
		if (two_lines)
		{
			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, VALUE, score, b.w - 10.0f), b.x + b.w, base, score,
				score_colour, GC_RIGHT);
			if (note[0])
				gc_text(c, GC_FONT_TEXT, 3.6f, b.x + b.w, base + 5.0f, note, note_colour, GC_RIGHT);
		}
		else
		{
			const float note_w = note[0] ? gc_text_width(GC_FONT_TEXT, 3.6f, note) + 3.0f : 0.0f;

			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, VALUE, score, b.w - 10.0f - note_w),
				b.x + b.w - note_w, base, score, score_colour, GC_RIGHT);
			if (note[0])
				gc_text(c, GC_FONT_TEXT, 3.6f, b.x + b.w, base, note, note_colour, GC_RIGHT);
		}
	}
}

static void draw_bees(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	char title[24], note[16];
	body b;

	if (v->bee_area >= 0)
		snprintf(title, sizeof(title), "BEES \x80 AREA %d", v->bee_area + 1);
	else
		snprintf(title, sizeof(title), "BEES");
	snprintf(note, sizeof(note), "%u / 13", v->bees_taken);
	b = gadget(c, x, y, w, h, title, v->own ? note : NULL);
	if (chrome_pass)
		return;
	{
		/* 7 to a row (the last row centred), as big as the body's width allows and its height */
		const float gap = 1.6f;
		float cell = (b.w - 6.0f * gap) / 7.0f;

		if (2.0f * cell + gap > b.h)
			cell = (b.h - gap) / 2.0f;
		for (int at = 0; at < 13; at++)
		{
			const int line = at / 7, column = at % 7, in_line = line ? 6 : 7;
			const float left = b.x + (b.w - ((float)in_line * cell + (float)(in_line - 1) * gap)) * 0.5f;
			const float cx = left + (float)column * (cell + gap), cy = b.y + (float)line * (cell + gap);
			const bool taken = (unsigned)at < v->bees_taken, out = !taken && v->bee_area_over;
			const uint32_t edge = taken ? GOLD : out ? RED : WELL_EDGE;

			gc_box(c, cx, cy, cell, cell, 1.2f, edge, 255u);
			gc_box(c, cx + 0.45f, cy + 0.45f, cell - 0.9f, cell - 0.9f, 0.9f,
				taken ? 0x2A2208u : out ? 0x2A0C14u : WELL, 255u);
			bee(c, cx + cell * 0.08f, cy + cell * 0.08f, cell * 0.84f, taken);
			if (out)
			{
				gc_line(c, cx + cell * 0.25f, cy + cell * 0.25f, cx + cell * 0.75f, cy + cell * 0.75f, cell * 0.1f, RED,
					255u);
				gc_line(c, cx + cell * 0.75f, cy + cell * 0.25f, cx + cell * 0.25f, cy + cell * 0.75f, cell * 0.1f, RED,
					255u);
			}
		}
	}
}

static float bees_least(float w)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 2.0f * ((w - 2.0f * FRAME - 2.0f * PAD - 9.6f) / 7.0f) + 1.6f;
}

static float input_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 14.0f;
}

static void draw_input(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	const body b = gadget(c, x, y, w, h, "INPUT", NULL);
	if (chrome_pass)
		return;
	const bool on = v->own;
	const uint16_t held = on ? v->held : 0u;
	const int dx = held & CAVE_LEFT ? -1 : held & CAVE_RIGHT ? 1 : 0, dy = held & CAVE_UP ? -1 : held & CAVE_DOWN ? 1 : 0;
	static const struct { uint16_t bit; const char *name; } buttons[3] = {
		{ CAVE_BUTTON1, "S" }, { CAVE_BUTTON2, "B" }, { CAVE_BUTTON3, "C" }
	};
	const float cell = 4.2f, stick = 3.0f * cell + 2.0f * 0.8f, radius = 5.6f;
	const float together = stick + 4.0f + 3.0f * 2.0f * radius + 2.0f * 2.4f;
	const float left = b.x + (b.w - together) * 0.5f, top = b.y + (b.h - stick) * 0.5f;

	for (int row_at = -1; row_at <= 1; row_at++)
		for (int column = -1; column <= 1; column++)
		{
			const float cx = left + (float)(column + 1) * (cell + 0.8f), cy = top + (float)(row_at + 1) * (cell + 0.8f);
			const bool lit = row_at == dy && column == dx && !(row_at == 0 && column == 0);

			gc_box(c, cx, cy, cell, cell, 0.7f, lit ? 0xFFE08Au : WELL_EDGE, 255u);
			gc_box(c, cx + 0.3f, cy + 0.3f, cell - 0.6f, cell - 0.6f, 0.5f,
				lit ? GOLD : row_at == 0 && column == 0 ? 0x1C3A6Bu : WELL, 255u);
		}
	for (int button = 0; button < 3; button++)
	{
		const float cx = left + stick + 4.0f + radius + (float)button * (2.0f * radius + 2.4f), cy = top + stick * 0.5f;
		const bool lit = (held & buttons[button].bit) != 0;

		gc_disc(c, cx, cy, radius, lit ? 0xFFE08Au : WELL_EDGE, 255u);
		gc_disc(c, cx, cy, radius - 0.5f, lit ? GOLD : WELL, 255u);
		gc_text(c, GC_FONT_BOLD, 5.0f, cx, gc_middle(GC_FONT_BOLD, 5.0f, cy - 2.5f, 5.0f), buttons[button].name,
			lit ? 0x1A1206u : (on ? LABEL : DIM), GC_CENTRE);
	}
}

/* the session, since the program started: the time played, the games and credits, the best score */
static float session_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 2.0f * (VALUE + 1.2f);
}

static void draw_session(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	const unsigned long seconds = (unsigned long)((double)v->session_frames / 57.55);
	char cells[4][2][24];
	body b;

	b = gadget(c, x, y, w, h, "SESSION", NULL);
	if (chrome_pass)
		return;
	snprintf(cells[0][0], sizeof(cells[0][0]), "PLAYED");
	if (seconds >= 3600u)
		snprintf(cells[0][1], sizeof(cells[0][1]), "%lu:%02lu:%02lu", seconds / 3600u, seconds / 60u % 60u,
			seconds % 60u);
	else
		snprintf(cells[0][1], sizeof(cells[0][1]), "%lu:%02lu", seconds / 60u, seconds % 60u);
	snprintf(cells[1][0], sizeof(cells[1][0]), "GAMES");
	snprintf(cells[1][1], sizeof(cells[1][1]), "%u", v->session_games);
	snprintf(cells[2][0], sizeof(cells[2][0]), "CREDITS");
	snprintf(cells[2][1], sizeof(cells[2][1]), "%u", v->session_credits);
	snprintf(cells[3][0], sizeof(cells[3][0]), "BEST");
	points_text(cells[3][1], sizeof(cells[3][1]), v->session_best, false);
	{
		/* two columns, two rows */
		const float gap = 4.0f, column = (b.w - gap) * 0.5f;

		for (int cell = 0; cell < 4; cell++)
		{
			const float cx = b.x + (float)(cell % 2) * (column + gap), top = b.y + (float)(cell / 2) * (VALUE + 1.2f);
			const float label_w = gc_text_width(GC_FONT_TEXT, SMALL, cells[cell][0]) + 1.5f;

			gc_text(c, GC_FONT_TEXT, SMALL, cx, gc_middle(GC_FONT_TEXT, SMALL, top, VALUE), cells[cell][0], LABEL,
				GC_LEFT);
			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, VALUE, cells[cell][1], column - label_w), cx + column,
				gc_middle(GC_FONT_TEXT, VALUE, top, VALUE), cells[cell][1], cell == 3 && v->session_best ? GOLD : NUMBER,
				GC_RIGHT);
		}
	}
}

static bool areas_two_lines;

static void part_areas(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	draw_areas(c, v, x, y, w, h, areas_two_lines);
}

/* both players in the game: a GET POINT each, and no areas (player 1's alone count) */
static bool two_players(const widgets_view *v)
{
	return v->own && v->in_play[0] && v->in_play[1];
}

static void draw_left(gcanvas *full, const widgets_view *v, float width)
{
	const bool two = two_players(v), wide = width >= 150.0f;
	float heights[6], height, w, used, gap, y = MARGIN, fixed;
	part_fn draws[6];
	int count = 0;
	gcanvas zoom, *c = &zoom;

	if (chrome_pass)
		background(full, width, (float)WIDGET_H);
	heights[count] = chain_height();
	draws[count++] = draw_chain_1;
	if (two)
	{
		heights[count] = chain_height();
		draws[count++] = draw_chain_2;
	}
	heights[count] = rank_height();
	draws[count++] = draw_rank;
	if (!two)
	{
		heights[count] = areas_height(!wide);
		draws[count++] = part_areas;
	}
	heights[count] = 0.0f;    /* (the bees: what is left, at least bees_least) */
	draws[count++] = draw_bees;
	heights[count] = input_height();
	draws[count++] = draw_input;
	if (SESSION_LINE)
	{
		heights[count] = session_height();
		draws[count++] = draw_session;
	}
	fixed = 0.0f;
	for (int at = 0; at < count; at++)
		fixed += heights[at];
	{
		/* (zoomed narrower than 150 units, the areas take two lines each: zoomed again for those) */
		const float full_width = width;

		zoom = zoomed(full, fixed + bees_least(width - 2.0f * MARGIN) + (float)(count - 1) * GAP, &width, &height);
		if (!two && wide && width < 150.0f)
		{
			width = full_width;
			heights[2] = areas_height(true);
			zoom = zoomed(full, fixed - areas_height(false) + areas_height(true) + bees_least(width - 2.0f * MARGIN)
				+ (float)(count - 1) * GAP, &width, &height);
		}
	}
	w = width - 2.0f * MARGIN;
	areas_two_lines = width < 150.0f;
	if (!two)
		heights[2] = areas_height(areas_two_lines);
	used = bees_least(w);
	for (int at = 0; at < count; at++)
		used += heights[at];
	gap = (height - 2.0f * MARGIN - used) / (float)(count - 1);
	if (gap > 12.0f)
		gap = 12.0f;
	if (gap < GAP * 0.5f)
		gap = GAP * 0.5f;
	{
		/* (the bees take what is left over the gaps) */
		const float bees = height - 2.0f * MARGIN - (used - bees_least(w)) - gap * (float)(count - 1);

		for (int at = 0; at < count; at++)
		{
			const float tall = draws[at] == draw_bees ? bees : heights[at];

			part(c, at, draws[at], v, MARGIN, y, w, tall);
			y += tall + gap;
		}
	}
}

/* ---- the right panel ---- */

static float score_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 15.0f + 2.0f + 2.0f * (VALUE + 1.6f);
}

static void draw_score(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	const int player = v->own && !v->in_play[0] && v->in_play[1] ? 1 : 0;
	const uint32_t score = v->score[player];
	char text[32], value[32];
	body b;
	float at, size;

	if (v->area_known)
		snprintf(text, sizeof(text), "AREA %d \x80 LOOP %d", v->area + 1, v->loop + 1);
	else
		text[0] = '\0';
	b = gadget(c, x, y, w, h, "SCORE", text[0] ? text : NULL);
	if (chrome_pass)
		return;
	points_text(value, sizeof(value), v->own ? score : 0u, false);
	size = gc_fit(GC_FONT_BOLD, 15.0f, value, b.w - 1.0f);
	gc_text_outlined(c, GC_FONT_BOLD, size, b.x + b.w, b.y + 13.0f, value, v->own ? HEADER_TEXT : DIM,
		v->own ? CYAN : WELL_EDGE, 0.5f, GC_RIGHT);
	at = b.y + 17.0f;
	if (!v->own || !v->graph_running)
	{
		points_text(value, sizeof(value), v->table[0], false);
		row(c, &b, at, v->own ? "TOP" : "NO GAME", DIM, NULL, DIM);
		at += VALUE + 1.6f;
		row(c, &b, at, "TOP (#1)", LABEL, value, NUMBER);
		return;
	}
	if (v->chasing >= 0)
	{
		const uint32_t chase = v->table[v->chasing];

		snprintf(text, sizeof(text), "NEXT #%d", v->chasing + 1);
		points_text(value, sizeof(value), chase, false);
		row(c, &b, at, text, places[v->chasing], value, places[v->chasing]);
		at += VALUE + 1.6f;
		points_text(value, sizeof(value), chase > score ? chase - score : 0u, false);
		row(c, &b, at, "TO GO", LABEL, value, NUMBER);
	}
	else
	{
		row(c, &b, at, "PAST #1!", GOLD, NULL, GOLD);
		at += VALUE + 1.6f;
		row(c, &b, at, "THE TOP SCORE", LABEL, NULL, LABEL);
	}
}

static float last_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 8.0f;
}

static void draw_last(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	char note[24], value[24];
	const bool on = v->own && v->gained;
	body b;

	snprintf(note, sizeof(note), "AT %u HIT", v->last_gain_hits);
	b = gadget(c, x, y, w, h, "LAST SCORED", on && v->last_gain_hits ? note : NULL);
	if (chrome_pass)
		return;
	points_text(value, sizeof(value), on ? v->last_gain : 0u, on);
	gc_text(c, GC_FONT_BOLD, gc_fit(GC_FONT_BOLD, 8.0f, value, b.w), b.x + b.w, b.y + 7.0f, on ? value : "-",
		on ? GOLD : DIM, GC_RIGHT);
}

/* the score graph: player 1's score through the game as an area against the #1 as the game began (player 2's a
   line), the 100% and 50% lines, a mark where each area began, the places of the table passed (a line in their
   colour where, with their number) and the one chased (dashed) */
static float graph_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 40.0f + 5.0f;
}

static void draw_graph(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	static const uint32_t fill[2] = { ORANGE, 0xB8520Fu };
	static const float fill_at[2] = { 0.0f, 1.0f };
	char note[24];
	body b;
	float most = 1.0f, gx, gy, gw, gh;
	const int count = v->graph_count;
	const float now = v->target ? (float)v->score[0] / (float)v->target : 0.0f;

	snprintf(note, sizeof(note), "VS #1 \x80 %d%%", (int)(now * 100.0f + 0.5f));
	b = gadget(c, x, y, w, h, "SCORE GRAPH", v->own && v->graph_running ? note : "VS #1");
	if (chrome_pass)
		return;
	gx = b.x;
	gy = b.y;
	gw = b.w;
	gh = b.h - 5.0f;
	gc_box(c, gx, gy, gw, gh, 1.0f, WELL_EDGE, 255u);
	gc_box(c, gx + 0.3f, gy + 0.3f, gw - 0.6f, gh - 0.6f, 0.8f, 0x050B1Au, 255u);
	gx += 0.6f;
	gy += 0.6f;
	gw -= 1.2f;
	gh -= 1.2f;
	if (v->own && v->graph_running && count > 0)
	{
		const float step = gw / (float)WIDGET_GRAPH_POINTS;

		for (int player = 0; player < 2; player++)
			for (int at = 0; at < count; at++)
				if (v->played[player][at] && v->points[player][at] > most)
					most = v->points[player][at];
		for (int at = 0; at < count; at++)
		{
			const float tall = v->played[0][at] ? v->points[0][at] / most * gh : 0.0f;

			if (tall > 0.05f)
				gc_gradient(c, gx + (float)at * step, gy + gh - tall, step + 0.05f, tall, 0.0f, fill, fill_at, 2);
		}
		for (int at = 1; at < count; at++)
			if (v->played[1][at] && v->played[1][at - 1])
				gc_line(c, gx + ((float)at - 0.5f) * step, gy + gh - v->points[1][at - 1] / most * gh,
					gx + ((float)at + 0.5f) * step, gy + gh - v->points[1][at] / most * gh, 0.6f, CYAN, 255u);
		for (int mark = 0; mark < v->mark_count; mark++)
			gc_box(c, gx + (float)v->marks[mark] * step, gy, 0.3f, gh, 0.0f, 0x6F86AEu, 150u);
		for (int place = 4; place >= 0; place--)
		{
			/* the table's places: passed, a dotted line at their score and one where, with their number; chased,
			   dashed */
			const float py = gy + gh - (float)v->table[place] / (float)v->target / most * gh;

			if (!v->table[place] || py < gy)
				continue;
			if (v->passed_at[place] >= 0)
			{
				const float px = gx + (float)v->passed_at[place] * step;
				char digit[2] = { (char)('1' + place), 0 };

				for (float dx = gx; dx < gx + gw; dx += 2.0f)
					gc_box(c, dx, py, 0.6f, 0.3f, 0.0f, places[place], 255u);
				gc_box(c, px, gy, 0.35f, gh, 0.0f, places[place], 255u);
				gc_text(c, GC_FONT_BOLD, 3.2f, px + 0.8f, gy + 3.0f + 3.4f * (float)(4 - place), digit, places[place],
					GC_LEFT);
			}
			else if (place == v->chasing)
				for (float dx = gx; dx < gx + gw; dx += 3.0f)
					gc_box(c, dx, py, 1.6f, 0.35f, 0.0f, places[place], 255u);
		}
	}
	/* the 100% and 50% lines */
	for (float dx = gx; dx < gx + gw; dx += 1.5f)
	{
		gc_box(c, dx, gy + gh - gh / most + 0.2f, 0.5f, 0.35f, 0.0f, HEADER_TEXT, 255u);
		gc_box(c, dx, gy + gh - gh / most * 0.5f, 0.5f, 0.3f, 0.0f, 0x6F86AEu, 255u);
	}
	gc_text(c, GC_FONT_TEXT, 3.2f, b.x, b.y + b.h - 0.8f, "START", DIM, GC_LEFT);
	gc_text(c, GC_FONT_TEXT, 3.2f, b.x + b.w, b.y + b.h - 0.8f, "NOW", DIM, GC_RIGHT);
}

/* the area: how far to its boss, or the mid-boss's or boss's health as one is fought */
static float area_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 6.0f;
}

static void draw_area(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	static const char *const notes[5] = { NULL, "AREA PROGRESS", "MID-BOSS", "BOSS HEALTH", "TRUE BOSS" };
	static const float at[2] = { 0.0f, 1.0f };
	uint32_t colours[2] = { TEAL, CYAN };
	char title[24], text[24];
	body b;

	if (v->area_known)
		snprintf(title, sizeof(title), "AREA %d", v->area + 1);
	else
		snprintf(title, sizeof(title), "AREA");
	b = gadget(c, x, y, w, h, title, notes[v->bar]);
	if (chrome_pass)
		return;
	if (v->bar == WIDGET_BAR_MIDBOSS)
		colours[0] = colours[1] = ORANGE;
	else if (v->bar == WIDGET_BAR_BOSS)
		colours[0] = colours[1] = RED;
	else if (v->bar == WIDGET_BAR_TRUE_BOSS)
		colours[0] = colours[1] = GOLD;
	else if (v->bar == WIDGET_BAR_PROGRESS && v->percent >= 100)
		colours[0] = colours[1] = RED;
	bar(c, b.x, b.y, b.w, 6.0f, v->bar == WIDGET_BAR_NONE ? 0.0f : (float)v->percent / 100.0f, colours, at, 2);
	if (v->bar == WIDGET_BAR_PROGRESS && v->percent < 100)
		snprintf(text, sizeof(text), "%s TO THE BOSS", v->bar_text);
	else
		snprintf(text, sizeof(text), "%s", v->bar == WIDGET_BAR_NONE ? "-" : v->bar_text);
	gc_text_outlined(c, GC_FONT_BOLD, 3.8f, b.x + b.w * 0.5f, gc_middle(GC_FONT_BOLD, 3.8f, b.y, 6.0f), text,
		HEADER_TEXT, INNER, 0.35f, GC_CENTRE);
}

/* the board: the game's frames a second, then the bullets and the settings in use (or the FPS meter's figures) */
static float board_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 10.0f + 1.5f + 3.0f * (VALUE + 1.0f);
}

static void draw_board(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	char text[24], cells[6][2][16];
	uint32_t colours[6];
	body b;
	float at;

	snprintf(text, sizeof(text), "OF %.2f", v->board_rate);
	b = gadget(c, x, y, w, h, "BOARD", text);
	if (chrome_pass)
		return;
	if (v->status.slowdown)
	{
		const float size = 9.0f;

		snprintf(text, sizeof(text), "%.1f", v->game_rate);
		gc_text(c, GC_FONT_BOLD, size, b.x, b.y + 8.0f, text, v->lagged_now ? YELLOW : GREEN, GC_LEFT);
		gc_text(c, GC_FONT_TEXT, SMALL, b.x + gc_text_width(GC_FONT_BOLD, size, text) + 1.5f, b.y + 8.0f, "FPS", LABEL,
			GC_LEFT);
	}
	else
		gc_text(c, GC_FONT_BOLD, 6.0f, b.x, b.y + 7.0f, "SLOWDOWN OFF", NUMBER, GC_LEFT);
	if (v->status.fast_forward > 0)
	{
		snprintf(text, sizeof(text), ">> %dX", v->status.fast_forward);
		gc_text(c, GC_FONT_BOLD, 5.0f, b.x + b.w, b.y + 8.0f, text, YELLOW, GC_RIGHT);
	}
	at = b.y + 11.5f;
	if (v->meter.on)
	{
		static const char *const names[6] = { "GAME", "BOARD", "WINDOW", "LATE", "WORK MS", "MOST MS" };
		const double values[6] = { v->meter.game, v->meter.board, v->meter.windows, (double)v->meter.late,
			v->meter.work, v->meter.work_most };

		for (int cell = 0; cell < 6; cell++)
		{
			snprintf(cells[cell][0], sizeof(cells[cell][0]), "%s", names[cell]);
			snprintf(cells[cell][1], sizeof(cells[cell][1]), cell == 3 ? "%.0f" : "%.1f", values[cell]);
			colours[cell] = NUMBER;
		}
		if (v->meter.game < v->meter.board - 0.5)
			colours[0] = YELLOW;
		if (v->meter.board < 57.0)
			colours[1] = RED;
		if (v->meter.late)
			colours[3] = RED;
		if (v->meter.work_most > 17.4)
			colours[5] = YELLOW;
	}
	else
	{
		static const char *const names[6] = { "TIMING", "FREE PLAY", "CHEATS", "AUTOBOMB", "BULLETS", "SLOWDOWN" };

		for (int cell = 0; cell < 6; cell++)
		{
			snprintf(cells[cell][0], sizeof(cells[cell][0]), "%s", names[cell]);
			colours[cell] = NUMBER;
		}
		snprintf(cells[0][1], sizeof(cells[0][1]), "%s", v->status.arcade_timing ? "ARCADE" : "FAST");
		snprintf(cells[1][1], sizeof(cells[1][1]), "%s", v->status.free_play ? "ON" : "OFF");
		snprintf(cells[2][1], sizeof(cells[2][1]), "%s", v->status.cheats ? "ON" : "OFF");
		snprintf(cells[3][1], sizeof(cells[3][1]), "%s", v->status.autobomb ? "ON" : "OFF");
		snprintf(cells[4][1], sizeof(cells[4][1]), "%u", v->bullets);
		snprintf(cells[5][1], sizeof(cells[5][1]), "%s", v->status.slowdown ? "ON" : "OFF");
		colours[2] = v->status.cheats ? RED : NUMBER;
		colours[3] = v->status.autobomb ? YELLOW : NUMBER;
		colours[4] = v->bullets >= 50u ? YELLOW : NUMBER;
	}
	{
		/* two columns, three rows */
		const float gap = 4.0f, column = (b.w - gap) * 0.5f;

		for (int cell = 0; cell < 6; cell++)
		{
			const body half = { b.x + (float)(cell % 2) * (column + gap), b.y, column, b.h };
			const float top = at + (float)(cell / 2) * (VALUE + 1.0f);

			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, SMALL, cells[cell][0], column * 0.6f), half.x,
				gc_middle(GC_FONT_TEXT, SMALL, top, VALUE), cells[cell][0], LABEL, GC_LEFT);
			gc_text(c, GC_FONT_TEXT, VALUE, half.x + half.w, gc_middle(GC_FONT_TEXT, VALUE, top, VALUE), cells[cell][1],
				colours[cell], GC_RIGHT);
		}
	}
}

static float goals_height(void)
{
	return HEADER + 2.0f * FRAME + 1.6f * PAD + 5.0f * 6.6f;
}

/* The goals: the achievements not yet earned that this game is nearest to (achievements_goal), chosen again as an
   area begins and every 10 seconds (between, a line keeps its place: met, it is ticked; lost, crossed); under them,
   to make five, the area's own (NO MISS, NO BOMB, ALL 13 BEES, the next CHAIN, the next score) with a gold dot when
   earned before. With no game that counts (training, a replay, a cheat): the area's own, as they stand */
#define GOALS 5
#define GOALS_EVERY 575    /* (frames drawn: 10 seconds) */

static const char *const own_goals[5] = { "no_miss_area", "no_bomb_area", "bees_area", "chain30", "score1m" };

static struct
{
	int index[GOALS], count;
	int area;
	unsigned frames;
} goals;

/* one of the area's own: the next of its kind for the CHAIN and the score (those after it follow in the list) */
static int own_goal(int line)
{
	int index = achievements_find(own_goals[line]);

	if (line >= 3)
		for (int step = 0; step < 2 && index >= 0 && achievements_date(index); step++)
			index++;
	return index;
}

static void goals_choose(const widgets_view *v)
{
	int order[64], found = 0;
	float parts[64];

	goals.frames++;
	if (!v->own || !v->area_known)
	{
		goals.count = 0;
		return;
	}
	if (goals.count && goals.area == v->area && goals.frames < GOALS_EVERY)
		return;
	goals.area = v->area;
	goals.frames = 0;
	/* (the ones not earned and in reach, nearest first) */
	for (int index = 0; index < achievements_count() && found < 64; index++)
	{
		achievement_goal goal;

		if (achievements_date(index) || !achievements_goal(index, &goal) || goal.state == GOAL_LOST)
			continue;
		{
			int at = found++;

			while (at > 0 && parts[at - 1] < goal.part)
			{
				order[at] = order[at - 1];
				parts[at] = parts[at - 1];
				at--;
			}
			order[at] = index;
			parts[at] = goal.part;
		}
	}
	goals.count = 0;
	if (!found)
		return;    /* (a game that doesn't count: the area's own) */
	for (int at = 0; at < found && goals.count < GOALS; at++)
		goals.index[goals.count++] = order[at];
	for (int line = 0; line < 5 && goals.count < GOALS; line++)
	{
		const int index = own_goal(line);
		bool shown = false;

		for (int other = 0; other < goals.count; other++)
			shown |= goals.index[other] == index;
		if (index >= 0 && !shown)
			goals.index[goals.count++] = index;
	}
}

static void draw_goals(gcanvas *c, const widgets_view *v, float x, float y, float w, float h, bool narrow)
{
	enum { KEPT, LOST, GOING };
	const bool on = v->own;
	static const char *const labels[5] = { "NO MISS", "NO BOMB", "ALL 13 BEES", "CHAIN", "SCORE" };
	char title[48], note[24];
	body b;

	snprintf(title, sizeof(title), narrow ? "GOALS" : "ACHIEVEMENTS");
	if (v->area_known)
		snprintf(title + strlen(title), sizeof(title) - strlen(title), " \x80 AREA %d", v->area + 1);
	snprintf(note, sizeof(note), "%d / %d", achievements_done(), achievements_count());
	b = gadget(c, x, y, w, h, title, note);
	if (chrome_pass)
		return;
	for (int line = 0; line < GOALS; line++)
	{
		const float top = b.y + (float)line * 6.6f, box_size = 4.4f;
		const int index = goals.count ? (line < goals.count ? goals.index[line] : -1) : own_goal(line);
		achievement_goal goal;
		bool earned, have;
		int state;
		uint32_t name_colour;

		if (index < 0)
			continue;
		have = achievements_goal(index, &goal);
		if (!have && !goals.count)
		{
			/* (no game that counts: the area's own, by name) */
			goal.label = line >= 3 ? achievements_name(index) : labels[line];
			goal.value[0] = '\0';
			goal.state = GOAL_GOING;
			goal.part = 0.0f;
		}
		else if (!have)
		{
			/* (out of this game's reach since it was chosen: its name, crossed) */
			goal.label = achievements_name(index);
			goal.value[0] = '\0';
			goal.state = GOAL_LOST;
			goal.part = 0.0f;
		}
		earned = achievements_date(index) != NULL;
		state = goal.state == GOAL_LOST ? LOST : goal.state == GOAL_KEPT ? KEPT : GOING;
		if (index == achievements_find("bees_area") && state == GOING && v->bee_area_over)
			state = LOST;    /* (the area's bees over: one was missed) */
		name_colour = !on ? DIM : state == LOST ? 0x7F98C2u : NUMBER;
		if (!on)
			gc_box(c, b.x, top, box_size, box_size, 0.8f, 0x1C3A6Bu, 255u);
		else if (state == KEPT)
		{
			gc_box(c, b.x, top, box_size, box_size, 0.8f, TEAL, 255u);
			gc_line(c, b.x + 0.9f, top + 2.3f, b.x + 1.9f, top + 3.3f, 0.6f, INNER, 255u);
			gc_line(c, b.x + 1.9f, top + 3.3f, b.x + 3.6f, top + 1.0f, 0.6f, INNER, 255u);
		}
		else if (state == LOST)
		{
			gc_box(c, b.x, top, box_size, box_size, 0.8f, 0x5A1620u, 255u);
			gc_line(c, b.x + 1.1f, top + 1.1f, b.x + 3.3f, top + 3.3f, 0.6f, RED, 255u);
			gc_line(c, b.x + 3.3f, top + 1.1f, b.x + 1.1f, top + 3.3f, 0.6f, RED, 255u);
		}
		else
		{
			/* (under way: the box filled up from the bottom as it nears) */
			const float filled = (box_size - 1.0f) * (goal.part < 0.0f ? 0.0f : goal.part > 1.0f ? 1.0f : goal.part);

			gc_box(c, b.x, top, box_size, box_size, 0.8f, 0x1C3A6Bu, 255u);
			if (filled > 0.15f)
				gc_box(c, b.x + 0.5f, top + box_size - 0.5f - filled, box_size - 1.0f, filled, 0.4f, 0x3B7CF0u, 255u);
		}
		gc_text(c, GC_FONT_TEXT, 4.0f, b.x + box_size + 2.0f, top + 3.7f, goal.label, name_colour, GC_LEFT);
		if (earned)
			gc_disc(c, b.x + box_size + 2.0f + gc_text_width(GC_FONT_TEXT, 4.0f, goal.label) + 1.8f, top + 2.2f, 0.9f,
				GOLD, 255u);
		if (on && goal.value[0])
		{
			/* (the value made to fit beside the name) */
			const float room = b.w - box_size - 2.0f - gc_text_width(GC_FONT_TEXT, 4.0f, goal.label) - 4.5f;

			gc_text(c, GC_FONT_TEXT, gc_fit(GC_FONT_TEXT, 3.6f, goal.value, room), b.x + b.w, top + 3.7f, goal.value,
				LABEL, GC_RIGHT);
		}
	}
}

static bool goals_narrow;

static void part_goals(gcanvas *c, const widgets_view *v, float x, float y, float w, float h)
{
	draw_goals(c, v, x, y, w, h, goals_narrow);
}

/* an achievement earned: its box over the score's gadget, as the classic panel's */
static void draw_toast(gcanvas *c, const widgets_view *v, float width)
{
	const float x = MARGIN, y = MARGIN, w = width - 2.0f * MARGIN, h = 40.0f;
	const int shown = 230 - v->toast_frames;
	const uint32_t rim = shown < 30 && shown / 5 % 2 ? 0xFFFFFFu : GOLD;
	char more[24];

	gc_box(c, x - 0.3f, y + 0.6f, w + 0.6f, h + 0.6f, RADIUS, 0x000000u, 140u);
	gc_box(c, x, y, w, h, RADIUS, rim, 255u);
	gc_box(c, x + 0.9f, y + 0.9f, w - 1.8f, h - 1.8f, RADIUS - 0.6f, 0x14142Au, 255u);
	gc_text(c, GC_FONT_BOLD, 4.4f, x + w * 0.5f, y + 8.0f, "ACHIEVEMENT UNLOCKED", GOLD, GC_CENTRE);
	gc_text(c, GC_FONT_BOLD, gc_fit(GC_FONT_BOLD, 8.0f, v->toast, w - 6.0f), x + w * 0.5f, y + 21.0f, v->toast,
		HEADER_TEXT, GC_CENTRE);
	if (v->toast_more > 0)
	{
		snprintf(more, sizeof(more), "+%d MORE", v->toast_more);
		gc_text(c, GC_FONT_TEXT, 3.6f, x + w * 0.5f, y + 31.0f, more, LABEL, GC_CENTRE);
	}
}

static void draw_right(gcanvas *full, const widgets_view *v, float width)
{
	const float heights[6] = { score_height(), last_height(), graph_height(), area_height(), board_height(),
		goals_height() };
	static const part_fn draws[6] = { draw_score, draw_last, draw_graph, draw_area, draw_board, part_goals };
	float height, w, used = 0.0f, gap, y = MARGIN;
	gcanvas zoom, *c = &zoom;

	if (chrome_pass)
		background(full, width, (float)WIDGET_H);
	for (int at = 0; at < 6; at++)
		used += heights[at];
	zoom = zoomed(full, used + 5.0f * GAP, &width, &height);
	w = width - 2.0f * MARGIN;
	goals_narrow = width < 140.0f;
	gap = (height - 2.0f * MARGIN - used) / 5.0f;
	if (gap > 12.0f)
		gap = 12.0f;
	if (gap < GAP * 0.5f)
		gap = GAP * 0.5f;
	for (int at = 0; at < 6; at++)
	{
		/* (the graph takes what is left over the gaps) */
		const float tall = at == 2 ? heights[at] + (height - 2.0f * MARGIN - used - 5.0f * gap) : heights[at];

		part(c, at, draws[at], v, MARGIN, y, w, tall);
		y += tall + gap;
	}
	if (v->toast_frames > 0 && !chrome_pass)
		draw_toast(c, v, width);
}

/* ---- the panels ---- */

static int clamp_width(int width)
{
	return width < WIDGET_W ? WIDGET_W : width > WIDGET_MAX_W ? WIDGET_MAX_W : width;
}

static struct
{
	int width, k;
	bool two;                   /* laid out for two players (the left panel's chrome differs) */
	uint32_t *left, *right;
} chrome;

void gadgets_invalidate(void)
{
	kept.fresh[0] = kept.fresh[1] = false;
}

void gadgets_draw(uint32_t *left, uint32_t *right, int width, int k, gadgets_dirty *dirty)
{
	widgets_view v;
	gcanvas c;
	size_t pixels;
	uint32_t *const outs[2] = { left, right };

	width = clamp_width(width);
	pixels = (size_t)width * (size_t)k * (size_t)WIDGET_H * (size_t)k;
	widgets_view_now(&v);
	c.w = width * k;
	c.h = WIDGET_H * k;
	c.k = (float)k;
	c.clip = 0;
	c.hash = NULL;
	if (dirty)
		dirty->count[0] = dirty->count[1] = 0;
	if (chrome.width != width || chrome.k != k || chrome.two != two_players(&v))
	{
		/* the chrome for this width, k and layout: drawn once */
		free(chrome.left);
		free(chrome.right);
		chrome.width = chrome.k = 0;
		gadgets_invalidate();
		chrome.left = malloc(pixels * sizeof(uint32_t));
		chrome.right = malloc(pixels * sizeof(uint32_t));
		if (chrome.left && chrome.right)
		{
			chrome_pass = true;
			c.pixels = chrome.left;
			draw_left(&c, &v, (float)width);
			c.pixels = chrome.right;
			draw_right(&c, &v, (float)width);
			chrome_pass = false;
			chrome.width = width;
			chrome.k = k;
			chrome.two = two_players(&v);
		}
	}
	goals_choose(&v);
	/* (the right side whole while an achievement's box is over it and the frame after, and after the help) */
	if (v.toast_frames > 0 || kept.toast || kept.help)
		kept.fresh[1] = false;
	kept.toast = v.toast_frames > 0;
	kept.help = false;
	dirty_now = dirty;
	for (int side = 0; side < 2; side++)
	{
		if (outs[side] != kept.pixels[side])
			kept.fresh[side] = false;
		side_now = side;
		chrome_now = chrome.width ? (side ? chrome.right : chrome.left) : NULL;
		c.pixels = outs[side];
		if (!kept.fresh[side] || !chrome_now)
		{
			/* (drawn whole: the chrome, or drawn again when there is no memory for it) */
			if (chrome_now)
				memcpy(outs[side], chrome_now, pixels * sizeof(uint32_t));
			else
			{
				chrome_pass = true;
				(side ? draw_right : draw_left)(&c, &v, (float)width);
				chrome_pass = false;
			}
			if (dirty)
			{
				const gadgets_rect whole = { 0, 0, c.w, c.h };

				dirty->rect[side][0] = whole;
				dirty->count[side] = 1;
			}
		}
		(side ? draw_right : draw_left)(&c, &v, (float)width);
		kept.fresh[side] = chrome_now != NULL;
		kept.pixels[side] = outs[side];
		if (dirty && dirty->count[side] > 1)
		{
			/* (a whole side once drawn whole: the gadgets' places after it are in it) */
			if (dirty->rect[side][0].w == c.w && dirty->rect[side][0].h == c.h)
				dirty->count[side] = 1;
		}
	}
	dirty_now = NULL;
}

void gadgets_draw_help(uint32_t *pixels, int width, int k, const char *title, const char *text)
{
	gcanvas c;
	body b;
	float y;

	width = clamp_width(width);
	c.w = width * k;
	c.h = WIDGET_H * k;
	c.k = (float)k;
	c.clip = 0;
	c.hash = NULL;
	c.pixels = pixels;
	kept.help = true;
	background(&c, (float)width, (float)WIDGET_H);
	chrome_pass = true;
	gadget(&c, MARGIN, MARGIN, (float)width - 2.0f * MARGIN, (float)WIDGET_H - 2.0f * MARGIN, "HELP", NULL);
	chrome_pass = false;
	b = gadget(&c, MARGIN, MARGIN, (float)width - 2.0f * MARGIN, (float)WIDGET_H - 2.0f * MARGIN, "HELP", NULL);
	y = b.y + 5.0f;
	for (int part = 0; part < 2; part++)
	{
		/* the entry's name (gold, larger), then what it does: in lines broken at their spaces to the body's width */
		const char *at = part ? text : title;
		const int font = part ? GC_FONT_TEXT : GC_FONT_BOLD;
		const float size = part ? 4.2f : 5.4f, line = part ? 6.0f : 7.2f;

		while (*at && y < b.y + b.h)
		{
			char buffer[96];
			int take = 0, last_space = -1;

			while (at[take] && take < (int)sizeof(buffer) - 1)
			{
				buffer[take] = at[take];
				buffer[take + 1] = '\0';
				if (gc_text_width(font, size, buffer) > b.w)
					break;
				if (at[take] == ' ')
					last_space = take;
				take++;
			}
			if (at[take] && last_space > 0)
				take = last_space;
			if (take == 0)
				take = 1;
			snprintf(buffer, sizeof(buffer), "%.*s", take, at);
			gc_text(&c, font, size, b.x, y, buffer, part ? NUMBER : GOLD, GC_LEFT);
			y += line;
			at += take;
			while (*at == ' ')
				at++;
		}
		if (!part)
		{
			gc_box(&c, b.x, y - 3.0f, b.w, 0.3f, 0.0f, WELL_EDGE, 255u);
			y += 3.0f;
		}
	}
}
