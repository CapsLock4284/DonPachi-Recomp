/* menu_shot: pictures of the menus, drawn as the port draws them (with a test settings file and achievements file):
   menu_front.ppm (the main menu), menu_training.ppm, menu_game.ppm (GAME OPTIONS), menu_achievements.ppm,
   menu_graphics.ppm, menu_stats.ppm and menu_stats_2.ppm (STATS=file: its statistics), menu_training_game.ppm (the options in a training game). Keys are sent as the player would press them. */
#include "common.h"
#include "menu.h"
#include "settings.h"
#include "achievements.h"
#include "training.h"
#include "stats.h"

static void quiet(bool flag) { (void)flag; }
static void nothing(void) {}
static bool no(void) { return false; }
static bool yes(void) { return true; }

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
	options.training_area = file.training_area = 1;    /* (the training page's longest values) */
	options.training_point = file.training_point = TRAINING_MIDBOSS;
	options.training_power = file.training_power = TRAINING_POWER_ON_PACE;
	start_game();
	achievements_load("menu_shot.achievements");
	stats_load(getenv("STATS") ? getenv("STATS") : "menu_shot.stats");
	host.options = &options;
	host.file = &file;
	host.path = "menu_shot.ini";
	host.pads = pads;
	host.reset = quiet;
	host.apply_window = host.apply_video = host.apply_timing = host.quit = host.main_menu = nothing;
	host.state = quiet;
	host.in_training = host.training = no;
	host.retry_training = nothing;
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
	key(SDL_SCANCODE_DOWN);    /* STATISTICS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_stats.ppm");
	for (int i = 0; i < 40; i++)
		key(SDL_SCANCODE_DOWN);    /* (scrolled to the areas) */
	shot("menu_stats_2.ppm");
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
	host.in_training = yes;    /* (the options in a training game) */
	menu_init(&host);
	menu_open();
	shot("menu_training_game.ppm");
	return 0;
}
