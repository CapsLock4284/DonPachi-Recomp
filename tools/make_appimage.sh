#!/bin/sh
# Name:        make_appimage.sh
# Description: the player's Linux AppImage: release/DonPachi-<version>-x86_64.AppImage, one file that runs on most
#              Linux systems with SDL2 carried inside (tools/make_release.sh makes the .tar.gz, which uses the
#              system's SDL2)
# Version:     0.1.0
# Created:     13-11-2026
#
# license:BSD-3-Clause
#
# Run it from the project's folder on Linux (or WSL), with the decompiled program in src/:
#     sh tools/make_appimage.sh
#     sh tools/make_appimage.sh --test     (to try it between releases: CHANGELOG.md not checked, the file named
#                                           DonPachi-<version>-test-x86_64.AppImage)
#
# It needs a C compiler, CMake, curl and the development files SDL2's video and sound backends are built with
# (Ubuntu / Debian; the check below names what is missing):
#     sudo apt install build-essential cmake curl libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
#         libxfixes-dev libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev \
#         libgl-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev
#
# What it does, each step only when it isn't done already. The work goes in a folder on Linux's own filesystem,
# ~/.cache/donpachi-appimage (or $DONPACHI_APPIMAGE_WORK): SDL2's source and its install have symbolic links, which
# a Windows drive in WSL (/mnt/c, /mnt/j) can't hold. The downloads are kept in build/downloads:
#   1. SDL2 built from its source (the same version as the Windows package), with its backends loaded as the
#      game starts (SDL's default: X11 or Wayland, PulseAudio, PipeWire or ALSA, whatever the player's system has),
#      so the library itself needs nothing but the C library: it can go inside the AppImage
#   2. the game built against it (the program finds the library beside it: its RUNPATH $ORIGIN)
#   3. the AppImage's folder (AppDir): the program and libSDL2 in usr/bin, AppRun, the .desktop
#      file and the icon, the README, changes and licences in usr/share/doc/donpachi
#   4. checked as make_release.sh checks its package (an allow list, no ROM set or player's files, what the
#      programs need), then made into one file by appimagetool (downloaded once: AppImage's own tool)
# The game keeps its files beside the .AppImage file (ports/sdl2/paths.c), as the .tar.gz's beside the program.
# The version is CMakeLists.txt's, the one the game shows; CHANGELOG.md must have its section, as for the .tar.gz.
set -e
cd "$(dirname "$0")/.."

fail() { echo "No AppImage made: $*" >&2; exit 1; }

SDL_VERSION=2.32.10
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.tar.gz"
TOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

test=""
if [ "$1" = "--test" ]; then
	test="-test"
	shift
fi
version=$(sed -n 's/^project(donpachi[[:space:]]\{1,\}VERSION[[:space:]]\{1,\}\([0-9]*\.[0-9]*\.[0-9]*\).*/\1/p' CMakeLists.txt)
[ -n "$version" ] || fail "no 'project(donpachi VERSION x.y.z' in CMakeLists.txt"
if [ -n "$1" ] && [ "$1" != "$version" ]; then
	fail "version $1 given, but CMakeLists.txt says $version (the game would show $version)"
fi
[ -n "$test" ] || grep -q "^## v$version\b" CHANGELOG.md || fail "CHANGELOG.md has no '## v$version' section: rename its '## Unreleased' to '## v$version (date)' first"
unreleased=$(tr -d '\r' < CHANGELOG.md | sed -n '/^## Unreleased/,/^## v/{/^## /d;/^[[:space:]]*$/d;/^Nothing yet\.$/d;p}')
[ -n "$test" ] || [ -z "$unreleased" ] || fail "CHANGELOG.md lists changes under '## Unreleased' that v$version doesn't have: make them a new version first (or move them into v$version's section)"
[ "$(uname -m)" = "x86_64" ] || fail "this makes the x86_64 AppImage: run it on a 64-bit PC's Linux (or WSL)"
ls src/*.c >/dev/null 2>&1 || fail "no decompiled program in src/: run python3 tools/decompile.py first"
for tool in cc cmake curl readelf objdump; do
	command -v "$tool" >/dev/null 2>&1 || fail "$tool is not installed (see the top of this script)"
done

work=${DONPACHI_APPIMAGE_WORK:-${XDG_CACHE_HOME:-$HOME/.cache}/donpachi-appimage}
downloads=$(pwd)/build/downloads
sdl=$work/sdl2                      # (SDL2 installed: lib/libSDL2-2.0.so.0, lib/cmake/SDL2)
appdir=$work/AppDir
name="DonPachi-$version$test-x86_64.AppImage"
image="release/$name"
mkdir -p "$work" "$downloads" release

# ---- 1. SDL2 from its source ----
if [ ! -f "$sdl/lib/libSDL2-2.0.so.0" ]; then
	source_tar="$downloads/SDL2-$SDL_VERSION.tar.gz"
	if [ ! -f "$source_tar" ]; then
		echo "Downloading SDL2 $SDL_VERSION's source"
		curl -fL --retry 3 -o "$source_tar.part" "$SDL_URL" || fail "couldn't download $SDL_URL"
		mv "$source_tar.part" "$source_tar"
	fi
	rm -rf "$work/sdl2-source" "$work/sdl2-build"
	mkdir -p "$work/sdl2-source"
	tar -xzf "$source_tar" -C "$work/sdl2-source" --strip-components=1
	echo "Building SDL2 $SDL_VERSION"
	cmake -S "$work/sdl2-source" -B "$work/sdl2-build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$sdl" \
		-DCMAKE_INSTALL_LIBDIR=lib -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF \
		-DSDL_RPATH=OFF > "$work/sdl2-configure.log" 2>&1 || fail "SDL2's configure failed: see $work/sdl2-configure.log"
	cmake --build "$work/sdl2-build" -j"$(nproc)" > "$work/sdl2-build.log" 2>&1 || fail "SDL2's build failed: see $work/sdl2-build.log"
	cmake --install "$work/sdl2-build" > /dev/null || fail "SDL2's install failed"
	cp "$work/sdl2-source/LICENSE.txt" "$sdl/LICENSE.txt"
fi
[ -f "$sdl/LICENSE.txt" ] || fail "no $sdl/LICENSE.txt (SDL2's licence): delete $sdl to build it again"
sdl_library=$(readlink -f "$sdl/lib/libSDL2-2.0.so.0")
# its backends: loaded by name as the game starts, so the library names them (built without their development
# files, a backend is left out and players with only that system would have no window or no sound)
missing=""
for backend in "X11 (libx11-dev and the libx*-dev):libX11.so" "Wayland (libwayland-dev, wayland-protocols, libxkbcommon-dev):libwayland-client.so" \
		"PulseAudio (libpulse-dev):libpulse" "ALSA (libasound2-dev):libasound.so.2"; do
	grep -q "${backend##*:}" "$sdl_library" || missing="$missing
  ${backend%%:*}"
done
if [ -n "$missing" ]; then
	rm -rf "$sdl"
	fail "SDL2 was built without these, whose development files aren't installed (see the top of this script):$missing"
fi

# ---- 2. the game, against that SDL2 ----
# (CMAKE_BUILD_WITH_INSTALL_RPATH: the program's RUNPATH only $ORIGIN, not this folder's SDL2)
cmake -S . -B "$work/game" -DCMAKE_BUILD_TYPE=Release -DSDL2_DIR="$sdl/lib/cmake/SDL2" -DDONPACHI_BIN_DIR="$work/bin" \
	-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
	> "$work/game-configure.log" 2>&1 || fail "the game's configure failed: see $work/game-configure.log"
cmake --build "$work/game" --target donpachi -j"$(nproc)" > "$work/game-build.log" 2>&1 \
	|| fail "the game's build failed: see $work/game-build.log"
program=$work/bin/donpachi
[ -f "$program" ] || fail "no $program after the build (SDL2 not found by CMake? see $work/game-configure.log)"

# ---- 3. the AppImage's folder, made fresh ----
rm -rf "$appdir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/doc/donpachi/licenses"
cp "$program" "$appdir/usr/bin/donpachi"
cp "$sdl_library" "$appdir/usr/bin/libSDL2-2.0.so.0"
strip "$appdir/usr/bin/donpachi" "$appdir/usr/bin/libSDL2-2.0.so.0" 2>/dev/null || true
cp ports/sdl2/icon/donpachi.png "$appdir/donpachi.png"
cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
# the AppImage's start: the game in usr/bin (it keeps its files beside the .AppImage file: $APPIMAGE)
here=$(dirname "$(readlink -f "$0")")
exec "$here/usr/bin/donpachi" "$@"
EOF
cat > "$appdir/donpachi.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=DonPachi
Comment=DonPachi (arcade, Cave 1995): the decompiled game, with your own ROM set
Exec=donpachi
Icon=donpachi
Categories=Game;ArcadeGame;
Terminal=false
X-AppImage-Version=$version
EOF
for text in README.txt:ports/sdl2/release/README.txt CHANGES.txt:CHANGELOG.md licenses/NOTICES.txt:ports/sdl2/release/NOTICES.txt; do
	tr -d '\r' < "${text#*:}" | sed "s/{VERSION}/$version/g" > "$appdir/usr/share/doc/donpachi/${text%%:*}"
done
cp runtime/LICENSE.tmnt-runtime "$appdir/usr/share/doc/donpachi/licenses/tmntarcade-runtime.txt"
cp "$sdl/LICENSE.txt" "$appdir/usr/share/doc/donpachi/licenses/SDL2.txt"
chmod 755 "$appdir/AppRun" "$appdir/usr/bin/donpachi" "$appdir/usr/bin/libSDL2-2.0.so.0"
chmod 644 "$appdir/donpachi.png" "$appdir/donpachi.desktop" "$appdir"/usr/share/doc/donpachi/*.txt "$appdir"/usr/share/doc/donpachi/licenses/*
ln -s donpachi.png "$appdir/.DirIcon"

# ---- 4. the checks ----
problems=""
allowed="AppRun donpachi.desktop donpachi.png usr/bin/donpachi usr/bin/libSDL2-2.0.so.0 usr/share/doc/donpachi/README.txt
	usr/share/doc/donpachi/CHANGES.txt usr/share/doc/donpachi/licenses/NOTICES.txt
	usr/share/doc/donpachi/licenses/tmntarcade-runtime.txt usr/share/doc/donpachi/licenses/SDL2.txt"
allowed=$(echo $allowed)
for file in $(cd "$appdir" && find . -type f | sed 's|^\./||'); do
	case " $allowed " in *" $file "*) ;; *) problems="$problems
  not on the list: $file" ;; esac
	case "$file" in *.zip|*.7z|*.dat|*.part|*.eeprom|*.hiscore|*.achievements|*.stats|*.dpreplay|*.ini|*.bin|*.rom|*.xml|*.u[0-9]*)
		problems="$problems
  a ROM set, game data, save or settings file: $file" ;; esac
done
size=$(wc -c < "$appdir/usr/bin/donpachi")
[ "$size" -le 8388608 ] || problems="$problems
  donpachi is $((size / 1048576)) MB: more than the program should be"
# what the two need: the program the C library and the SDL2 beside it, SDL2 the C library alone
for needed in $(readelf -d "$appdir/usr/bin/donpachi" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
	case "$needed" in libSDL2-2.0.so.0|libm.so.6|libc.so.6|libpthread.so.0|libdl.so.2) ;;
		*) problems="$problems
  donpachi needs $needed, which players may not have" ;; esac
done
readelf -d "$appdir/usr/bin/donpachi" | grep -q 'R.*PATH.*\$ORIGIN' || problems="$problems
  donpachi doesn't look for libSDL2 beside itself (no \$ORIGIN in its RUNPATH)"
for needed in $(readelf -d "$appdir/usr/bin/libSDL2-2.0.so.0" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
	case "$needed" in libm.so.6|libc.so.6|libpthread.so.0|libdl.so.2|librt.so.1|ld-linux-x86-64.so.2) ;;
		*) problems="$problems
  libSDL2 needs $needed (built with it linked in, not loaded as the game starts)" ;; esac
done
if [ -n "$problems" ]; then
	echo "No AppImage made:$problems" >&2
	exit 1
fi

# ---- the AppImage ----
tool="$downloads/appimagetool-x86_64.AppImage"
if [ ! -x "$tool" ]; then
	echo "Downloading appimagetool"
	curl -fL --retry 3 -o "$tool.part" "$TOOL_URL" || fail "couldn't download $TOOL_URL"
	chmod 755 "$tool.part"
	mv "$tool.part" "$tool"
fi
rm -f "$image"
# (extracted and run: no FUSE needed, as in WSL)
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tool" --no-appstream "$appdir" "$image" > "$work/appimagetool.log" 2>&1 \
	|| fail "appimagetool failed: see $work/appimagetool.log"
chmod 755 "$image"
glibc=$(for binary in "$appdir/usr/bin/donpachi" "$appdir/usr/bin/libSDL2-2.0.so.0"; do
	objdump -T "$binary" 2>/dev/null | grep -o 'GLIBC_[0-9.]*'; done | sort -uV | tail -n 1)
echo "Made $image ($(( $(wc -c < "$image") / 1024 )) KB)"
echo "  It runs on 64-bit Linux with glibc ${glibc#GLIBC_} or newer (2.34 is 2021's: Ubuntu 22.04, Debian 12, Fedora 35 and later)"
# (a glibc newer than 2.35: built on a new system, the AppImage runs only on systems as new; built on an older one,
# Ubuntu 22.04's WSL, it runs on most)
if [ "$(printf '%s\n' "${glibc#GLIBC_}" 2.35 | sort -V | tail -n 1)" != 2.35 ]; then
	echo "  WARNING: glibc ${glibc#GLIBC_} is this system's own: the AppImage won't start on older ones (most players'). What needs it:"
	for binary in "$appdir/usr/bin/donpachi" "$appdir/usr/bin/libSDL2-2.0.so.0"; do
		objdump -T "$binary" 2>/dev/null | awk -v b="${binary##*/}" '/GLIBC_2\.(3[6-9]|[4-9][0-9])/ { print "    " b ": " $NF " " $(NF-1) }'
	done | sort -u | head -n 20
	echo "  Build it on Ubuntu 22.04 instead (in PowerShell: wsl --install -d Ubuntu-22.04; then this script there)"
fi
echo "  Try it: $image   (in WSL, or without FUSE: $image --appimage-extract-and-run)"
