"""
Name:        extract_assets.py
Description: pulls the graphics and the samples out of the user's own DonPachi ROM set: the three tilemap layers and
             the sprites as PNG sheets, the OKI M6295 samples as WAV files

Usage (from the repository root; the ROM set is in rom/):
    python tools/extract_assets.py                 graphics and audio, into assets/
    python tools/extract_assets.py --gfx           graphics only
    python tools/extract_assets.py --audio         audio only
    python tools/extract_assets.py --palette 0x12345   colour the sheets with 16 colours (xGRB 555 words) read from
                                                   that address of the program ROM (default: shades of grey)

The layouts come from MAME's cave.cpp (BSD-3-Clause) for the 'donpachi' set:
  layers 0-2   8x8 tiles, 4 bits a pixel, the left pixel in the high nibble (gfx_8x8x4_packed_msb), 32 bytes a tile
  sprites      16x16 tiles of 16 lines of 16 pixels (layout_sprites_msb), 4 bits a pixel, the left pixel in the high
               nibble, 8 bytes a line; a 16x16 sprite tile is taken to be 16 lines one after another
  samples      two OKI M6295 (4-bit ADPCM) at 4.22 MHz / 4 and / 2, pin 7 high (MAME marks it "not verified"), so
               7992 Hz and 15985 Hz; the sample ROMs are banked by an NMK112 in 64 KB pages that the game chooses
               while it runs

What is a guess, and checked only by looking and listening: the sprite tile arrangement, and for the samples that the
four 64 KB windows of a sample table are consecutive pages from the table's own page. The tables are found by their
shape (see find_tables below). assets/ is in .gitignore: what this writes is derived from the ROMs, so it stays out
of the repository.
"""
import argparse
import array
import struct
import sys
import wave
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import donpachi_rom as rom  # noqa: E402

HIGH = bytes(b >> 4 for b in range(256))
LOW = bytes(b & 15 for b in range(256))
GAP = 16                      # the palette entry between the tiles of a sheet


# ---------------------------------------------------------------------------------------------------- PNG

def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_png(path, width, height, pixels, palette):
    """An 8-bit indexed PNG; pixels is width * height palette indices."""
    raw = b"".join(b"\x00" + bytes(pixels[y * width:(y + 1) * width]) for y in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)
    colours = bytes(value for colour in palette for value in colour)
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"PLTE", colours)
                           + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def make_palette(program, address):
    """17 colours: 16 for the pixels and one for the gap between tiles."""
    if address is None:
        colours = [(i * 17, i * 17, i * 17) for i in range(16)]
    else:
        colours = []
        for i in range(16):
            word = int.from_bytes(program[address + 2 * i:address + 2 * i + 2], "big")   # xGRB 555
            green, red, blue = (word >> 10) & 31, (word >> 5) & 31, word & 31
            colours.append((red * 255 // 31, green * 255 // 31, blue * 255 // 31))
    return colours + [(64, 64, 96)]


# ---------------------------------------------------------------------------------------------------- graphics

def tile_sheets(data, tile_width, tile_height, row_bytes, columns, rows, folder, name, palette):
    """Sheets of columns x rows tiles, in tile order; the tile of a sheet's cell is page * columns * rows + cell."""
    tile_bytes = row_bytes * tile_height
    tiles = len(data) // tile_bytes
    per_sheet = columns * rows
    cell_width, cell_height = tile_width + 1, tile_height + 1
    width, height = columns * cell_width + 1, rows * cell_height + 1
    folder.mkdir(parents=True, exist_ok=True)
    pages = (tiles + per_sheet - 1) // per_sheet
    for page in range(pages):
        sheet = bytearray([GAP]) * (width * height)
        for cell in range(min(per_sheet, tiles - page * per_sheet)):
            offset = (page * per_sheet + cell) * tile_bytes
            left = 1 + (cell % columns) * cell_width
            top = 1 + (cell // columns) * cell_height
            for y in range(tile_height):
                line = data[offset + y * row_bytes:offset + (y + 1) * row_bytes]
                pixels = bytearray(tile_width)
                pixels[0::2] = line.translate(HIGH)       # the left pixel of a pair is the high nibble
                pixels[1::2] = line.translate(LOW)
                start = (top + y) * width + left
                sheet[start:start + tile_width] = pixels
        write_png(folder / f"{name}_{page:04d}.png", width, height, sheet, palette)
    return tiles, pages


def extract_graphics(files, out, palette):
    layers = (("layer0", rom.build_layer0(files), "layer 0: 8x8"),
              ("layer1", rom.build_layer1(files), "layer 1: 8x8"),
              ("layer2", rom.build_layer2(files), "layer 2 (text): 8x8"))
    for name, data, label in layers:
        tiles, pages = tile_sheets(data, 8, 8, 4, 32, 32, out / "gfx" / name, name, palette)
        print(f"  {label}: {tiles} tiles in {pages} sheets (32 x 32 tiles each), assets/gfx/{name}/")
    sprites = rom.build_sprites(files)
    # a 16x16 sprite tile is 16 lines of 8 bytes, one line after another
    tiles, pages = tile_sheets(sprites, 16, 16, 8, 16, 16, out / "gfx" / "sprites", "sprites", palette)
    print(f"  sprites: {tiles} tiles of 16x16 in {pages} sheets (16 x 16 tiles each), assets/gfx/sprites/")


# ---------------------------------------------------------------------------------------------------- audio

STEPS = [int(16.0 * pow(11.0 / 10.0, step)) for step in range(49)]
SHIFT = (-1, -1, -1, -1, 2, 4, 6, 8)
DIFF = [(-1 if nibble & 8 else 1) * (STEPS[step] * ((nibble >> 2) & 1) + (STEPS[step] // 2) * ((nibble >> 1) & 1)
                                     + (STEPS[step] // 4) * (nibble & 1) + STEPS[step] // 8)
        for step in range(49) for nibble in range(16)]


def decode_adpcm(data):
    """OKI ADPCM as MAME's okim6295 does it: 4 bits a sample, the high nibble first, 12 bits out (made 16)."""
    out = array.array("h")
    signal, step = -2, 0
    for byte in data:
        for nibble in (byte >> 4, byte & 15):
            signal += DIFF[step * 16 + nibble]
            signal = -2048 if signal < -2048 else 2047 if signal > 2047 else signal
            step += SHIFT[nibble & 7]
            step = 0 if step < 0 else 48 if step > 48 else step
            out.append(signal * 16)
    if sys.byteorder == "big":
        out.byteswap()
    return out.tobytes()


def find_tables(data, entries):
    """The 64 KB pages that start with a table of sample addresses: 'entries' records of 8 bytes, each three bytes of
    start, three of end (inclusive), two zeros; an unused one is all zeros. Returns [(page, [(phrase, start, end)])]."""
    found = []
    for page in range(len(data) // 0x10000):
        base = page * 0x10000
        phrases = []
        for number in range(entries):
            record = data[base + 8 * number:base + 8 * number + 8]
            if record[6:8] != b"\x00\x00":
                phrases = None
                break
            start = int.from_bytes(record[0:3], "big")
            end = int.from_bytes(record[3:6], "big")
            if start == 0 and end == 0:
                continue
            if start < 0x400 or end <= start or end >= 0x40000:
                phrases = None
                break
            phrases.append((number, start, end))
        if phrases is not None and len(phrases) >= 3:
            found.append((page, phrases))
    return found


def extract_audio(files, out, rates):
    chips = (("chip0 (music, one table of 128)", files["atdp.u33"], 128, rates[0]),
             ("chip1 (effects, tables of 32 from each page)", files["atdp.u32"] + files["atdp.u33"], 32, rates[1]))
    manifest = ["chip  page  phrase   start     end   bytes  seconds  file"]
    for number, (label, data, entries, rate) in enumerate(chips):
        folder = out / "audio" / f"chip{number}"
        folder.mkdir(parents=True, exist_ok=True)
        tables = find_tables(data, entries)
        written = 0
        for page, phrases in tables:
            base = page * 0x10000
            for phrase, start, end in phrases:
                if base + end >= len(data):
                    continue                      # runs past the ROM: the pages are not consecutive here
                samples = decode_adpcm(data[base + start:base + end + 1])
                name = f"page{page:02d}_phrase{phrase:03d}.wav"
                with wave.open(str(folder / name), "wb") as wav:
                    wav.setnchannels(1)
                    wav.setsampwidth(2)
                    wav.setframerate(rate)
                    wav.writeframes(samples)
                seconds = len(samples) / 2 / rate
                manifest.append(f"{number:>4} {page:>5} {phrase:>7} {start:07X} {end:07X} {end - start + 1:>7} "
                                f"{seconds:>8.2f}  chip{number}/{name}")
                written += 1
        print(f"  {label}: {len(tables)} tables found (pages {', '.join(str(p) for p, _ in tables[:12])}"
              f"{' ...' if len(tables) > 12 else ''}), {written} samples at {rate} Hz, assets/audio/chip{number}/")
        if not tables:
            print("    no table of sample addresses found at the start of any 64 KB page: the layout is not as assumed")
    (out / "audio").mkdir(parents=True, exist_ok=True)
    (out / "audio" / "manifest.txt").write_text("\n".join(manifest) + "\n")


# ---------------------------------------------------------------------------------------------------- main

def main():
    parser = argparse.ArgumentParser(description="Extract DonPachi's graphics and samples from the ROM set in rom/")
    parser.add_argument("--gfx", action="store_true", help="graphics only")
    parser.add_argument("--audio", action="store_true", help="audio only")
    parser.add_argument("--out", default="assets", help="output folder (default: assets)")
    parser.add_argument("--palette", help="address in the program ROM of 16 colours (xGRB 555 words)")
    parser.add_argument("--rate0", type=int, default=7992, help="sample rate of chip 0 (default 7992 Hz)")
    parser.add_argument("--rate1", type=int, default=15985, help="sample rate of chip 1 (default 15985 Hz)")
    parser.add_argument("rom", nargs="?", default="rom", help="ROM folder or zip (default: rom)")
    arguments = parser.parse_args()
    both = not arguments.gfx and not arguments.audio

    files = rom.load_set(arguments.rom)
    out = Path(arguments.out)
    if both or arguments.gfx:
        address = int(arguments.palette, 0) if arguments.palette else None
        palette = make_palette(rom.build_program(files), address)
        print("Graphics:")
        extract_graphics(files, out, palette)
    if both or arguments.audio:
        print("Audio:")
        extract_audio(files, out, (arguments.rate0, arguments.rate1))
    print(f"Done. Look at the sheets and listen to a few samples before relying on any of it: see the notes at the top of"
          f" tools/extract_assets.py.")


if __name__ == "__main__":
    main()
