"""
Name:        latency.py
Description: measures how many frames the game takes to react to the controls: for each control, a run that presses
             it at a frame of play and a run that does not, the same otherwise; the first picture (and the first
             memory) where the two differ
Version:     0.1.0 (draft)
Created:     09-11-2026

Usage:
    python tools/latency.py [--headless build/bin/donpachi_headless] [--rom rom] [--cheats cheats/donpachi.xml]
                            [--at FRAME ...] [--arcade-timing]

The runs are the headless port's (ports/headless), so the result is the decompiled program's, which runs as the
original does (tools/cref). A control pressed "before picture N" is what the port reads before it runs the frame
whose picture is N: the game reads it in that frame's vblank interrupt. "Frames" counts the pictures from there:
1 is picture N itself, the earliest a reaction can show (it is drawn right after the control is read), 2 the next.

    picture        the first picture that differs (what is on the screen)
    memory         the first frame whose memory differs (the game has read the control and acted on it)

--arcade-timing measures the pictures as the board shows them (the ports' --arcade-timing): a frame later.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

try:
    from PIL import Image, ImageChops
except ImportError:
    sys.exit("latency.py needs Pillow: python -m pip install pillow")

CONTROLS = (("up", 0x01), ("down", 0x02), ("left", 0x04), ("right", 0x08), ("shot", 0x10), ("bomb", 0x20))
START = ["--input", "600:605:1:100", "--input", "700:705:1:80", "--input", "900:905:1:10"]    # coin, start, ship
AFTER = 10    # pictures looked at after the press


def run(arguments, extra, frames, folder):
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(folder)
    command = [arguments.headless, "--rom", arguments.rom, "--frames", str(max(frames) + 1),
               "--cheats", arguments.cheats, "--cheat", "P1 Invincibility", "--cheat", "P1 Infinite Lives"]
    if arguments.arcade_timing:
        command.append("--arcade-timing")
    command += START + extra + ["--shot-dir", folder, "--shot"] + [str(f) for f in frames]
    command += ["--dump"] + [str(f) for f in frames]
    subprocess.run(command, stdout=subprocess.DEVNULL, check=True)
    pictures = {f: Image.open(os.path.join(folder, f"frame_{f:05d}.bmp")).convert("RGB") for f in frames}
    memories = {f: open(os.path.join(folder, f"dump_{f:05d}.bin"), "rb").read() for f in frames}
    return pictures, memories


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    exe = ".exe" if os.name == "nt" else ""
    parser.add_argument("--headless", default=os.path.join("build", "bin", "donpachi_headless" + exe))
    parser.add_argument("--rom", default="rom")
    parser.add_argument("--cheats", default=os.path.join("cheats", "donpachi.xml"))
    parser.add_argument("--at", type=int, nargs="+", default=[1800, 2400, 3000], help="frames of play to press at")
    parser.add_argument("--arcade-timing", action="store_true", help="the pictures as the board shows them")
    arguments = parser.parse_args()

    work = tempfile.mkdtemp(prefix="latency_")
    try:
        for at in arguments.at:
            frames = list(range(at, at + AFTER + 1))
            plain = run(arguments, [], frames, os.path.join(work, "plain"))
            print(f"pressed before picture {at + 1}:")
            for name, bit in CONTROLS:
                pressed = run(arguments, ["--input", f"{at}:{at + 30}:1:{bit:x}"], frames, os.path.join(work, name))
                picture = next((f for f in frames if ImageChops.difference(plain[0][f], pressed[0][f]).getbbox()), None)
                memory = next((f for f in frames if plain[1][f] != pressed[1][f]), None)
                shown = f"{picture - at} frame(s)" if picture else f"no change in {AFTER} frames"
                read = f"{memory - at} frame(s)" if memory else "none"
                print(f"  {name:6} picture: {shown:22} memory: {read}")
            print()
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
