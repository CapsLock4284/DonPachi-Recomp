#!/bin/sh
# Builds the test tools in tests/harness/bin (Linux, gcc; SDL2's development files installed, or SDL2_INCLUDE and
# SDL2_LIB set). Run from anywhere:  sh tests/harness/build.sh
# Needs the decompiled program in src/ (python tools/decompile.py) and the ROM set in rom/donpachi.zip to run.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
HERE="$ROOT/tests/harness"
OUT="$HERE/bin"
OBJ="$OUT/obj"
SDL_INC=${SDL2_INCLUDE:-$(sdl2-config --cflags 2>/dev/null | sed 's/-I//;s/ .*//')}
SDL_LIB=${SDL2_LIB:-$(sdl2-config --libs 2>/dev/null || echo -lSDL2)}
CFLAGS="-std=gnu11 -O1 -w -I$ROOT/runtime/include -I$ROOT/runtime/src -I$ROOT/src -I$ROOT/ports/sdl2 -I$SDL_INC"
mkdir -p "$OBJ"

# the game: the board, the decompiled program, and the port's parts the tools use (not its main.c)
for f in "$ROOT"/runtime/src/*.c "$ROOT"/src/*.c "$ROOT"/src/*/*.c \
         "$ROOT"/ports/sdl2/settings.c "$ROOT"/ports/sdl2/menu.c "$ROOT"/ports/sdl2/widgets.c \
         "$ROOT"/ports/sdl2/training.c "$ROOT"/ports/sdl2/achievements.c "$ROOT"/ports/sdl2/rom_drop.c \
         "$ROOT"/ports/sdl2/hitbox.c "$ROOT"/ports/sdl2/ui_sound.c "$ROOT"/ports/sdl2/stats.c; do
	o="$OBJ/$(echo "$f" | sed "s|$ROOT/||;s|/|_|g;s|\.c$|.o|")"
	[ "$o" -nt "$f" ] || { echo "cc $f"; gcc $CFLAGS -DDONPACHI_VERSION='"test"' -c "$f" -o "$o"; }
done

for tool in ${TOOLS:-autoplay training_check panels_shot menu_shot romdata_check drop_window_test pickups botcmp hitbox_check sfx_log ui_sounds_wav ui_sound_compare midboss_find midboss_fight}; do
	echo "link $tool"
	gcc $CFLAGS "$HERE/$tool.c" "$OBJ"/*.o $SDL_LIB -lm -o "$OUT/$tool"
done
echo "built in $OUT"
