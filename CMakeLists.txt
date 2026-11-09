# Name:        CMakeLists.txt
# Description: the build of DonPachi's port: the board's runtime, the decompiled program (src/), the headless
#              checks and, with SDL2, the game in a window
# Version:     0.1.0 (draft)
# Created:     09-11-2026
#
# Modelled on tmntarcade-decomp's CMakeLists.txt (BSD-3-Clause, Sebastian Toyos).
#
#   cmake -S . -B build/msvc
#   cmake --build build/msvc --config Release
#
# The executables go to build/bin: donpachi_headless (no window), and donpachi (a window, with SDL2: on Windows
# its development package, SDL2-devel-2.x.x-VC.zip, unpacked in build/downloads/sdl2). Both read the player's own
# ROM set, MAME's donpachi.zip (--rom PATH, default rom/donpachi.zip... or rom/).
cmake_minimum_required(VERSION 3.20)
project(donpachi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(DONPACHI_BIN_DIR ${CMAKE_SOURCE_DIR}/build/bin CACHE PATH "Where the executables go")
foreach(config IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${config} ${DONPACHI_BIN_DIR})
endforeach()

if(MSVC)
	add_compile_options(/W3 /utf-8)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# The board: its memory map, the EEPROM, the picture, the ROM set's reader
add_library(donpachi_runtime STATIC
	runtime/src/machine.c
	runtime/src/video.c
	runtime/src/rom_set.c
	runtime/src/inflate.c
	runtime/src/cheats.c
)
target_include_directories(donpachi_runtime PUBLIC runtime/include PRIVATE runtime/src)

# The game: the decompiled 68000 program (tools/decompile.py writes src/) on its runtime (game_program.c: the
# frames, the interrupt, the program's data and tables from the ROM set; tasks.c: the program's own stack)
set(DONPACHI_DECOMPILED_DIR ${CMAKE_SOURCE_DIR}/src CACHE PATH "The game's C (tools/decompile.py)")
file(GLOB DONPACHI_DECOMPILED CONFIGURE_DEPENDS ${DONPACHI_DECOMPILED_DIR}/*.c ${DONPACHI_DECOMPILED_DIR}/*/*.c)
if(NOT DONPACHI_DECOMPILED)
	message(FATAL_ERROR "No decompiled program in ${DONPACHI_DECOMPILED_DIR}: run python tools/decompile.py first")
endif()
add_library(donpachi_game STATIC runtime/src/game_program.c runtime/src/tasks.c ${DONPACHI_DECOMPILED})
target_include_directories(donpachi_game PUBLIC runtime/include ${DONPACHI_DECOMPILED_DIR})
target_link_libraries(donpachi_game PUBLIC donpachi_runtime)
if(MSVC)
	# (one source file of 2.4 MB; the decompiler's casts between widths)
	target_compile_options(donpachi_game PRIVATE /bigobj /wd4244 /wd4146 /wd4018 /wd4554)
else()
	target_compile_options(donpachi_game PRIVATE -fno-strict-aliasing)
endif()

add_executable(donpachi_headless ports/headless/main.c)
target_link_libraries(donpachi_headless PRIVATE donpachi_game)

# The game in a window (ports/sdl2): SDL2 from the system, or on Windows the development package unpacked in
# build/downloads/sdl2, its library copied beside the executable
set(DONPACHI_SDL2_UNPACKED "")
if(WIN32)
	file(GLOB DONPACHI_SDL2_UNPACKED ${CMAKE_SOURCE_DIR}/build/downloads/sdl2/SDL2-*/cmake)
endif()
find_package(SDL2 CONFIG QUIET HINTS ${DONPACHI_SDL2_UNPACKED})
if(SDL2_FOUND)
	set(DONPACHI_SDL2 SDL2::SDL2)
	if(TARGET SDL2::SDL2main)
		list(APPEND DONPACHI_SDL2 SDL2::SDL2main)
	endif()
	add_executable(donpachi WIN32 ports/sdl2/main.c)
	target_link_libraries(donpachi PRIVATE donpachi_game ${DONPACHI_SDL2})
	if(WIN32)
		add_custom_command(TARGET donpachi POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:donpachi>)
	endif()
else()
	message(STATUS "No SDL2: the window port (donpachi) is not built")
endif()
