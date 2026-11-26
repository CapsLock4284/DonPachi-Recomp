/**
 *    Name:        gadget_canvas.h
 *    Description: drawing for the gadget panels (gadgets.h): filled and rounded boxes, gradients, and the panels' text
 *                 (gadget_font.h: Chakra Petch) filled smooth at any size, on a picture of 0x00RRGGBB pixels
 *    Version:     0.1.0 (draft)
 *    Created:     25-11-2026
 *
 *    license:BSD-3-Clause
 *    Everything is placed in the panel's units (the game's pixels: a panel is WIDGET_H of them tall) and drawn at
 *    'k' of the screen's pixels to each, so the panels are as sharp as the screen. The text: each glyph's outline
 *    (its curves flattened) filled with its exact coverage (the area of each pixel inside it: the accumulation way
 *    of font-rs), kept in a cache once drawn at a size. Colours are 0x00RRGGBB; 'alpha' 0-255.
 */
#ifndef DONPACHI_GADGET_CANVAS_H
#define DONPACHI_GADGET_CANVAS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint32_t *pixels;
	int w, h;                  /* in pixels */
	float k;                   /* pixels a unit */
	int clip;                  /* boxes drawn only left of this pixel (0: the whole width) */
	uint32_t *hash;            /* not NULL: nothing drawn, each drawing's place, size, colours and text folded into
	                              it instead (gc_record: what a gadget would draw, to tell whether it changed) */
} gcanvas;

/* Data folded into a recording canvas's hash (as each gc_ function folds its own) */
void gc_record(gcanvas *c, const void *data, unsigned size);

enum { GC_FONT_TEXT, GC_FONT_BOLD };    /* Chakra Petch SemiBold, Bold */
enum { GC_LEFT, GC_CENTRE, GC_RIGHT };

/* A box, its corners rounded by 'radius' (units; 0 square), smooth at its edges */
void gc_box(gcanvas *c, float x, float y, float w, float h, float radius, uint32_t colour, unsigned alpha);

/* A box shaded from top to bottom through 'count' colours at 'stops' (0-1, rising) */
void gc_gradient(gcanvas *c, float x, float y, float w, float h, float radius, const uint32_t *colours,
	const float *stops, int count);

/* The same from left to right */
void gc_gradient_across(gcanvas *c, float x, float y, float w, float h, float radius, const uint32_t *colours,
	const float *stops, int count);

/* Lines across a box, as brushed metal: every 'step' units, 'alpha' of 'colour' */
void gc_brushed(gcanvas *c, float x, float y, float w, float h, float step, uint32_t colour, unsigned alpha);

/* A circle's disc (centre and radius in units) */
void gc_disc(gcanvas *c, float cx, float cy, float radius, uint32_t colour, unsigned alpha);

/* A straight line 'width' units wide */
void gc_line(gcanvas *c, float x0, float y0, float x1, float y1, float width, uint32_t colour, unsigned alpha);

/* Text: its width at 'size' (the em, units) */
float gc_text_width(int font, float size, const char *text);

/* Text at (x, baseline y), its 'align' to x; drawn with an outline 'stroke' units wide in 'outline' (stroke 0:
   none). Characters past ASCII: 127 the multiplication sign, 128 the middle dot (gadget_font.h) */
void gc_text(gcanvas *c, int font, float size, float x, float y, const char *text, uint32_t colour, int align);
void gc_text_outlined(gcanvas *c, int font, float size, float x, float y, const char *text, uint32_t colour,
	uint32_t outline, float stroke, int align);

/* The largest size up to 'size' at which 'text' fits in 'width' units */
float gc_fit(int font, float size, const char *text, float width);

/* The baseline's place for text of 'size' set in the middle of a band from y, 'height' tall */
float gc_middle(int font, float size, float y, float height);

#endif
