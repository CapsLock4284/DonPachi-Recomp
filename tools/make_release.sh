#!/bin/sh
# Name:        make_release.sh
# Description: the player's package of the game for Linux: release/DonPachi-<version>-linux-x64.tar.gz, for a
#              GitHub Release (tools/make_release.ps1 makes the Windows one)
# Version:     0.1.0
# Created:     11-11-2026
#
# license:BSD-3-Clause
#
# Run it from the project's folder after a Release build on Linux (or WSL):
#     cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
#     cmake --build build/linux -j
#     sh tools/make_release.sh
#
# The version is CMakeLists.txt's, the one the game shows. It copies only the files named below (an allow list),
# then checks the folder: no ROM set, no extracted game data, no saves or settings, and a 64-bit Linux program that
# needs nothing but the system's own libraries and SDL2. Any problem stops it before the .tar.gz is made.
# SDL2 isn't carried: a distribution's libSDL2 needs many of its other libraries; the player's system has its own
# (README.txt says how to install it where it hasn't).
set -e
cd "$(dirname "$0")/.."

fail() { echo "No package made: $*" >&2; exit 1; }

version=$(sed -n 's/^project(donpachi[[:space:]]\{1,\}VERSION[[:space:]]\{1,\}\([0-9]*\.[0-9]*\.[0-9]*\).*/\1/p' CMakeLists.txt)
[ -n "$version" ] || fail "no 'project(donpachi VERSION x.y.z' in CMakeLists.txt"
if [ -n "$1" ] && [ "$1" != "$version" ]; then
	fail "version $1 given, but CMakeLists.txt says $version (the game would show $version)"
fi
grep -q "^## v$version\b" CHANGELOG.md || fail "CHANGELOG.md has no '## v$version' section: rename its '## Unreleased' to '## v$version (date)' first"

name="DonPachi-$version-linux-x64"
out="release/$name"
tarball="release/$name.tar.gz"
program=build/bin-linux/donpachi

sdl_license=$(ls build/downloads/sdl2/SDL2-*/LICENSE.txt 2>/dev/null | head -n 1)
[ -n "$sdl_license" ] || sdl_license=/usr/share/doc/libsdl2-2.0-0/copyright
for from in "$program" ports/sdl2/release/README.txt CHANGELOG.md ports/sdl2/release/NOTICES.txt \
		runtime/LICENSE.tmnt-runtime "$sdl_license"; do
	[ -f "$from" ] || fail "missing: $from (build the Release first?)"
done

# the program newer than its sources: a build made after the last change
newest=$(find ports/sdl2 runtime -name '*.[ch]' -newer "$program" | head -n 1)
[ -z "$newest" ] || fail "$program is older than $newest: build the Release again first"

# ---- the folder, made fresh ----
rm -rf "$out" "$tarball"
mkdir -p "$out/licenses"
cp "$program" "$out/donpachi"
strip "$out/donpachi" 2>/dev/null || true
for text in README.txt:ports/sdl2/release/README.txt CHANGES.txt:CHANGELOG.md licenses/NOTICES.txt:ports/sdl2/release/NOTICES.txt; do
	# (Unix line ends, the version in the README)
	tr -d '\r' < "${text#*:}" | sed "s/{VERSION}/$version/g" > "$out/${text%%:*}"
done
cp runtime/LICENSE.tmnt-runtime "$out/licenses/tmntarcade-runtime.txt"
cp "$sdl_license" "$out/licenses/SDL2.txt"
chmod 755 "$out/donpachi"
chmod 644 "$out"/*.txt "$out"/licenses/*

# ---- the checks ----
problems=""
allowed="donpachi README.txt CHANGES.txt licenses/NOTICES.txt licenses/tmntarcade-runtime.txt licenses/SDL2.txt"
for file in $(cd "$out" && find . -type f | sed 's|^\./||'); do
	case " $allowed " in *" $file "*) ;; *) problems="$problems
  not on the list: $file" ;; esac
	case "$file" in *.zip|*.7z|*.dat|*.part|*.eeprom|*.hiscore|*.achievements|*.stats|*.dpreplay|*.ini|*.bin|*.rom|*.xml|*.u[0-9]*)
		problems="$problems
  a ROM set, game data, save or settings file: $file" ;; esac
done
# the program: a 64-bit Linux one, small (the game's data stays in the player's ROM set), needing only the
# system's libraries and SDL2
file_kind=$(od -An -tx1 -N5 "$out/donpachi" | tr -d ' \n')
[ "$file_kind" = "7f454c4602" ] || problems="$problems
  donpachi is not a 64-bit Linux program"
size=$(wc -c < "$out/donpachi")
[ "$size" -le 8388608 ] || problems="$problems
  donpachi is $((size / 1048576)) MB: more than the program should be"
for needed in $(readelf -d "$out/donpachi" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
	case "$needed" in libSDL2-2.0.so.0|libm.so.6|libc.so.6|libpthread.so.0|libdl.so.2) ;;
		*) problems="$problems
  donpachi needs $needed, which players may not have" ;; esac
done
if [ -n "$problems" ]; then
	rm -rf "$out"
	echo "No package made:$problems" >&2
	exit 1
fi

tar -C release -czf "$tarball" --owner=0 --group=0 "$name"
glibc=$(objdump -T "$out/donpachi" 2>/dev/null | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -n 1)
echo "Made $tarball"
echo "  (it needs ${glibc:-glibc} or newer: a Linux at least as recent as the one it was built on)"
(cd "$out" && find . -type f | sed 's|^\./||' | sort | while read -r file; do
	printf '  %-36s %10s bytes\n' "$file" "$(wc -c < "$file")"
done)
