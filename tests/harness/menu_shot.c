/* menu_shot: pictures of the menus, drawn as the port draws them (with a test settings file and achievements file):
   menu_front.ppm (the main menu), menu_training.ppm, menu_game.ppm (GAME OPTIONS), menu_achievements.ppm,
   menu_graphics.ppm. Keys are sent as the player would press them. */
#include "common.h"
#include "menu.h"
#include "settings.h"
#include "achievements.h"

static void quiet(bool flag) { (void)flag; }
static void nothing(void) {}
static bool no(void) { return false; }

static void key(SDL_Scancode code)
{
	SDL_Event event = { 0 };

	event.type = SDL_KEYDOWN;
	event.key.keysym.scancode = code;
	menu_event(&event);
}

static void shot(const char *name)
{
	static uint32_t game[240 * 320], out[240 * 320];

	menu_draw(game, out);
	save_ppm(name, out, 240, 320);
	printf("%s\n", name);
}

int main(void)
{
	static settings options, file;
	static SDL_GameController *pads[2];
	menu_host host = { 0 };

	settings_defaults(&options);
	settings_defaults(&file);
	start_game();
	achievements_load("menu_shot.achievements");
	host.options = &options;
	host.file = &file;
	host.path = "menu_shot.ini";
	host.pads = pads;
	host.reset = quiet;
	host.apply_window = host.apply_video = host.apply_timing = host.quit = host.main_menu = nothing;
	host.state = quiet;
	host.in_training = host.training = no;
	menu_init(&host);

	menu_open_front();
	shot("menu_front.ppm");
	key(SDL_SCANCODE_DOWN);    /* TRAINING */
	key(SDL_SCANCODE_RETURN);
	shot("menu_training.ppm");
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_DOWN);    /* ACHIEVEMENTS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_achievements.ppm");
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_DOWN);    /* SETTINGS */
	key(SDL_SCANCODE_RETURN);
	for (int i = 0; i < 3; i++)
		key(SDL_SCANCODE_DOWN);    /* GAME OPTIONS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_game.ppm");
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_DOWN);    /* GRAPHICS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_graphics.ppm");
	return 0;
}
