"""make_gadget_font.py: the gadget panels' font (ports/sdl2/gadgets.c), from Chakra Petch's font files, as
outlines in C.

    python tools/make_gadget_font.py FOLDER > ports/sdl2/gadget_font.h

FOLDER holds Chakra Petch's TrueType files (Google Fonts: https://github.com/google/fonts/tree/main/ofl/chakrapetch):
ChakraPetch-SemiBold.ttf for the panels' text and ChakraPetch-Bold.ttf for their headings and big numbers. Each glyph
the panels use (ASCII, and the multiplication sign and middle dot) is written as its outline: its curves flattened to
points (within a 512th of the em of the curve), in the font's units, y up. The digits are made all as wide as the
widest (each centred in it: tabular), so a number that changes doesn't shift. gadget_canvas.c fills them at any size,
so the panels' text is sharp at the screen's own resolution. Chakra Petch is the Chakra Petch Project Authors' (SIL
Open Font License 1.1: ports/sdl2/fonts/OFL-ChakraPetch.txt); the header is a part of it, under that licence too.
Needs fontTools (pip install fonttools).
"""
import os
import sys

from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont

FONTS = (("text", "ChakraPetch-SemiBold.ttf"), ("bold", "ChakraPetch-Bold.ttf"))
CHARACTERS = [chr(code) for code in range(32, 127)] + ["×", "·"]    # (ASCII, then 127 x, 128 middle dot)


class Flatten(BasePen):
    """The outline as polygons: lines kept, curves cut into short lines"""

    def __init__(self, glyph_set, tolerance):
        super().__init__(glyph_set)
        self.tolerance = tolerance
        self.contours = []
        self.points = []

    def _moveTo(self, point):
        self._end()
        self.points = [point]

    def _lineTo(self, point):
        self.points.append(point)

    def _steps(self, bend):
        """The lines a curve becomes: as few as keep each within the tolerance of the curve ('bend' its control
        points' second difference: how far it bends)"""
        return max(1, min(32, int((0.75 * bend / self.tolerance) ** 0.5 + 0.999)))

    def _curveToOne(self, p1, p2, p3):
        p0 = self.points[-1]
        bend = max(((p0[0] - 2 * p1[0] + p2[0]) ** 2 + (p0[1] - 2 * p1[1] + p2[1]) ** 2) ** 0.5,
                   ((p1[0] - 2 * p2[0] + p3[0]) ** 2 + (p1[1] - 2 * p2[1] + p3[1]) ** 2) ** 0.5) * 3
        steps = self._steps(bend)
        for step in range(1, steps + 1):
            t = step / steps
            u = 1 - t
            self.points.append((u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
                                u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]))

    def _qCurveToOne(self, p1, p2):
        p0 = self.points[-1]
        bend = ((p0[0] - 2 * p1[0] + p2[0]) ** 2 + (p0[1] - 2 * p1[1] + p2[1]) ** 2) ** 0.5 * 2
        steps = self._steps(bend)
        for step in range(1, steps + 1):
            t = step / steps
            u = 1 - t
            self.points.append((u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
                                u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]))

    def _closePath(self):
        self._end()

    def _endPath(self):
        self._end()

    def _end(self):
        if len(self.points) >= 3:
            self.contours.append([(int(round(x)), int(round(y))) for x, y in self.points])
        self.points = []


def glyph_name(font, character):
    """The glyph for a character"""
    return font.getBestCmap().get(ord(character))


def write_font(label, path, out):
    font = TTFont(path)
    units = font["head"].unitsPerEm
    glyph_set = font.getGlyphSet()
    points, ends, glyphs = [], [], []
    digit_width = max(glyph_set[glyph_name(font, digit)].width for digit in "0123456789")
    for character in CHARACTERS:
        name = glyph_name(font, character)
        if name is None:
            glyphs.append((len(ends), 0, glyph_set[".notdef"].width))
            continue
        pen = Flatten(glyph_set, units / 512)
        glyph_set[name].draw(pen)
        width, shift = glyph_set[name].width, 0
        if character.isdigit():
            width, shift = digit_width, (digit_width - glyph_set[name].width) // 2    # (tabular: centred)
        first = len(ends)
        for contour in pen.contours:
            points.extend((x + shift, y) for x, y in contour)
            ends.append(len(points))
        glyphs.append((first, len(pen.contours), width))
    hhea = font["hhea"]
    out.append(f"/* {os.path.basename(path)}: {len(points)} points, {len(ends)} contours */")
    out.append(f"static const int16_t gf_{label}_points[{len(points) * 2}] = {{")
    flat = [value for point in points for value in point]
    for at in range(0, len(flat), 20):
        out.append("\t" + ", ".join(str(value) for value in flat[at:at + 20]) + ",")
    out.append("};")
    out.append(f"static const uint32_t gf_{label}_ends[{len(ends)}] = {{")
    for at in range(0, len(ends), 16):
        out.append("\t" + ", ".join(str(value) for value in ends[at:at + 16]) + ",")
    out.append("};")
    out.append(f"static const gadget_glyph gf_{label}_glyphs[{len(glyphs)}] = {{")
    for at in range(0, len(glyphs), 6):
        out.append("\t" + " ".join(f"{{ {g[0]}, {g[1]}, {g[2]} }}," for g in glyphs[at:at + 6]))
    out.append("};")
    out.append(f"static const gadget_font gf_{label} = {{ {units}, {hhea.ascent}, {hhea.descent}, "
               f"gf_{label}_points, gf_{label}_ends, gf_{label}_glyphs }};")
    out.append("")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = ["/**",
           " *    Name:        gadget_font.h",
           " *    Description: the gadget panels' font: Chakra Petch's outlines (tools/make_gadget_font.py writes it)",
           " *",
           " *    license:OFL-1.1",
           " *    Chakra Petch, Copyright 2018 The Chakra Petch Project Authors (https://github.com/m4rc1e/Chakra-Petch.git):",
           " *    its glyphs' outlines, under the SIL Open Font License 1.1 (ports/sdl2/fonts/OFL-ChakraPetch.txt). Written",
           " *    by tools/make_gadget_font.py from ChakraPetch-SemiBold.ttf and ChakraPetch-Bold.ttf: not to be edited by",
           " *    hand. The digits made tabular (each as wide as the widest, centred).",
           " *    The characters: 32-126 (ASCII), then 127 the multiplication sign, 128 the middle dot.",
           " */",
           "#ifndef DONPACHI_GADGET_FONT_H",
           "#define DONPACHI_GADGET_FONT_H",
           "",
           "#include <stdint.h>",
           "",
           "typedef struct { uint16_t first_contour, contours; int16_t advance; } gadget_glyph;",
           "typedef struct",
           "{",
           "\tint16_t units, ascent, descent;          /* the em's units; above and below the baseline */",
           "\tconst int16_t *points;                    /* x, y pairs, y up */",
           "\tconst uint32_t *ends;                     /* each contour's end in the points (its last + 1) */",
           "\tconst gadget_glyph *glyphs;               /* characters 32-128 */",
           "} gadget_font;",
           ""]
    for label, file in FONTS:
        write_font(label, os.path.join(sys.argv[1], file), out)
    out.append("#endif")
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
