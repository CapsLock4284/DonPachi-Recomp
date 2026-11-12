/* achievement_targets CASE: the three area-target achievements (achievements.c), each played both ways from the
   training's area start (shielded, as an arcade game for achievements.c: its ship select faked), checking it is
   earned when it should be and not when it shouldn't:
     planes      area 2: the 3 heavy planes shot down, no bomb             -> HEAVY METAL
     planes_bomb area 2: the same, with a bomb as the first one comes      -> nothing
     boat        area 3: the hover boat shot after its stop (it drifts down, stops a second, then speeds off up) -> PATIENCE
     boat_early  area 3: the hover boat shot as soon as it comes            -> nothing
     building    area 4: the extend building shot                           -> HIDDEN EXTEND
     building_bomb area 4: the extend building bombed                       -> nothing
   A target is left at 1 hit point and shot from under it. Prints PASS or FAIL */
#include "common.h"
#include "training.h"
#include "achievements.h"

#include <string.h>

static int earned(const char *name)
{
	for (int index = 0; index < achievements_count(); index++)
		if (!strcmp(achievements_name(index), name))
			return achievements_date(index) != NULL;
	return -1;
}

int main(int argc, char **argv)
{
	const char *const which = argc > 1 ? argv[1] : "planes";
	const bool bomb = strstr(which, "_bomb") != NULL, early = strstr(which, "_early") != NULL;
	const unsigned type = !strncmp(which, "planes", 6) ? 0x13u : !strncmp(which, "boat", 4) ? 0x14u : 0x70u;
	const char *const achievement = type == 0x13u ? "HEAVY METAL" : type == 0x14u ? "PATIENCE" : "HIDDEN EXTEND";
	const int stop_at = type == 0x13u ? 420 : type == 0x14u ? 230 : 640;
	const training_setup setup = { type == 0x13u ? 1 : type == 0x14u ? 2 : 3, 0, 0, TRAINING_POWER_MAX, false, 0 };
	const bool want = !bomb && !early;
	int on_screen[64] = { 0 };
	bool bombed = false;

	start_game();
	achievements_load("");
	training_prepare(&setup);
	/* (achievements.c counts from a ship select: one faked, then play) */
	*(uint16_t *)cave_work_ram_at(0x1018ECu, 2) = 0xBu;
	achievements_frame(true);
	*(uint16_t *)cave_work_ram_at(0x1018ECu, 2) = 7u;
	achievements_frame(true);
	for (long f = 0; f < 30000 && (int16_t)ram_word(0x101A56u) < stop_at; f++)
	{
		const int sx = (int16_t)ram_word(0x102158u + 8u) >> 6;
		unsigned in = 0;
		int tx = -1;

		*cave_work_ram_byte(0x102196u) = 2;
		for (int slot = 0; slot < 64; slot++)
		{
			const uint32_t object = 0x100102u + 0x40u * (uint32_t)slot;
			const uint32_t main = (uint32_t)ram_word(object + 6u) << 16 | ram_word(object + 8u);
			int x, y;

			if (!(ram_word(object) & 0x8000u) || (ram_word(object + 0xCu) >> 8) != type || main < 0x103814u
					|| main >= 0x105000u || (ram_word(main) & 0xA000u) != 0xA000u)
			{
				on_screen[slot] = 0;
				continue;
			}
			x = (int16_t)ram_word(main + 8u) >> 6, y = (int16_t)ram_word(main + 6u) >> 6;
			on_screen[slot] = x > 8 && x < 232 && y > 16 && y < 240 ? on_screen[slot] + 1 : 0;
			if (!on_screen[slot])
				continue;
			if (bomb && !bombed)
			{
				in |= f % 2 ? CAVE_BUTTON2 : 0u;
				bombed = (ram_word(0x106E14u) & 0x8000u) != 0;
			}
			if (tx < 0 && (type != 0x14u || early || on_screen[slot] > 420) && !(bomb && !bombed))
			{
				*(uint16_t *)cave_work_ram_at(main + 0x18u, 2) = 1;
				tx = x;
			}
		}
		if (tx >= 0)
		{
			in |= (f / 2) % 2 ? CAVE_BUTTON1 : 0u;
			if (sx < tx - 3)
				in |= CAVE_RIGHT;
			if (sx > tx + 3)
				in |= CAVE_LEFT;
		}
		cave_board.in0 = (uint16_t)~in;
		cave_board.in1 = 0xFFFFu;
		program_run_frame();
		cave_sound_drop();
		achievements_frame(true);
	}
	printf("%-14s %s %s (should %s): %s\n", which, achievement, earned(achievement) ? "earned" : "not earned",
		want ? "be" : "not be", earned(achievement) == want ? "PASS" : "FAIL");
	return earned(achievement) == want ? 0 : 1;
}
