/* drop_window_test: the first run's "drop the ROM set here" window (rom_drop.c), with drops made for it: a file that
   is not a .zip after 2.5 s, then the ROM set after 5 s. Run it under a desktop, or a virtual one
   (xvfb-run -a tests/harness/bin/drop_window_test), and look at (or capture) the window. */
#include "rom_drop.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static Uint32 drop(Uint32 interval, void *path)
{
	SDL_Event event;

	(void)interval;
	memset(&event, 0, sizeof(event));
	event.type = SDL_DROPFILE;
	event.drop.file = SDL_strdup((const char *)path);
	SDL_PushEvent(&event);
	return 0;
}

int main(int argc, char **argv)
{
	bool saved;
	bool read;

	(void)argc;
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	SDL_AddTimer(2500, drop, argv[0]);
	SDL_AddTimer(5000, drop, getenv("DONPACHI_ROM") ? getenv("DONPACHI_ROM") : "rom/donpachi.zip");
	read = rom_drop_screen("drop_window_test.dat", "NO DONPACHI_USA.DAT, AND NO DONPACHI.ZIP IN THE 'ROM' FOLDER",
		&saved);
	printf("read %d, saved %d\n", read, saved);
	remove("drop_window_test.dat");
	SDL_Quit();
	return 0;
}
