/**
 *    Name:        gadget_canvas.c
 *    Description: drawing for the gadget panels (gadget_canvas.h)
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    The glyphs' filling after font-rs (Raph Levien, Apache-2.0/MIT: its accumulation rasterizer, written again
 *    here): each line of the outline adds its signed area to the cells it crosses, and a running sum along the
 *    rows gives each pixel's coverage.
 */
#include "gadget_canvas.h"
#include "gadget_font.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- recording: a gadget's drawing as a hash (gcanvas.hash) ---- */

void gc_record(gcanvas *c, const void *data, unsigned size)
{
	const unsigned char *const bytes = data;
	uint32_t hash = *c->hash;

	for (unsigned at = 0; at < size; at++)
		hash = (hash ^ bytes[at]) * 16777619u;
	*c->hash = hash;
}

/* (a drawing's operation, its numbers (the canvas's scale and clip with them) and colour folded in: true when the
   canvas records, and nothing is drawn) */
static bool recorded(gcanvas *c, int op, const float *numbers, unsigned count, uint32_t colour, unsigned alpha)
{
	if (!c->hash)
		return false;
	{
		const float place[2] = { c->k, (float)c->clip };

		gc_record(c, &op, sizeof(op));
		gc_record(c, place, sizeof(place));
		gc_record(c, numbers, count * (unsigned)sizeof(float));
		gc_record(c, &colour, sizeof(colour));
		gc_record(c, &alpha, sizeof(alpha));
	}
	return true;
}

/* ---- pixels ---- */

static void blend(uint32_t *pixel, uint32_t colour, unsigned alpha)
{
	if (alpha >= 255u)
	{
		*pixel = colour;
		return;
	}
	if (alpha == 0u)
		return;
	{
		const uint32_t old = *pixel, keep = 255u - alpha;
		const uint32_t r = ((old >> 16 & 255u) * keep + (colour >> 16 & 255u) * alpha + 127u) / 255u;
		const uint32_t g = ((old >> 8 & 255u) * keep + (colour >> 8 & 255u) * alpha + 127u) / 255u;
		const uint32_t b = ((old & 255u) * keep + (colour & 255u) * alpha + 127u) / 255u;

		*pixel = r << 16 | g << 8 | b;
	}
}

static uint32_t mix(uint32_t a, uint32_t b, float t)
{
	const float u = 1.0f - t;
	const uint32_t r = (uint32_t)((float)(a >> 16 & 255u) * u + (float)(b >> 16 & 255u) * t + 0.5f);
	const uint32_t g = (uint32_t)((float)(a >> 8 & 255u) * u + (float)(b >> 8 & 255u) * t + 0.5f);
	const uint32_t bl = (uint32_t)((float)(a & 255u) * u + (float)(b & 255u) * t + 0.5f);

	return r << 16 | g << 8 | bl;
}

/* how much of the pixel whose centre is (px, py) is inside the rounded box (pixels): its distance from the edge */
static float inside(float px, float py, float x0, float y0, float x1, float y1, float r)
{
	const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
	const float hx = (x1 - x0) * 0.5f - r, hy = (y1 - y0) * 0.5f - r;
	const float qx = fabsf(px - cx) - hx, qy = fabsf(py - cy) - hy;
	const float ox = qx > 0.0f ? qx : 0.0f, oy = qy > 0.0f ? qy : 0.0f;
	const float d = sqrtf(ox * ox + oy * oy) + (qx > qy ? (qx < 0.0f ? qx : 0.0f) : (qy < 0.0f ? qy : 0.0f)) - r;
	const float cover = 0.5f - d;

	return cover < 0.0f ? 0.0f : cover > 1.0f ? 1.0f : cover;
}

typedef uint32_t (*shade_fn)(const void *how, float px, float py);

static bool row_shade(shade_fn shade, const void *how);

static void fill(gcanvas *c, float x, float y, float w, float h, float radius, shade_fn shade, const void *how,
	unsigned alpha)
{
	const float x0 = x * c->k, y0 = y * c->k, x1 = (x + w) * c->k, y1 = (y + h) * c->k;
	float r = radius * c->k;
	int top = (int)floorf(y0), bottom = (int)ceilf(y1), left = (int)floorf(x0), right = (int)ceilf(x1);

	if (r > (x1 - x0) * 0.5f)
		r = (x1 - x0) * 0.5f;
	if (r > (y1 - y0) * 0.5f)
		r = (y1 - y0) * 0.5f;
	if (top < 0)
		top = 0;
	if (left < 0)
		left = 0;
	if (bottom > c->h)
		bottom = c->h;
	if (right > c->w)
		right = c->w;
	if (c->clip > 0 && right > c->clip)
		right = c->clip;
	/* (the whole pixels between the sides' edges, in the rows clear of the corners and the box's top and bottom) */
	const int whole_left = (int)ceilf(x0) > left ? (int)ceilf(x0) : left;
	const int whole_right = (int)floorf(x1) < right ? (int)floorf(x1) : right;
	/* (and the pixels between the corners: a pixel's distance from the edge there goes by its row alone) */
	const int middle_left = (int)ceilf(x0 + r + 1.0f) > left ? (int)ceilf(x0 + r + 1.0f) : left;
	const int middle_right = (int)floorf(x1 - r - 1.0f) < right ? (int)floorf(x1 - r - 1.0f) : right;

	for (int py = top; py < bottom; py++)
	{
		uint32_t *const row = c->pixels + (size_t)py * (size_t)c->w;
		const float cy = (float)py + 0.5f;
		const bool plain = cy - 0.5f >= y0 + r && cy + 0.5f <= y1 - r && whole_left < whole_right;

		for (int px = left; px < right; px++)
		{
			const float cx = (float)px + 0.5f;
			float cover;

			if (plain && px == whole_left)
			{
				/* (the run of them: the shade the same along a row when it is flat or goes down the box) */
				if (row_shade(shade, how))
				{
					const uint32_t colour = shade(how, cx, cy);

					if (alpha >= 255u)
						for (; px < whole_right; px++)
							row[px] = colour;
					else
						for (; px < whole_right; px++)
							blend(row + px, colour, alpha);
				}
				else
					for (; px < whole_right; px++)
						blend(row + px, shade(how, (float)px + 0.5f, cy), alpha);
				px--;
				continue;
			}
			cover = inside(cx, cy, x0, y0, x1, y1, r);
			if (!plain && px >= middle_left && px < middle_right)
			{
				/* (a row through the corners or along the top or bottom: its cover the same between the corners) */
				const unsigned a = (unsigned)(cover * (float)alpha + 0.5f);

				if (a)
				{
					if (row_shade(shade, how))
					{
						const uint32_t colour = shade(how, cx, cy);

						for (; px < middle_right; px++)
							blend(row + px, colour, a);
					}
					else
						for (; px < middle_right; px++)
							blend(row + px, shade(how, (float)px + 0.5f, cy), a);
				}
				else
					px = middle_right;
				px--;
				continue;
			}
			if (cover > 0.0f)
				blend(row + px, shade(how, cx, cy), (unsigned)(cover * (float)alpha + 0.5f));
		}
	}
}

static uint32_t flat_shade(const void *how, float px, float py)
{
	(void)px;
	(void)py;
	return *(const uint32_t *)how;
}

void gc_box(gcanvas *c, float x, float y, float w, float h, float radius, uint32_t colour, unsigned alpha)
{
	const float numbers[5] = { x, y, w, h, radius };

	if (recorded(c, 1, numbers, 5, colour, alpha))
		return;
	fill(c, x, y, w, h, radius, flat_shade, &colour, alpha);
}

typedef struct
{
	const uint32_t *colours;
	const float *stops;
	int count;
	float from, length;         /* pixels */
	bool across;
} gradient;

static uint32_t gradient_shade(const void *how, float px, float py)
{
	const gradient *const g = how;
	const float t = ((g->across ? px : py) - g->from) / g->length;

	if (t <= g->stops[0])
		return g->colours[0];
	for (int at = 1; at < g->count; at++)
		if (t <= g->stops[at])
			return mix(g->colours[at - 1], g->colours[at], (t - g->stops[at - 1]) / (g->stops[at] - g->stops[at - 1]));
	return g->colours[g->count - 1];
}

/* whether the shade is the same all along a row of pixels */
static bool row_shade(shade_fn shade, const void *how)
{
	return shade == flat_shade || (shade == gradient_shade && !((const gradient *)how)->across);
}

void gc_gradient(gcanvas *c, float x, float y, float w, float h, float radius, const uint32_t *colours,
	const float *stops, int count)
{
	const gradient g = { colours, stops, count, y * c->k, h * c->k, false };
	const float numbers[5] = { x, y, w, h, radius };

	if (recorded(c, 2, numbers, 5, 0u, (unsigned)count))
	{
		gc_record(c, colours, (unsigned)count * (unsigned)sizeof(uint32_t));
		gc_record(c, stops, (unsigned)count * (unsigned)sizeof(float));
		return;
	}

	fill(c, x, y, w, h, radius, gradient_shade, &g, 255u);
}

void gc_gradient_across(gcanvas *c, float x, float y, float w, float h, float radius, const uint32_t *colours,
	const float *stops, int count)
{
	const gradient g = { colours, stops, count, x * c->k, w * c->k, true };
	const float numbers[5] = { x, y, w, h, radius };

	if (recorded(c, 3, numbers, 5, 0u, (unsigned)count))
	{
		gc_record(c, colours, (unsigned)count * (unsigned)sizeof(uint32_t));
		gc_record(c, stops, (unsigned)count * (unsigned)sizeof(float));
		return;
	}

	fill(c, x, y, w, h, radius, gradient_shade, &g, 255u);
}

static void brushed(gcanvas *c, float x, float y, float w, float h, float step, uint32_t colour, unsigned alpha)
{
	/* (a hair line each step: whole pixels, at least two apart) */
	const float every = step * c->k < 2.0f ? 2.0f : step * c->k;
	const int top = (int)(y * c->k), bottom = (int)((y + h) * c->k);
	int left = (int)(x * c->k), right = (int)((x + w) * c->k);

	if (left < 0)
		left = 0;
	if (right > c->w)
		right = c->w;
	for (float fx = (float)left; fx < (float)right; fx += every)
		for (int py = top < 0 ? 0 : top; py < bottom && py < c->h; py++)
			blend(c->pixels + (size_t)py * (size_t)c->w + (int)fx, colour, alpha);
}

void gc_brushed(gcanvas *c, float x, float y, float w, float h, float step, uint32_t colour, unsigned alpha)
{
	const float numbers[5] = { x, y, w, h, step };

	if (!recorded(c, 4, numbers, 5, colour, alpha))
		brushed(c, x, y, w, h, step, colour, alpha);
}

void gc_disc(gcanvas *c, float cx, float cy, float radius, uint32_t colour, unsigned alpha)
{
	const float numbers[3] = { cx, cy, radius };

	if (recorded(c, 5, numbers, 3, colour, alpha))
		return;
	fill(c, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, radius, flat_shade, &colour, alpha);
}

static void line(gcanvas *c, float x0, float y0, float x1, float y1, float width, uint32_t colour, unsigned alpha)
{
	/* (each pixel near it by its distance from the line's middle: smooth at its sides, square at its ends) */
	const float ax = x0 * c->k, ay = y0 * c->k, bx = x1 * c->k, by = y1 * c->k, half = width * c->k * 0.5f;
	const float dx = bx - ax, dy = by - ay, length = sqrtf(dx * dx + dy * dy);
	int left = (int)floorf((ax < bx ? ax : bx) - half - 1.0f), right = (int)ceilf((ax > bx ? ax : bx) + half + 1.0f);
	int top = (int)floorf((ay < by ? ay : by) - half - 1.0f), bottom = (int)ceilf((ay > by ? ay : by) + half + 1.0f);

	if (length <= 0.0f)
		return;
	if (left < 0)
		left = 0;
	if (top < 0)
		top = 0;
	if (right > c->w)
		right = c->w;
	if (bottom > c->h)
		bottom = c->h;
	for (int py = top; py < bottom; py++)
		for (int px = left; px < right; px++)
		{
			const float qx = (float)px + 0.5f - ax, qy = (float)py + 0.5f - ay;
			const float along = (qx * dx + qy * dy) / length, across = fabsf(qx * dy - qy * dx) / length;
			float cover = half + 0.5f - across;

			if (along < -0.5f || along > length + 0.5f || cover <= 0.0f)
				continue;
			if (cover > 1.0f)
				cover = 1.0f;
			blend(c->pixels + (size_t)py * (size_t)c->w + px, colour, (unsigned)(cover * (float)alpha + 0.5f));
		}
}

void gc_line(gcanvas *c, float x0, float y0, float x1, float y1, float width, uint32_t colour, unsigned alpha)
{
	const float numbers[5] = { x0, y0, x1, y1, width };

	if (!recorded(c, 6, numbers, 5, colour, alpha))
		line(c, x0, y0, x1, y1, width, colour, alpha);
}

/* ---- the glyphs ---- */

static const gadget_font *font_of(int font)
{
	return font == GC_FONT_BOLD ? &gf_bold : &gf_text;
}

static int glyph_index(unsigned char character)
{
	return character >= 32u && character <= 128u ? (int)character - 32 : '?' - 32;
}

/* a glyph filled at a size: its coverage (0-255), and where it sits from the pen (pixels: its left, and its top
   above the baseline) */
typedef struct
{
	bool used;
	int font, index, size_q, spread_q;    /* (the size and the outline's spread, in quarters of a pixel) */
	int w, h, left, top;
	uint8_t *cover;
} cached_glyph;

#define CACHE 2048

static cached_glyph cache[CACHE];

/* one line of the outline into the accumulation (font-rs's draw_line) */
static void accumulate(float *acc, int w, int h, float x0, float y0, float x1, float y1)
{
	float dir = 1.0f, dxdy, x;
	int y_first, y_end;

	if (y0 == y1)
		return;
	if (y0 > y1)
	{
		float t = x0;

		x0 = x1;
		x1 = t;
		t = y0;
		y0 = y1;
		y1 = t;
		dir = -1.0f;
	}
	dxdy = (x1 - x0) / (y1 - y0);
	x = x0;
	if (y0 < 0.0f)
		x -= y0 * dxdy;
	y_first = y0 < 0.0f ? 0 : (int)y0;
	y_end = (int)ceilf(y1);
	if (y_end > h)
		y_end = h;
	for (int y = y_first; y < y_end; y++)
	{
		float *const line = acc + (size_t)y * (size_t)w;
		const float dy = ((float)(y + 1) < y1 ? (float)(y + 1) : y1) - ((float)y > y0 ? (float)y : y0);
		const float xnext = x + dxdy * dy, d = dy * dir;
		float a0 = x < xnext ? x : xnext, a1 = x < xnext ? xnext : x;
		const float x0floor = floorf(a0), x1ceil = ceilf(a1);
		const int x0i = (int)x0floor;

		if (x1ceil <= x0floor + 1.0f)
		{
			const float xmf = 0.5f * (x + xnext) - x0floor;

			line[x0i] += d - d * xmf;
			line[x0i + 1] += d * xmf;
		}
		else
		{
			const float s = 1.0f / (a1 - a0), x0f = a0 - x0floor, a0area = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
			const float x1f = a1 - x1ceil + 1.0f, am = 0.5f * s * x1f * x1f;
			const int x1i = (int)x1ceil;

			line[x0i] += d * a0area;
			if (x1i == x0i + 2)
				line[x0i + 1] += d * (1.0f - a0area - am);
			else
			{
				const float a1area = s * (1.5f - x0f);

				line[x0i + 1] += d * (a1area - a0area);
				for (int xi = x0i + 2; xi < x1i - 1; xi++)
					line[xi] += d * s;
				line[x1i - 1] += d * (1.0f - (a1area + (float)(x1i - x0i - 3) * s) - am);
			}
			line[x1i] += d * am;
		}
		x = xnext;
	}
}

static cached_glyph *glyph(int font, int index, float size, float spread)
{
	const int size_q = (int)(size * 4.0f + 0.5f), spread_q = (int)(spread * 4.0f + 0.5f);
	uint32_t key = ((uint32_t)font << 28 | (uint32_t)index << 20 | (uint32_t)size_q << 8 | (uint32_t)spread_q)
		* 2654435761u;
	const unsigned slot = (key ^ key >> 15) % CACHE;
	cached_glyph *const g = &cache[slot];
	const gadget_font *const f = font_of(font);
	const gadget_glyph *const gl = &f->glyphs[index];
	const float scale = (float)size_q / 4.0f / (float)f->units, pad = (float)spread_q / 4.0f + 1.0f;
	float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
	float *acc;
	int w, h;

	if (g->used && g->font == font && g->index == index && g->size_q == size_q && g->spread_q == spread_q)
		return g;
	free(g->cover);
	memset(g, 0, sizeof(*g));
	g->used = true;
	g->font = font;
	g->index = index;
	g->size_q = size_q;
	g->spread_q = spread_q;
	if (gl->contours == 0)
		return g;
	for (uint32_t contour = gl->first_contour; contour < (uint32_t)gl->first_contour + gl->contours; contour++)
	{
		const uint32_t start = contour ? f->ends[contour - 1] : 0u;

		for (uint32_t at = start; at < f->ends[contour]; at++)
		{
			const float px = (float)f->points[2 * at] * scale, py = -(float)f->points[2 * at + 1] * scale;

			min_x = px < min_x ? px : min_x;
			max_x = px > max_x ? px : max_x;
			min_y = py < min_y ? py : min_y;
			max_y = py > max_y ? py : max_y;
		}
	}
	g->left = (int)floorf(min_x - pad);
	g->top = (int)floorf(min_y - pad);
	w = (int)ceilf(max_x + pad) - g->left + 2;
	h = (int)ceilf(max_y + pad) - g->top + 1;
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
		return g;
	if (!(acc = calloc((size_t)w * (size_t)h + 2u, sizeof(float))) || !(g->cover = malloc((size_t)w * (size_t)h)))
	{
		free(acc);
		return g;
	}
	g->w = w;
	g->h = h;
	for (uint32_t contour = gl->first_contour; contour < (uint32_t)gl->first_contour + gl->contours; contour++)
	{
		const uint32_t start = contour ? f->ends[contour - 1] : 0u, end = f->ends[contour];

		for (uint32_t at = start; at < end; at++)
		{
			const uint32_t next = at + 1u < end ? at + 1u : start;

			accumulate(acc, w, h, (float)f->points[2 * at] * scale - (float)g->left,
				-(float)f->points[2 * at + 1] * scale - (float)g->top,
				(float)f->points[2 * next] * scale - (float)g->left,
				-(float)f->points[2 * next + 1] * scale - (float)g->top);
		}
	}
	{
		float sum = 0.0f;

		for (size_t at = 0; at < (size_t)w * (size_t)h; at++)
		{
			float a;

			sum += acc[at];
			a = fabsf(sum);
			g->cover[at] = (uint8_t)((a > 1.0f ? 1.0f : a) * 255.0f + 0.5f);
		}
	}
	free(acc);
	if (spread_q > 0)
	{
		/* the outline: the coverage spread by the stroke (each pixel the most within its distance) */
		const float r = (float)spread_q / 4.0f;
		const int reach = (int)ceilf(r);
		uint8_t *const spread_cover = malloc((size_t)w * (size_t)h);

		if (spread_cover)
		{
			for (int y = 0; y < h; y++)
				for (int x = 0; x < w; x++)
				{
					unsigned most = 0;

					for (int dy = -reach; dy <= reach; dy++)
						for (int dx = -reach; dx <= reach; dx++)
						{
							const int sx = x + dx, sy = y + dy;
							const float distance = sqrtf((float)(dx * dx + dy * dy));
							float weight;
							unsigned value;

							if (sx < 0 || sy < 0 || sx >= w || sy >= h || distance > r + 0.5f)
								continue;
							weight = r + 0.5f - distance;
							weight = weight > 1.0f ? 1.0f : weight;
							value = (unsigned)((float)g->cover[(size_t)sy * (size_t)w + (size_t)sx] * weight);
							most = value > most ? value : most;
						}
					spread_cover[(size_t)y * (size_t)w + (size_t)x] = (uint8_t)most;
				}
			free(g->cover);
			g->cover = spread_cover;
		}
	}
	return g;
}

static void draw_glyph(gcanvas *c, const cached_glyph *g, int pen_x, int base_y, uint32_t colour)
{
	if (!g->cover)
		return;
	for (int y = 0; y < g->h; y++)
	{
		const int py = base_y + g->top + y;

		if (py < 0 || py >= c->h)
			continue;
		for (int x = 0; x < g->w; x++)
		{
			const int px = pen_x + g->left + x;
			const unsigned a = g->cover[(size_t)y * (size_t)g->w + (size_t)x];

			if (a && px >= 0 && px < c->w)
				blend(c->pixels + (size_t)py * (size_t)c->w + px, colour, a);
		}
	}
}

float gc_text_width(int font, float size, const char *text)
{
	const gadget_font *const f = font_of(font);
	float width = 0.0f;

	for (const unsigned char *at = (const unsigned char *)text; *at; at++)
		width += (float)f->glyphs[glyph_index(*at)].advance * size / (float)f->units;
	return width;
}

static void text(gcanvas *c, int font, float size, float x, float y, const char *string, uint32_t colour,
	uint32_t outline, float stroke, int align)
{
	const gadget_font *const f = font_of(font);
	const float px_size = size * c->k, width = gc_text_width(font, size, string);
	float pen = (align == GC_RIGHT ? x - width : align == GC_CENTRE ? x - width * 0.5f : x) * c->k;
	const int base = (int)(y * c->k + 0.5f);

	for (int pass = stroke > 0.0f ? 0 : 1; pass < 2; pass++)
	{
		float at_x = pen;

		for (const unsigned char *at = (const unsigned char *)string; *at; at++)
		{
			const int index = glyph_index(*at);

			draw_glyph(c, glyph(font, index, px_size, pass == 0 ? stroke * c->k : 0.0f), (int)(at_x + 0.5f), base,
				pass == 0 ? outline : colour);
			at_x += (float)f->glyphs[index].advance * px_size / (float)f->units;
		}
	}
}

/* (text recorded: its font, size, place, alignment, colours and characters) */
static bool text_recorded(gcanvas *c, int font, float size, float x, float y, const char *string, uint32_t colour,
	uint32_t outline, float stroke, int align)
{
	const float numbers[6] = { (float)font, size, x, y, stroke, (float)align };

	if (!recorded(c, 7, numbers, 6, colour, outline))
		return false;
	gc_record(c, string, (unsigned)strlen(string) + 1u);
	return true;
}

void gc_text(gcanvas *c, int font, float size, float x, float y, const char *string, uint32_t colour, int align)
{
	if (!text_recorded(c, font, size, x, y, string, colour, 0u, 0.0f, align))
		text(c, font, size, x, y, string, colour, 0u, 0.0f, align);
}

void gc_text_outlined(gcanvas *c, int font, float size, float x, float y, const char *string, uint32_t colour,
	uint32_t outline, float stroke, int align)
{
	if (!text_recorded(c, font, size, x, y, string, colour, outline, stroke, align))
		text(c, font, size, x, y, string, colour, outline, stroke, align);
}

float gc_fit(int font, float size, const char *string, float width)
{
	const float now = gc_text_width(font, size, string);

	return now <= width || now <= 0.0f ? size : size * width / now;
}

float gc_middle(int font, float size, float y, float height)
{
	/* (the capitals' height, about 0.7 of the em in Chakra Petch, centred in the band) */
	(void)font;
	return y + height * 0.5f + size * 0.35f;
}
