/* menu_shot: pictures of the menus, drawn as the port draws them (with a test settings file and achievements file):
   menu_front.ppm (the main menu), menu_training.ppm, menu_training_part_way.ppm (START AT on a point part way), menu_game.ppm (GAME OPTIONS), menu_achievements.ppm,
   menu_debug.ppm and menu_debug_2.ppm (the DEBUG TOOLS page), menu_cheats_ai.ppm and menu_cheats.ppm (the CHEATS page: the port's own, then the cheat file's), menu_graphics.ppm, menu_replays.ppm (the replays in replays/ here), menu_replay.ppm, menu_replay_rename.ppm, menu_replay_delete.ppm (the newest
   chosen), menu_stats.ppm and menu_stats_2.ppm (STATS=file: its statistics), menu_training_game.ppm (the options in a training game), menu_game_options.ppm (in an arcade game: STATE SLOT chosen), menu_controls.ppm (its hotkeys), menu_save_replay.ppm (the question as a game ends). Keys are sent as the player would press them. Each
   picture has the chosen entry's tooltip panel at its right. */
#include "common.h"
#include "own_cheats.h"
#include "cave/cheats.h"
#include "menu.h"
#include "settings.h"
#include "achievements.h"
#include "training.h"
#include "stats.h"
#include "replay.h"
#include "widgets.h"

static void quiet(bool flag) { (void)flag; }
static void nothing(void) {}
static bool no(void) { return false; }
static bool yes(void) { return true; }
static bool no_replay(const char *file) { (void)file; return false; }
static int slot_now = 1;
static int slot_step(int step) { slot_now = (slot_now + step + STATE_SLOTS) % STATE_SLOTS; return slot_now; }
static bool slot_used(int slot) { return slot == 0; }

static void key(SDL_Scancode code)
{
	SDL_Event event = { 0 };

	event.type = SDL_KEYDOWN;
	event.key.keysym.scancode = code;
	menu_event(&event);
}

/* the menu's picture, with its tooltip's panel at the right (as the port shows it on a wide screen) */
static void shot(const char *name)
{
	static uint32_t game[240 * 320], out[240 * 320], help[WIDGET_W * WIDGET_H], both[(240 + WIDGET_W) * 320];
	const char *title, *text;

	menu_draw(game, out);
	memset(help, 0, sizeof(help));
	if (menu_help(&title, &text))
		widgets_draw_help(help, title, text);
	for (int y = 0; y < 320; y++)
	{
		memcpy(both + y * (240 + WIDGET_W), out + y * 240, 240 * sizeof(uint32_t));
		memcpy(both + y * (240 + WIDGET_W) + 240, help + y * WIDGET_W, WIDGET_W * sizeof(uint32_t));
	}
	save_ppm(name, both, 240 + WIDGET_W, 320);
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
	replay_folder("replays/");
	host.options = &options;
	host.file = &file;
	host.path = "menu_shot.ini";
	host.pads = pads;
	host.reset = quiet;
	host.apply_window = host.apply_video = host.apply_timing = host.quit = host.main_menu = nothing;
	host.state = quiet;
	host.in_training = host.training = no;
	host.state_slot = slot_step;
	host.state_used = slot_used;
	host.in_replay = no;
	host.play_replay = no_replay;
	host.keep_replay = quiet;
	host.replay_in_game = no;
	host.hold_replay = nothing;
	host.retry_training = nothing;
	own_cheats_add();
	cheats_load("cheats/donpachi.xml");
	menu_init(&host);

	menu_open_front();
	shot("menu_front.ppm");
	key(SDL_SCANCODE_DOWN);    /* TRAINING */
	key(SDL_SCANCODE_RETURN);
	shot("menu_training.ppm");
	key(SDL_SCANCODE_DOWN);    /* START AT, moved back from MID-BOSS to 95% OF THE WAY */
	key(SDL_SCANCODE_LEFT);
	shot("menu_training_part_way.ppm");
	key(SDL_SCANCODE_RIGHT);
	key(SDL_SCANCODE_UP);
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_DOWN);    /* REPLAYS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_replays.ppm");
	key(SDL_SCANCODE_RETURN);    /* (the newest) */
	shot("menu_replay.ppm");
	key(SDL_SCANCODE_DOWN);    /* RENAME */
	key(SDL_SCANCODE_RETURN);
	key(SDL_SCANCODE_UP);
	shot("menu_replay_rename.ppm");
	key(SDL_SCANCODE_ESCAPE);
	key(SDL_SCANCODE_DOWN);    /* DELETE */
	key(SDL_SCANCODE_RETURN);
	shot("menu_replay_delete.ppm");
	key(SDL_SCANCODE_BACKSPACE);    /* (no) */
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
	for (int i = 0; i < 7; i++)
		key(SDL_SCANCODE_DOWN);    /* CHEATS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_cheats_ai.ppm");    /* (the first: AI PLAYER 2) */
	key(SDL_SCANCODE_DOWN);    /* (the second) */
	shot("menu_cheats.ppm");
	if (getenv("CHEATS_DOWN"))
	{
		for (int i = 0; i < atoi(getenv("CHEATS_DOWN")); i++)
			key(SDL_SCANCODE_DOWN);
		shot("menu_cheats_down.ppm");
		for (int i = 0; i < atoi(getenv("CHEATS_DOWN")); i++)
			key(SDL_SCANCODE_UP);
	}
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_DOWN);    /* DEBUG TOOLS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_debug.ppm");
	key(SDL_SCANCODE_DOWN);
	shot("menu_debug_2.ppm");
	key(SDL_SCANCODE_BACKSPACE);
	for (int i = 0; i < 8; i++)
		key(SDL_SCANCODE_UP);
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_DOWN);    /* GRAPHICS */
	key(SDL_SCANCODE_RETURN);
	shot("menu_graphics.ppm");
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_DOWN);    /* CONTROLS */
	key(SDL_SCANCODE_RETURN);
	for (int i = 0; i < 40; i++)
		key(SDL_SCANCODE_DOWN);    /* (to its end: the hotkeys) */
	key(SDL_SCANCODE_UP);
	key(SDL_SCANCODE_UP);
	key(SDL_SCANCODE_UP);
	shot("menu_controls.ppm");
	key(SDL_SCANCODE_BACKSPACE);
	key(SDL_SCANCODE_UP);
	host.in_training = yes;    /* (the options in a training game) */
	menu_init(&host);
	menu_open();
	shot("menu_training_game.ppm");
	host.in_training = no;    /* (the options in an arcade game: the save states' slot chosen) */
	key(SDL_SCANCODE_ESCAPE);
	menu_init(&host);
	menu_open();
	for (int i = 0; i < 6; i++)
		key(SDL_SCANCODE_DOWN);    /* STATE SLOT */
	shot("menu_game_options.ppm");
	key(SDL_SCANCODE_ESCAPE);
	menu_ask_save_replay(false);
	shot("menu_save_replay.ppm");
	return 0;
}
