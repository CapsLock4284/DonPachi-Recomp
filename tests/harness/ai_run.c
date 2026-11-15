/* ai_run [FRAMES]: the computer plays the game by itself for the best run it can find, no help of any kind (no
   cheats, no shield, no slowdown off: the arcade game as it is), and saves it as a replay (replays/) to watch in the
   game. Stage 1 of it: area 1, cleared without a miss.

   How: the game is deterministic, so the run is searched, not played. From where the run is, every action (the 9
   ways of the stick, with the shot tapped or the laser held) is tried for SEGMENT frames from a save state; each that
   lives is looked at further (LOOK frames, held on, or a few ways out of trouble), and the best by its score, the
   damage it did (high-health targets' counted more, the laser preferred while one is up) and how safe it is goes
   into the run. If none lives, the run goes back a few segments and tries the next best there
   (a bomb as a last way out: not yet). Then the whole run is played again from the board's reset as a replay is
   recorded, and checked: the same board at the end, and the replay played back as REPLAYS does.

   Prints the run's progress, and PASS or FAIL: area 1 cleared, no miss, the replay the same.
   First run (14-11-2026): area 1 cleared without a miss or a bomb in 13,602 frames of play (3:47), 262,550 points
   (the boss shot down); 6.2 minutes here, 5.6 million frames looked at.
   With type C and the damage done counted, the laser first on high-health targets (the owner's, 15-11-2026): area 1
   in 9,912 frames (2:45), 377,500 points, no miss, no bomb; 4.9 minutes here.
   With the chain valued (its hits at the look's end, a chain let go counted as lost) and the look 54 frames: area 1 in
   9,240 frames (2:34), 375,720 points, best chain 18: no better. How the chain pays (seen in the game's memory): each
   kill within the timer's 28-30 frames adds a hit, and the kill's bonus grows with the hits (390, 780, 1,300 at 2, 3,
   4 hits...). Long chains need the kills timed and spaced across waves, which looking 60 frames ahead from one plan
   at a time does not find: a search keeping several plans (a beam) over a longer stretch is the next step.
   The beam search (15-11-2026): 4 plans, 346,850 points in 7,896 frames (3 minutes here); 16 plans (the default),
   418,800 points, best chain 21, area 1 in 7,872 frames (2:11), no miss, no bomb; 11.5 minutes here.
   Weights tried (CHAIN_W, DAMAGE_W): the chain's hit worth 600 or 1,500 made no difference at all (the same run: the
   plans' chains come with their kills, not from holding kills back); the damage weighed 0.3 cleared the area later
   with fewer points (340,460): the defaults (400, 1.0) stay. Lasering the boss does not keep the chain's timer up
   (only kills do).
   Every kill worth 300 more and the bees (the owner's, 15-11-2026: kill everything, small chains too; always take
   the bees): a bee taken 20,000, a hiding place's damage 50 times (taken as a hit record showing the bee's picture,
   $0C2E-$0C3F, 10 hit points: not sure that is the hiding place). Area 1: 417,800 points, best chain 13, 1 bee of
   13: about the same. The bees need their places (to steer to them, and to shoot where they hide) worked out first.
   Area 1's bees placed from the shmups wiki's map (below: the nearest pulls the ship, the laser under it counts):
   8 of 13 taken, 448,650 points, best chain 16, area 1 in 11,574 frames (3:13), no miss, no bomb; 17 minutes here
   (the owner's BEAM=32 without the bees: 435,950).
   The chain setups (a top player's way, from the owner's screenshots of his notes: enemies weakened first, then
   killed one after another): an enemy weakened and still alive worth 600 x the part of its hit points gone (lost if
   it is killed alone), a kill no longer worth anything by itself: 456,220, best chain 26 (16-21 before), area 1 in
   2:23. With it the chain's hit worth matters at last: 1,500 a hit, 476,510 (the defaults now); a weakened enemy
   worth 1,500, 437,320; 300, 424,580. 17 minutes a run here (16 plans). His area 1: 1,616,990 */
#include "common.h"
#include "replay.h"

#include <math.h>
#include <time.h>

#define SEGMENT     6           /* the frames an action is held */
#define LOOK        54          /* the frames looked further, after it (more than the chain's 30: a kill timed to keep it) */
#define BACK        6           /* the segments gone back when nothing lives */
#define RUN_MOST    (60 * 60 * 6)   /* the frames of play the run may take */

#define MODE        0x1018ECu
#define SHIP        0x102158u   /* player 1's record: +0 bit 15 flying, +6 / +8 its place, +$1C its type (% 3: A B C),
                                   +$22 its bombs */
#define LIVES       0x101A2Fu
#define STAGE       0x101A40u
#define SCORE       0x10177Au   /* player 1's score: BCD, 4 bytes */
#define CHAIN_TIMER 0x101742u   /* player 1's chain: the frames left to go on with it (30 after a kill), */
#define CHAIN_HITS  0x10174Eu   /* its hits, */
#define CHAIN_BEST  0x101754u   /* the best chain (widgets.c reads the same) */
#define CHAIN_VALUE chain_value /* a hit of a chain still going at the look's end: worth this much (the hits to come
                                   with it; a chain let go is worth nothing more) */
static double chain_value = 1500.0; /* (CHAIN_W) */
static double damage_weight = 1.0;  /* the damage done against the score (DAMAGE_W) */

enum { ACTIONS = 18 };

typedef struct
{
	program_state *state;       /* before the segment */
	int action;
	uint32_t tried;             /* the actions tried here and found wanting (bits) */
} step;

static uint16_t *inputs;        /* the run's inputs, frame by frame from the reset */
static long input_count;
static step *steps;
static int step_count;
static long frames_emulated;

static double now_s(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static bool flying(void)
{
	return (ram_word(SHIP) & 0x8000u) != 0;
}

static uint32_t score(void)
{
	uint32_t s = 0;

	for (int i = 0; i < 4; i++)
	{
		const unsigned b = ram_byte(SCORE + (uint32_t)i);

		s = s * 100u + (b >> 4) * 10u + (b & 15u);
	}
	return s;
}

/* an action's inputs in its 'frame'-th frame (0-): the stick's way, and the shot tapped (every other frame) or the
   laser held */
static uint16_t action_input(int action, int frame)
{
	static const unsigned ways[9] = { 0, CAVE_UP, CAVE_DOWN, CAVE_LEFT, CAVE_RIGHT, CAVE_UP | CAVE_LEFT,
		CAVE_UP | CAVE_RIGHT, CAVE_DOWN | CAVE_LEFT, CAVE_DOWN | CAVE_RIGHT };
	const int way = action % 9;
	const bool laser = action >= 9;
	unsigned held = ways[way];

	if (laser || frame % 2 == 0)
		held |= CAVE_BUTTON1;
	return (uint16_t)~held;
}

/* one frame with these inputs (player 2 idle) */
static void frame_with(uint16_t in0)
{
	cave_board.in0 = in0;
	cave_board.in1 = 0xFFFFu;
	program_run_frame();
	cave_sound_drop();
	frames_emulated++;
}

/* the run's inputs: one more frame */
static void keep_input(uint16_t in0)
{
	inputs[input_count++] = in0;
}

/* 'frames' frames of the action from frame 'from' of it: false if the ship is shot down in them (or the play ends) */
static bool hold(int action, int from, int frames, bool keep)
{
	for (int f = 0; f < frames; f++)
	{
		const uint16_t in = action_input(action, from + f);

		frame_with(in);
		if (keep)
			keep_input(in);
		if (!flying() || ram_word(MODE) != 7u)
			return flying();
	}
	return true;
}

/* ---- the damage done: the enemies' hit records, $103814 (100) and $104494 (50), $20 bytes each: +0 bits 15 and 13
   able to be shot, +$18 the hit points (ai_player.c reads them the same way). A target of BIG hit points or more is a
   high-health one (a boss's, a mid-boss's, a big enemy's): the laser's work (the owner: laser fire on high-health
   targets first), its damage counted BIG_WEIGHT times ---- */

#define RECORDS     150
#define BIG         1000
#define BIG_WEIGHT  3

typedef struct
{
	int hp[RECORDS];            /* each record's hit points (-1: not one that can be shot) */
	bool bee[RECORDS];          /* a bee's hiding place (its picture the bee's, $0C2E-$0C3F: shot, it shows the bee) */
} health;

#define KILL_VALUE  kill_value  /* an enemy shot down (its record's hit points gone): the owner's, every kill tried for,
                                   small chains too. 0 by default since the chain setups (a top player's way: enemies
                                   weakened first, then killed one after another; a lone kill early spoils that) */
static double kill_value = 0.0;     /* (KILL_W) */
static double setup_value = 600.0;  /* an enemy weakened and still alive, all its hit points but the last gone: worth
                                       this much while it waits to be killed into a chain (SETUP_W; lost if it is
                                       killed alone, more than won back if killed in a chain) */
#define BEE_SPOT    50.0        /* a bee's hiding place's damage counts this many times (shooting it shows the bee) */
#define BEE_VALUE   20000.0     /* a bee taken (the owner's: always try for the bees) */
#define BEES_1      0x10379Au   /* player 1's bees taken in the area */

static uint32_t record_at(int r)
{
	return r < 100 ? 0x103814u + 0x20u * (uint32_t)r : 0x104494u + 0x20u * (uint32_t)(r - 100);
}

static void read_health(health *h, bool *big_up)
{
	*big_up = false;
	for (int r = 0; r < RECORDS; r++)
	{
		const uint32_t at = record_at(r);
		const int hp = (int16_t)ram_word(at + 0x18u);

		h->hp[r] = (ram_word(at) & 0xA000u) == 0xA000u ? (hp > 0 ? hp : 0) : -1;
		h->bee[r] = ram_word(at + 4u) >= 0x0C2Eu && ram_word(at + 4u) < 0x0C40u;
		if (h->hp[r] >= BIG)
			*big_up = true;
	}
}

/* the damage done since 'before': each record that could be shot then, its hit points since lost (all of them if it
   is gone), the high-health ones' counted BIG_WEIGHT times */
static double damage_since(const health *before)
{
	double done = 0.0;

	for (int r = 0; r < RECORDS; r++)
		if (before->hp[r] > 0)
		{
			const uint32_t at = record_at(r);
			const int hp = (int16_t)ram_word(at + 0x18u), now = (ram_word(at) & 0xA000u) == 0xA000u ? (hp > 0 ? hp : 0) : 0;

			if (now < before->hp[r])
				done += (double)(before->hp[r] - now) * (before->bee[r] ? BEE_SPOT : before->hp[r] >= BIG ? BIG_WEIGHT : 1)
					* damage_weight;
			if (now == 0 && !before->bee[r])
				done += KILL_VALUE;
		}
	return done;
}

/* how good an action is from the state 'from' (loaded first): -1 if the ship is shot down in its segment or in every
   way out of the look after it; otherwise its score gained (in the segment and the best look) and the ways out that
   lived */
static double value(const program_state *from, int action)
{
	static const int ways_out[5] = { -1, 0, 3, 4, 2 };    /* (held on; still; left; right; down: with the shot) */
	const uint32_t before = score();
	health hp_before;
	unsigned chain_before;
	bool big_up;
	program_state *after;
	double best = -1.0;
	int lived = 0;

	program_state_load(from);
	read_health(&hp_before, &big_up);
	chain_before = ram_word(CHAIN_TIMER) ? ram_word(CHAIN_HITS) : 0u;
	if (!hold(action, 0, SEGMENT, false))
		return -1.0;
	if (ram_word(STAGE) != 0u || ram_word(MODE) != 7u)
		return 1e9;    /* (the area cleared in it) */
	after = program_state_save();
	for (int w = 0; w < 5; w++)
	{
		const int out = ways_out[w] < 0 ? action : ways_out[w];

		if (w)
			program_state_load(after);
		if (hold(out, w ? 0 : SEGMENT, LOOK, false))
		{
			const unsigned hits = ram_word(CHAIN_TIMER) ? ram_word(CHAIN_HITS) : 0u;
			/* (a chain going when the action began and let go in the look: the hits lost) */
			const double lost = hits == 0 && chain_before >= 2u ? CHAIN_VALUE * (double)chain_before : 0.0;
			const double gained = (double)(score() - before) + damage_since(&hp_before) + CHAIN_VALUE * (double)hits
				- lost;

			lived++;
			if (gained > best)
				best = gained;
		}
	}
	program_state_free(after);
	if (!lived)
		return -1.0;
	/* the score and the damage done, then how many ways out lived (safety); with a high-health target up, the laser
	   (the owner's choice) */
	return best + 200.0 * (double)lived + (big_up && action >= 9 ? 300.0 : 0.0);
}

/* the chain setups (a top player's way, the owner's screenshots): the enemies weakened and still alive, each worth
   setup_value times how much of its hit points is gone. 'full' (the plan's): each record's hit points when it came,
   brought up to date here ('before': the records as the segment began: one there then is the same enemy) */
static double setup_worth(int16_t *full, const health *before)
{
	double worth = 0.0;

	for (int r = 0; r < RECORDS; r++)
	{
		const uint32_t at = record_at(r);
		const int hp = (int16_t)ram_word(at + 0x18u);
		const bool there = (ram_word(at) & 0xA000u) == 0xA000u && hp > 0;

		if (!there)
		{
			full[r] = 0;
			continue;
		}
		if (full[r] <= 0 || before->hp[r] < 0 || hp > full[r])
			full[r] = (int16_t)(before->hp[r] > hp ? before->hp[r] : hp);    /* (new: as it came) */
		if (hp < full[r] && !before->bee[r] && full[r] < BIG)
			worth += setup_value * (double)(full[r] - hp) / (double)full[r];
	}
	return worth;
}

/* ---- area 1's bees (the owner's: always take the bees; the shmups wiki's map of them, Donpachi_stage1_wbees.png):
   each hidden until the laser goes over it, then taken by flying into it. Their places on that map (made 240 pixels
   wide: the map's x, y down from its top), and where the screen is on it: its top at 3,057 - 6 x (scroll - 279)
   (the scroll, $101A56, matched against the map at a few places, and checked with the bees the runs took). A bee's
   place on the screen then: across = its x, up (as the ship's +6) = top + 320 - its y ---- */

#define SCROLL      0x101A56u
#define AREA1_BEES  13
#define BEE_PULL    3000.0      /* the nearest bee on the screen pulls the ship this much (at no distance) */
#define BEE_LASER   2000.0      /* the laser under a bee not yet taken */

static const int area1_bees[AREA1_BEES][2] = {
	{ 11, 729 }, { 35, 868 }, { 221, 976 }, { 195, 1134 }, { 11, 1223 }, { 227, 1223 }, { 11, 1289 }, { 227, 1289 },
	{ 221, 1439 }, { 11, 2051 }, { 41, 2507 }, { 11, 3421 }, { 226, 3452 }
};

/* the bee nearest the ship on the screen not yet taken ('taken': bits), and how far (pixels); -1 if none */
static int nearest_bee(unsigned taken, int *dx, int *dy)
{
	const int scroll = (int16_t)ram_word(SCROLL), top = 3057 - 6 * (scroll - 279);
	const int sx = (int16_t)ram_word(SHIP + 8u) >> 6, sy = (int16_t)ram_word(SHIP + 6u) >> 6;
	int best = -1, best_d = 1 << 30;

	if (ram_word(STAGE) != 0u || ram_word(0x101A42u) != 0u)
		return -1;
	for (int b = 0; b < AREA1_BEES; b++)
	{
		const int up = top + 320 - area1_bees[b][1], ddx = area1_bees[b][0] - sx, ddy = up - sy;

		if (taken >> b & 1u || up < 0 || up > 320)
			continue;
		if (ddx * ddx + ddy * ddy < best_d)
		{
			best_d = ddx * ddx + ddy * ddy;
			best = b;
			*dx = ddx;
			*dy = ddy;
		}
	}
	return best;
}

/* ---- the beam search (BEAM=width, 16 if not given; BEAM=0 the greedy search above): many plans kept at each step, the best by their run's worth so far, each
   tried with every action for a segment; a plan whose ship is shot down, or whose way on dies at once (SAFE frames held
   on, or else still), is dropped; of plans ending with the ship in the same place (to 8 pixels) and the same chain,
   only the best is kept, so the plans differ. Worth: the score and the damage done so far (high-health targets'
   more), and a chain going (its hits, CHAIN_VALUE each). The run is the first plan to clear the area. A plan's actions
   are a tree (each step's node: its parent and its action); its board is kept only while it is in the beam (or in a
   checkpoint: if the beam dies out, the search goes back CHECKPOINT steps with a wider beam) ---- */

#define SAFE        24
#define CHECKPOINT  25

typedef struct
{
	int parent, action;
} node;

typedef struct
{
	program_state *state;
	int node, parent, action;   /* its node; while being made: the node it came from and its action */
	double done, worth;         /* the score and damage so far; with the chain going */
	uint32_t score;
	unsigned bees_taken;        /* area 1's bees this plan has taken (bits: area1_bees) */
	int16_t full[RECORDS];      /* each enemy record's hit points when it came (0: none there) */
	unsigned hits;
	int x, y;
	bool kept;                  /* its board belongs to a checkpoint */
} plan;

static node *nodes;
static int node_count, node_room;

static int new_node(int parent, int action)
{
	if (node_count == node_room)
	{
		node_room = node_room ? node_room * 2 : 65536;
		nodes = realloc(nodes, sizeof(node) * (size_t)node_room);
	}
	nodes[node_count].parent = parent;
	nodes[node_count].action = action;
	return node_count++;
}

/* the run's inputs from a plan's actions, after the boot's */
static void plan_inputs(int at)
{
	int depth = 0, *actions;

	for (int n = at; n >= 0; n = nodes[n].parent)
		depth++;
	actions = malloc(sizeof(int) * (size_t)(depth + 1));
	for (int n = at, i = depth - 1; n >= 0; n = nodes[n].parent, i--)
		actions[i] = nodes[n].action;
	for (int i = 0; i < depth; i++)
		for (int f = 0; f < SEGMENT; f++)
			keep_input(action_input(actions[i], f));
	free(actions);
}

static int by_worth(const void *a, const void *b)
{
	const double wa = ((const plan *)a)->worth, wb = ((const plan *)b)->worth;

	return wa < wb ? 1 : wa > wb ? -1 : 0;
}

static void drop(plan *p)
{
	if (!p->kept)
		program_state_free(p->state);
	p->state = NULL;
}

static bool beam_search(int width, long most, double began)
{
	enum { RING = 3 };
	const int widest = width * 4;
	plan *beam = calloc((size_t)widest, sizeof(plan)), *next = calloc((size_t)widest * ACTIONS, sizeof(plan));
	plan *ring[RING];
	int ring_count[RING] = { 0 }, beam_count = 1, now_width = width, wide_until = 0, back_count = 0;
	long ring_step[RING] = { 0 }, step = 0;
	const uint32_t start_score = score();

	for (int r = 0; r < RING; r++)
		ring[r] = calloc((size_t)widest, sizeof(plan));
	beam[0].state = program_state_save();
	beam[0].node = -1;
	while (step * SEGMENT < most)
	{
		int next_count = 0;

		if (step % CHECKPOINT == 0)
		{
			/* this beam kept to go back to (the oldest checkpoint's boards let go) */
			plan *const oldest = ring[RING - 1];

			for (int i = 0; i < ring_count[RING - 1]; i++)
			{
				oldest[i].kept = false;
				drop(&oldest[i]);
			}
			memmove(&ring[1], &ring[0], sizeof(plan *) * (RING - 1));
			memmove(&ring_count[1], &ring_count[0], sizeof(int) * (RING - 1));
			memmove(&ring_step[1], &ring_step[0], sizeof(long) * (RING - 1));
			ring[0] = oldest;
			for (int i = 0; i < beam_count; i++)
				beam[i].kept = true;
			memcpy(ring[0], beam, sizeof(plan) * (size_t)beam_count);
			ring_count[0] = beam_count;
			ring_step[0] = step;
		}
		for (int b = 0; b < beam_count; b++)
			for (int a = 0; a < ACTIONS; a++)
			{
				plan *const p = &next[next_count];
				health hp_before;
				bool big_up;

				program_state_load(beam[b].state);
				read_health(&hp_before, &big_up);
				{
					const uint32_t at_start = score();
					const unsigned bees_before = ram_word(BEES_1);

					if (!hold(a, 0, SEGMENT, false))
						continue;
					memset(p, 0, sizeof(*p));
					p->parent = beam[b].node;
					p->action = a;
					p->score = score();
					p->done = beam[b].done + (double)(score() - at_start) + damage_since(&hp_before)
						+ (ram_word(BEES_1) > bees_before ? BEE_VALUE * (double)(ram_word(BEES_1) - bees_before) : 0.0);
					p->bees_taken = beam[b].bees_taken;
					memcpy(p->full, beam[b].full, sizeof(p->full));
					if (ram_word(BEES_1) > bees_before)
					{
						/* (the one taken: the nearest) */
						int dx, dy;
						const int bee = nearest_bee(p->bees_taken, &dx, &dy);

						if (bee >= 0)
							p->bees_taken |= 1u << bee;
					}
				}
				p->state = program_state_save();
				if (ram_word(STAGE) != 0u)
				{
					/* the area cleared: this plan is the run */
					const int n = new_node(beam[b].node, a);

					program_state_load(p->state);
					plan_inputs(n);
					printf("  the area cleared: step %ld, %d plans at most, %d times back, %.0f s\n", step, widest,
						back_count, now_s() - began);
					return true;
				}
				p->hits = ram_word(CHAIN_TIMER) ? ram_word(CHAIN_HITS) : 0u;
				p->x = ((int16_t)ram_word(SHIP + 8u) >> 6) / 8;
				p->y = ((int16_t)ram_word(SHIP + 6u) >> 6) / 8;
				p->worth = p->done + CHAIN_VALUE * (double)p->hits + (big_up && a >= 9 ? 300.0 : 0.0)
					+ setup_worth(p->full, &hp_before);
				{
					/* the nearest bee: towards it, and the laser under it (that shows it) */
					int dx, dy;

					if (nearest_bee(p->bees_taken, &dx, &dy) >= 0)
					{
						const double distance = sqrt((double)(dx * dx + dy * dy));

						p->worth += BEE_PULL * (distance < 300.0 ? 1.0 - distance / 300.0 : 0.0);
						if (a >= 9 && abs(dx) < 12 && dy > 0)
							p->worth += BEE_LASER;
					}
				}
				/* its way on must live: held on, or else still */
				if (!hold(a, SEGMENT, SAFE, false))
				{
					program_state_load(p->state);
					if (!hold(0, 0, SAFE, false))
					{
						drop(p);
						continue;
					}
				}
				next_count++;
			}
		/* the old beam's boards let go (not a checkpoint's) */
		for (int b = 0; b < beam_count; b++)
			drop(&beam[b]);
		if (!next_count)
		{
			/* the beam died out: back to a checkpoint, wider */
			int r = ring_step[0] < step || ring_count[1] == 0 ? 0 : 1;

			if (++back_count > 40 || !ring_count[r])
			{
				printf("given up at step %ld: the beam died out\n", step);
				return false;
			}
			beam_count = ring_count[r];
			for (int i = 0; i < beam_count; i++)
			{
				beam[i] = ring[r][i];    /* (its boards stay the checkpoint's) */
				beam[i].kept = true;
			}
			step = ring_step[r];
			now_width = now_width * 2 > widest ? widest : now_width * 2;
			wide_until = (int)step + 2 * CHECKPOINT;
			printf("  dead end: back to step %ld, %d plans\n", step, now_width);
			/* (the checkpoint is used again: the ring goes on from it) */
			for (int k = 0; k < r; k++)
			{
				for (int i = 0; i < ring_count[k]; i++)
				{
					ring[k][i].kept = false;
					drop(&ring[k][i]);
				}
				ring_count[k] = 0;
			}
			step++;    /* (its checkpoint is not taken again) */
			continue;
		}
		qsort(next, (size_t)next_count, sizeof(plan), by_worth);
		{
			int kept = 0;

			for (int i = 0; i < next_count; i++)
			{
				bool same = false;

				for (int k = 0; k < kept && !same; k++)
					same = beam[k].x == next[i].x && beam[k].y == next[i].y && beam[k].hits == next[i].hits;
				if (same || kept == now_width)
				{
					drop(&next[i]);
					continue;
				}
				beam[kept] = next[i];
				beam[kept].kept = false;
				kept++;
			}
			for (int k = 0; k < kept; k++)
				beam[k].node = new_node(beam[k].parent, beam[k].action);
			beam_count = kept;
		}
		if (now_width > width && step >= wide_until)
			now_width = width;
		step++;
		if (step % 100 == 0)
			printf("  frame %ld: best score %u, chain %u, %d plans, %d times back, %.0f s, %.1f M frames\n",
				step * SEGMENT, beam[0].score, beam[0].hits, beam_count, back_count, now_s() - began,
				(double)frames_emulated / 1e6);
	}
	return false;
}

int main(int argc, char **argv)
{
	const long run_most = argc > 1 ? atol(argv[1]) : RUN_MOST;
	const double began = now_s();
	long boot = 0, select_frames = 0;
	int backs = 0;
	unsigned lives_at_start;
	bool cleared = false, missed = false;

	if (getenv("CHAIN_W"))
		chain_value = atof(getenv("CHAIN_W"));
	if (getenv("DAMAGE_W"))
		damage_weight = atof(getenv("DAMAGE_W"));
	if (getenv("KILL_W"))
		kill_value = atof(getenv("KILL_W"));
	if (getenv("SETUP_W"))
		setup_value = atof(getenv("SETUP_W"));
	printf("the chain's hit worth %.0f, the damage's weight %.2f, a kill %.0f, a weakened enemy %.0f\n", chain_value,
		damage_weight, kill_value, setup_value);
	inputs = malloc(sizeof(uint16_t) * (size_t)(run_most + 20000));
	steps = calloc((size_t)(run_most / SEGMENT + 10), sizeof(step));
	start_game();
	frame_with(0xFFFFu);
	program_reset();
	/* the boot, a coin, Start and the ship select (type C), as a player would: until the play begins */
	for (boot = 0; ram_word(MODE) != 7u && boot < 20000; boot++)
	{
		unsigned held = 0;

		if (ram_word(MODE) == 0xBu)
		{
			/* type C (the owner's choice: its spread shot covers the screen, the most to hit): the stick down
			   twice (as training.c chooses), then the shot */
			select_frames++;
			if (select_frames > 60 && select_frames <= 120 && (select_frames - 61) % 30 < 8)
				held = CAVE_DOWN;
			else if (select_frames > 140 && (select_frames / 8) % 2)
				held = CAVE_BUTTON1;
		}
		else if (boot > 400)
		{
			const long beat = boot % 40;

			held = beat < 5 ? CAVE_COIN : beat >= 20 && beat < 25 ? CAVE_START : 0u;
		}
		frame_with((uint16_t)~held);
		keep_input((uint16_t)~held);
	}
	printf("play begins at frame %ld, ship type %c\n", boot, "ABC"[ram_word(SHIP + 0x1Cu) % 3u]);
	lives_at_start = ram_byte(LIVES);
	/* the search: the beam (BEAM=width, 16 if not given), or the greedy one (BEAM=0) */
	if (!getenv("BEAM") || atoi(getenv("BEAM")) > 0)
	{
		cleared = beam_search(getenv("BEAM") ? atoi(getenv("BEAM")) : 16, run_most, began);
		goto searched;
	}
	while (input_count - boot < run_most)
	{
		step *const here = &steps[step_count];
		double best = -1.0;
		int choice = -1;

		if (!here->state)
			here->state = program_state_save();
		for (int a = 0; a < ACTIONS; a++)
		{
			double v;

			if (here->tried >> a & 1u)
				continue;
			v = value(here->state, a);
			if (v > best)
			{
				best = v;
				choice = a;
			}
		}
		if (choice < 0)
		{
			/* nothing lives: back a few segments, the action taken there ruled out */
			int to = step_count - BACK;

			if (to < 0)
				to = 0;
			backs++;
			for (int s = step_count; s > to; s--)
			{
				program_state_free(steps[s].state);
				steps[s].state = NULL;
				steps[s].tried = 0;
			}
			step_count = to;
			steps[to].tried |= 1u << steps[to].action;
			input_count = boot + (long)to * SEGMENT;
			if (backs % 50 == 0)
				printf("  stuck at frame %ld: %d times back so far\n", input_count - boot, backs);
			if (backs > 3000)
			{
				printf("given up: too many dead ends\n");
				break;
			}
			continue;
		}
		/* the best action into the run */
		program_state_load(here->state);
		here->action = choice;
		hold(choice, 0, SEGMENT, true);
		step_count++;
		if (!flying())
		{
			missed = true;
			break;
		}
		if (ram_word(STAGE) != 0u)
		{
			cleared = true;
			break;
		}
		if (step_count % 200 == 0)
			printf("  frame %ld: score %u, chain %u (best %u), %d times back, %.0f s, %.1f M frames emulated\n",
				input_count - boot, score(), ram_word(CHAIN_HITS), ram_word(CHAIN_BEST), backs, now_s() - began,
				(double)frames_emulated / 1e6);
	}
searched:
	printf("the run: %ld frames of play, score %u, best chain %u, bees %u, lives %u, %s; %d times back; %.0f s (%.1f M "
		"frames emulated)\n", input_count - boot, score(), ram_word(CHAIN_BEST), ram_word(BEES_1), ram_byte(LIVES), cleared ? "area 1 cleared" : missed ? "a miss" : "not cleared",
		backs, now_s() - began, (double)frames_emulated / 1e6);
	{
		/* the run again from the reset, recorded as a replay */
		const uint64_t searched = cave_state_hash();
		const char *saved = NULL;
		uint64_t again;

		program_reset();
		replay_folder("replays/");
		replay_reset();
		for (long f = 0; f < input_count; f++)
		{
			replay_input(inputs[f], 0xFFFFu, 0);
			frame_with(inputs[f]);
			replay_after_frame();
		}
		again = cave_state_hash();
		if (replay_in_game())
		{
			replay_hold_unfinished();
			saved = replay_keep(true);
		}
		const bool no_miss = !missed && ram_byte(LIVES) == lives_at_start;
		bool played_back = false;

		printf("replay: %s\n", saved ? saved : "(not saved)");
		if (saved)
		{
			/* the replay played as the game's REPLAYS does: in step all through, the same board at the end */
			const char *const problem = replay_play(saved);
			bool in_step = !problem;

			while (in_step && replay_next(NULL))
			{
				program_run_frame();
				cave_sound_drop();
				in_step = replay_after_playback_frame();
			}
			played_back = in_step && cave_state_hash() == searched;
			replay_stop();
		}
		printf("%-60s %s\n", "area 1 cleared", cleared ? "PASS" : "FAIL");
		printf("%-60s %s\n", "no miss", no_miss ? "PASS" : "FAIL");
		printf("%-60s %s\n", "the run again from the reset: the same board", again == searched ? "PASS" : "FAIL");
		printf("%-60s %s\n", "its replay played back: in step, the same board", played_back ? "PASS" : "FAIL");
		return !(cleared && no_miss && again == searched && played_back);
	}
}
