"""slowdown_compare.py MAME_WAITS PORT_WAITS: the port's slowdown against MAME's 68000 at the board's timing.
Each file has a line per frame of the game, as it comes to its wait for the next one, its work done (the wait
loop's first round in a wait the program has made ready): "game_frame vblank hash", the VBLANKs so far and
cave_state_hash() there (tools/cref/cref_timed --waits; tests/harness/slowdown_check's slowdown_waits.txt). The same game frame at the same VBLANK with the same
board: the slowdown and everything else the same. Prints how many frames agree, and the first that does not."""
import sys


def read(path):
    rows = {}
    with open(path) as file:
        for line in file:
            frame, vblank, state = line.split()
            rows[int(frame)] = (int(vblank), state)
    return rows


mame, port = read(sys.argv[1]), read(sys.argv[2])
common = sorted(set(mame) & set(port))
same_time = sum(1 for f in common if mame[f][0] == port[f][0])
same = sum(1 for f in common if mame[f] == port[f])
print(f"game frames compared: {len(common)}; at the same VBLANK: {same_time}; the same in every way: {same}")
first = next((f for f in common if mame[f] != port[f]), None)
if first is None:
    print("the port and MAME's 68000 agree at every frame: the same frames slowed down, the same board")
else:
    print(f"first difference at game frame {first}: MAME VBLANK {mame[first][0]} {mame[first][1]}, "
          f"the port's VBLANK {port[first][0]} {port[first][1]}")
lost_mame = max(v for v, _ in mame.values()) - len(mame)
lost_port = max(v for v, _ in port.values()) - len(port)
print(f"VBLANKs without a frame of the game (start-up and slowdown): MAME {lost_mame}, the port {lost_port}")
