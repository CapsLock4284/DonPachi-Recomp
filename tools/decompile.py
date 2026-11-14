"""
Name:        decompile.py
Description: runs the shared core's decompile.py (tools/core) for this game: the 68000 program as readable C in
             src/, with the names of symbols.txt; or the Japanese version's (--set japan) in src_jpn/
Version:     0.3.0 (draft)
Created:     07-11-2026

The script is the core's (arcade-decomp, which tools/core holds); tools/game_profile.py tells it about this game
(DECOMPILER: the 68000's driver, m68000/decompile.py; the board's memory map, its interrupt handler's frame).
Its usage is the core's:
  python tools/decompile.py            src/ (game.h, ram.h, table.c and the functions' files)
  python tools/decompile.py --check    what does not lift
and --set japan for the Japanese version ('donpachij', in the rom folder with donpachi.zip, its parent):
  python tools/decompile.py --set japan    src_jpn/
symbols.txt's names are the US program's: for the Japanese one they are carried over to where the same code is
(tools/version_map.py) into build/decompile_jpn/symbols_jpn.txt (made each time: symbols.txt stays the one list),
those in code the Japanese program doesn't share left out (listed there); tools/call_tables.py's addresses the same
way (game_profile.here). Same wrapper as tmntarcade-decomp's tools/decompile.py, with one addition:
tools/call_tables.py teaches the core's 68000 analysis the calls and jumps DonPachi makes straight into tables of
stubs, which the core does not follow (it is installed here, before the core runs; tools/core itself is not changed).
"""
import os
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

SETS = {"us": "donpachi", "donpachi": "donpachi", "japan": "donpachij", "donpachij": "donpachij"}
arguments = sys.argv[1:]
if "--set" in arguments:
    position = arguments.index("--set")
    if position + 1 >= len(arguments) or arguments[position + 1].lower() not in SETS:
        raise SystemExit("--set us or --set japan")
    os.environ["DONPACHI_SET"] = SETS[arguments[position + 1].lower()]
    del arguments[position:position + 2]


def carry_symbols(source, target, here):
    """symbols.txt's lines with their program addresses moved to the Japanese program's (here(): None where the code
    differs, the line left out); the work RAM's (var) as they are. How many were left out"""
    lines, left_out = [], []
    for line in source.read_text(encoding="utf-8").splitlines():
        parts = line.split("#", 1)[0].split()
        if len(parts) >= 3 and parts[1] in ("func", "code", "data", "file") and all(
                c in "0123456789abcdefABCDEF" for c in parts[0]):
            address = int(parts[0], 16)
            moved = here(address)
            if moved is None:
                left_out.append(line)
                lines.append(f"# (not in the Japanese program) {line}")
                continue
            start = line.index(parts[0])
            line = line[:start] + f"{moved:06X}" + line[start + len(parts[0]):]
        lines.append(line)
    header = ["# Made by tools/decompile.py --set japan from symbols.txt (the US program's names), each address",
              "# moved to where the Japanese program has the same code (tools/version_map.py). The comments' addresses are",
              f"# still the US program's. Left out, in code the Japanese program doesn't share: {len(left_out)}", ""]
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(header + lines) + "\n", encoding="utf-8")
    return left_out


if os.environ.get("DONPACHI_SET") == "donpachij":
    import game_profile  # noqa: E402
    import m68000.decompile  # noqa: E402

    reports = ROOT / "build" / "decompile_jpn"
    symbols = reports / "symbols_jpn.txt"
    game_profile.here(0x000420)    # (the two programs aligned)
    left_out = carry_symbols(ROOT / "symbols.txt", symbols, game_profile._address_map)
    print(f"symbols:    {symbols} (symbols.txt's names moved; {len(left_out)} in code that differs, left out)")
    m68000.decompile.REPORTS = reports
    if "--symbols" not in arguments:
        arguments += ["--symbols", str(symbols)]
    if "--out" not in arguments:
        arguments += ["--out", str(ROOT / "src_jpn")]
sys.argv[1:] = arguments

import call_tables  # noqa: E402


def prefix_header(out_dir):
    """out_dir/jpn_names.h: every name the Japanese program's C defines (its functions, the tables' copies, the
    registry: game_code, game_entry, game_tables...), as jpn_NAME. The build includes it before each of its files
    (CMakeLists.txt, tests/harness/build.sh), so the two programs' C goes into one game without a name twice; the
    runtime picks the program by the ROM set (runtime/src/game_program.c). Found as the C has them: what starts at
    a line's first column and is defined there (not static, typedef or extern)"""
    import re
    names = set()
    definition = re.compile(r"(?:const\s+)?(?:unsigned\s+)?[A-Za-z_]\w*(?:\s+const)?[\s*]+(?:const\s+)?\**\s*"
                            r"([A-Za-z_]\w*)\s*(\(|\[|=|;)")
    for path in sorted(out_dir.rglob("*.c")):
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if not line or line[0] in " \t#/{}*" or line.startswith(
                    ("static", "typedef", "extern", "struct ", "enum ", "union ")):
                continue
            found = definition.match(line)
            if found:
                names.add(found.group(1))
    lines = ["/* The Japanese program's names, each as jpn_NAME: included before each file of src_jpn/ (made by",
             "   tools/decompile.py --set japan), so that its C and the US program's go into one game */",
             "#ifndef DONPACHI_JPN_NAMES_H", "#define DONPACHI_JPN_NAMES_H", ""]
    lines += [f"#define {name} jpn_{name}" for name in sorted(names)]
    lines += ["", "#endif", ""]
    (out_dir / "jpn_names.h").write_text("\n".join(lines), encoding="ascii")
    print(f"prefix:     {out_dir / 'jpn_names.h'} ({len(names)} names, jpn_)")


call_tables.install()
try:
    runpy.run_path(str(TOOLS / "core" / "decompile.py"), run_name="__main__")
except SystemExit as stopped:
    if os.environ.get("DONPACHI_SET") == "donpachij" and not stopped.code and "--check" not in arguments:
        prefix_header(Path(arguments[arguments.index("--out") + 1]))
    raise
