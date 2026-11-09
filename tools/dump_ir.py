"""
Name:        dump_ir.py
Description: prints the decompiler's own form (the IR) of some functions, after the stack pass, to see why the C
             writer refuses them; with --full, also after the whole analysis the C writer runs on
Version:     0.1.0 (draft)
Created:     08-11-2026

Usage:
    python tools/dump_ir.py 00081A 034BB0 037C22 | Out-File -Encoding utf8 build\\ir.txt
    python tools/dump_ir.py --full 00081A | Out-File -Encoding utf8 build\\ir.txt   (about as long as a decompile)

Loads the program as tools/decompile.py does (tools/call_tables.py installed). Nothing here changes any file.
"""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

import call_tables  # noqa: E402

call_tables.install()

import m68000.decompile as d  # noqa: E402
from decompiler import stack  # noqa: E402


def show(function, title):
    print(f"==== {function.name} ({title})")
    for label, block in function.blocks.items():
        print(f"{label}  {[f'{address:06X}' for address in block.addresses]}")
        for statement in block.statements:
            if type(statement).__name__ == "SetFlag":
                continue
            origin = getattr(statement, "origin", None)
            where = f"{origin:06X}" if isinstance(origin, int) else "      "
            print(f"    {where}  {statement}")
        print(f"    END {block.end}")
    print()


def main():
    arguments = sys.argv[1:]
    full = "--full" in arguments
    entries = [int(argument, 16) for argument in arguments if argument != "--full"]
    d.configure()
    program = d.load_program(d.ROOT / "rom")
    if not full:
        builder = d.functions.Builder(program)
        for entry in entries:
            function = builder.build(entry)
            try:
                stack.analyse(function)
            except stack.StackError as problem:
                print(f"==== {entry:06X}: {problem}")
                continue
            show(function, "after the stack pass")
        return
    built, failures = d.build_functions(program)
    d.analyse(program, built)
    for entry in entries:
        show(built[entry], "after the analysis")


if __name__ == "__main__":
    main()
