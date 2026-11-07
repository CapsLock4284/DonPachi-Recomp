"""
Name:        donpachi_rom.py
Description: MAME 'donpachi' ROM set (DonPachi, US, Ver.1.12 1995/05/2x) and its regions
Version:     0.1.0 (draft)
Created:     06-11-2026

Modelled on tmnt_rom.py from tmntarcade-decomp. Files are identified by CRC32 + SHA1, not by name.
Accepts a folder or the .zip as is.
Region layouts follow ROM_START( donpachi ) in MAME's cave.cpp (BSD-3-Clause).

Board: Cave 1st generation, AT-C01DP-2: one 68000 and no Z80. The sound is two OKI M6295 chips with an
NMK 112 sample bank chip, so there is no sound program and no build_sound().

NOT YET TESTED against a real ROM set. Run check_vectors(build_program(load_set(...))) first.
"""
import hashlib
import zipfile
import zlib
from pathlib import Path

SET_NAME = "donpachi"

# MAME name -> (crc32, sha1, size)
ROM_FILES = {
    "prgu.u29": ("89c36802", "7857c726cecca5a4fce282e0d2b873774d2c1b1d", 0x080000),
    "atdp.u44": ("7189e953", "53adbe6ea5e01ecb48575e9db82cc3d0dc8a3726", 0x200000),
    "atdp.u45": ("6984173f", "625dd6674adeb206815855b8b6a1fba79ed5c4cd", 0x200000),
    "atdp.u54": ("6bda6b66", "6472e6706505bac17484fb8bf4e8922ced4adf63", 0x100000),
    "atdp.u57": ("0a0e72b9", "997e8253777e7acca5a1c0c4026e78eecc122d5d", 0x100000),
    "text.u58": ("5dba06e7", "f9dab7f6c732a683fddb4cae090a875b3962332b", 0x040000),
    "atdp.u32": ("0d89fcca", "e16ed15fa5e72537822f7b37e83ccfed0fa87338", 0x100000),
    "atdp.u33": ("d749de00", "64a0acc23eb2515e7d0459f0289919e083c63afc", 0x200000),
    "eeprom-donpachi.u10": ("315fb546", "7f597107d1610fc286413e0e93c794c80c0c554f", 0x000080),
    "peel18cv8p-15.u18": ("3f4787e9", "fc7da25c9f36c9cbc6ba5a7314c4828d405d1261", 0x000155),
}

# Files that do not affect the program, so a set without them still loads (the PAL is the board's logic, not data)
OPTIONAL_FILES = {"peel18cv8p-15.u18"}

# The 68000 program image: 0x000000-0x07FFFF, no mirrors (work RAM is at 0x100000)
PROGRAM_AREAS = [
    (0x000000, 0x080000),  # prgu.u29, 16-bit words byte-swapped (ROM_LOAD16_WORD_SWAP)
]
PROGRAM_SIZE = 0x080000

# MAME's region sizes, for checks
OKI1_SIZE = 0x240000
OKI2_SIZE = 0x340000
OKI_EMPTY = 0x040000   # "leave the 0x40000 bytes addressable by the chip empty"


def read_files(source):
    """Return {original_name: bytes} from a folder or a .zip."""
    source = Path(source)
    contents = {}
    if source.is_file() and source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            for entry in archive.infolist():
                if not entry.is_dir():
                    contents[Path(entry.filename).name] = archive.read(entry)
        return contents
    for path in sorted(source.iterdir()):
        if path.is_file():
            contents[path.name] = path.read_bytes()
    return contents


def identify_set(source):
    """Return ({mame_name: bytes}, [report lines]), matching every file by its hashes. A folder that holds
    none of the set's files loose is read through the first .zip in it that does."""
    found, report = _identify(source)
    if not found and Path(source).is_dir():
        for archive in sorted(Path(source).glob("*.zip")):
            found, report = _identify(archive)
            if found:
                break
    return found, report


def _identify(source):
    by_crc = {crc: name for name, (crc, _, _) in ROM_FILES.items()}
    found = {}
    report = []
    for original_name, data in read_files(source).items():
        crc = format(zlib.crc32(data) & 0xFFFFFFFF, "08x")
        mame_name = by_crc.get(crc)
        if mame_name and ROM_FILES[mame_name][1] == hashlib.sha1(data).hexdigest():
            found[mame_name] = data
            report.append(f"OK      {original_name:20} = {mame_name}")
        else:
            report.append(f"--      {original_name:20} not part of the set (crc {crc})")
    for mame_name in sorted(set(ROM_FILES) - set(found)):
        tag = "OPTION " if mame_name in OPTIONAL_FILES else "MISSING"
        report.append(f"{tag} {mame_name}")
    return found, report


def load_set(source):
    """Like identify_set, but stops when a required file is missing."""
    found, _ = identify_set(source)
    missing = sorted(set(ROM_FILES) - OPTIONAL_FILES - set(found))
    if missing:
        raise SystemExit(f"The '{SET_NAME}' ROM set is incomplete. Missing: " + ", ".join(missing))
    return found


def swap16(data):
    """ROM_LOAD16_WORD_SWAP: the file holds 16-bit words in the opposite byte order to the big-endian region."""
    result = bytearray(len(data))
    result[0::2] = data[1::2]
    result[1::2] = data[0::2]
    return bytes(result)


def build_program(rom):
    """maincpu region (512 KB, big endian) as the 68000 sees it at 0x000000-0x07FFFF."""
    return swap16(rom["prgu.u29"])


def build_sprites(rom):
    """sprites0 region (4 MB): the sprite graphics, word swapped, in file order. MAME's init_donpachi also runs
    unpack_sprites(0) on this; that step is not done here."""
    return swap16(rom["atdp.u44"]) + swap16(rom["atdp.u45"])


def build_layer0(rom):
    """layer0 region (1 MB): tilemap layer 0's 8x8 4-bit tiles."""
    return bytes(rom["atdp.u54"])


def build_layer1(rom):
    """layer1 region (1 MB): tilemap layer 1's 8x8 4-bit tiles."""
    return bytes(rom["atdp.u57"])


def build_layer2(rom):
    """layer2 region (256 KB): the text / character layer's tiles."""
    return bytes(rom["text.u58"])


def build_oki1(rom):
    """oki1 region (2.25 MB): OKI #1's samples, after 256 KB the chip can address but the board leaves empty."""
    return bytes(OKI_EMPTY) + bytes(rom["atdp.u33"])


def build_oki2(rom):
    """oki2 region (3.25 MB): OKI #2's samples: the same empty 256 KB, then atdp.u32, then atdp.u33 again."""
    return bytes(OKI_EMPTY) + bytes(rom["atdp.u32"]) + bytes(rom["atdp.u33"])


def build_eeprom(rom):
    """eeprom region (128 bytes): the 93C46's contents as shipped, loaded as is."""
    return bytes(rom["eeprom-donpachi.u10"])


def check_vectors(program):
    """Sanity check of the byte swap: the 68000's reset vectors. The first long is the stack pointer (it should
    point into work RAM, 0x100000-0x110000) and the second the first instruction (inside the program ROM).
    Returns a list of problems, empty if it looks right."""
    problems = []
    stack = int.from_bytes(program[0:4], "big")
    start = int.from_bytes(program[4:8], "big")
    if not 0x100000 <= stack <= 0x110000:
        problems.append(f"initial stack pointer {stack:08X} is not in work RAM (100000-10FFFF): wrong byte order?")
    if not 0x8 <= start < PROGRAM_SIZE or start & 1:
        problems.append(f"reset address {start:08X} is not an even address inside the program: wrong byte order?")
    return problems


def canonical_address(address):
    """Map a 68000 address to its offset in the program image, or None if it is not ROM.

    The 68000 drives 24 address lines: the top byte of an address is not decoded."""
    address &= 0xFFFFFF
    return address if address < PROGRAM_SIZE else None


class ProgramMemory:
    """68000 view of the program ROM for the disassembler: big endian, anything outside it reads 0xFF."""

    def __init__(self, image):
        self.image = image

    def r8(self, address):
        offset = canonical_address(address)
        return self.image[offset] if offset is not None else 0xFF

    def r16(self, address):
        return (self.r8(address) << 8) | self.r8(address + 1)

    def r32(self, address):
        return (self.r16(address) << 16) | self.r16(address + 2)
