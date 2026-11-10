/**
 *    Name:        cheats.c
 *    Description: MAME's cheats that write the work RAM (cave/cheats.h)
 *    Version:     0.1.0 (draft)
 *    Created:     09-11-2026
 *
 *    license:BSD-3-Clause
 */
#include "cave/cheats.h"

#include "cave/machine.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_WRITES 128
#define MAX_CHEATS 64

typedef struct
{
	uint32_t address;
	uint32_t value;
	int bytes;           /* 1, 2 or 4 */
	int every_frame;     /* run: every frame; on: once */
	int param;           /* the value is the cheat's parameter */
	int done;
} cheat_write;

typedef struct
{
	char name[64];
	int first, count;    /* its writes */
	int usable;          /* only RAM writes (no program patches, no MAME expressions) */
	int needs_value;     /* it has a <parameter> */
	int on;
	uint32_t value;
} cheat;

static cheat_write writes[MAX_WRITES];
static int write_count;
static cheat cheats[MAX_CHEATS];
static int cheat_total;
static char loaded_path[512];

static char *read_text(const char *path)
{
	FILE *input = fopen(path, "rb");
	char *text = NULL;
	long length;

	if (!input)
		return NULL;
	if (fseek(input, 0, SEEK_END) == 0 && (length = ftell(input)) >= 0 && fseek(input, 0, SEEK_SET) == 0
		&& (text = malloc((size_t)length + 1u)) != NULL)
	{
		if (fread(text, 1, (size_t)length, input) != (size_t)length)
		{
			free(text);
			text = NULL;
		}
		else
			text[length] = '\0';
	}
	fclose(input);
	return text;
}

/* The value of the attribute 'name' in the tag at 'tag' (up to its '>'), into 'out' */
static int attribute(const char *tag, const char *name, char *out, size_t size)
{
	const char *end = strchr(tag, '>'), *at = tag;
	const size_t length = strlen(name);

	while (end && (at = strstr(at, name)) != NULL && at < end)
	{
		if (at[length] == '=' && at[length + 1] == '"')
		{
			const char *value = at + length + 2, *close = strchr(value, '"');
			size_t count;

			if (!close)
				return 0;
			count = (size_t)(close - value) < size - 1 ? (size_t)(close - value) : size - 1;
			memcpy(out, value, count);
			out[count] = '\0';
			return 1;
		}
		at += length;
	}
	return 0;
}

static int contains_nocase(const char *text, const char *part)
{
	for (; *text; text++)
	{
		size_t index = 0;

		while (part[index] && tolower((unsigned char)text[index]) == tolower((unsigned char)part[index]))
			index++;
		if (!part[index])
			return 1;
	}
	return !*part;
}

/* The cheat's text from 'start' to its </cheat> (or its self-closing end) */
static const char *cheat_end(const char *start)
{
	const char *close = strstr(start, "</cheat>"), *gt = strchr(start, '>');

	if (gt && gt > start && gt[-1] == '/')
		return gt + 1;
	return close ? close : start + strlen(start);
}

void cheats_list(const char *path)
{
	char *text = read_text(path);
	const char *at;

	if (!text)
	{
		printf("no cheat file %s\n", path);
		return;
	}
	for (at = text; (at = strstr(at, "<cheat ")) != NULL; at++)
	{
		char name[128];

		if (attribute(at, "desc", name, sizeof(name)) && strspn(name, " ") != strlen(name))
			printf("  %s%s\n", name, strstr(at, "<parameter") && strstr(at, "<parameter") < cheat_end(at)
				? "  (=VALUE)" : "");
	}
	free(text);
}

/* The file's cheats into the table (once a file: cheats_load again with another path reads that one) */
static const char *load(const char *path)
{
	static char problem[256];
	char *text;
	const char *at;

	if (cheat_total && !strcmp(path, loaded_path))
		return NULL;
	if (!(text = read_text(path)))
	{
		snprintf(problem, sizeof(problem), "no cheat file %s", path);
		return problem;
	}
	snprintf(loaded_path, sizeof(loaded_path), "%s", path);
	cheat_total = 0;
	write_count = 0;
	for (at = text; (at = strstr(at, "<cheat ")) != NULL && cheat_total < MAX_CHEATS; at++)
	{
		cheat *const c = &cheats[cheat_total];
		const char *end = cheat_end(at), *script;

		if (!attribute(at, "desc", c->name, sizeof(c->name)) || strspn(c->name, " ") == strlen(c->name))
			continue;    /* (the file's blank lines) */
		c->first = write_count;
		c->count = 0;
		c->usable = 1;
		c->on = 0;
		c->value = 0;
		c->needs_value = strstr(at, "<parameter") && strstr(at, "<parameter") < end;
		for (script = at; (script = strstr(script, "<script")) != NULL && script < end; script++)
		{
			char state[16] = "run";
			const char *script_end = strstr(script, "</script>");

			attribute(script, "state", state, sizeof(state));
			if (!strcmp(state, "off"))
				continue;
			for (const char *action = script; (action = strstr(action, "<action>")) != NULL
					&& (!script_end || action < script_end); action++)
			{
				const char *body = action + 8;
				char kind, *after;
				unsigned long address;

				while (*body == ' ')
					body++;
				if (strncmp(body, "maincpu.p", 9) != 0 || !strchr("bwd", body[9]) || body[10] != '@')
				{
					c->usable = 0;
					continue;
				}
				kind = body[9];
				address = strtoul(body + 11, &after, 16);
				while (*after == ' ')
					after++;
				if (*after != '=')
					continue;
				after++;
				while (*after == ' ')
					after++;
				if (write_count == MAX_WRITES)
				{
					c->usable = 0;
					continue;
				}
				writes[write_count++] = (cheat_write){ (uint32_t)address & 0xFFFFFFu,
					strncmp(after, "param", 5) ? (uint32_t)strtoul(after, NULL, 16) : 0u,
					kind == 'b' ? 1 : kind == 'w' ? 2 : 4, strcmp(state, "on") != 0, !strncmp(after, "param", 5), 0 };
				c->count++;
			}
		}
		cheat_total++;
	}
	free(text);
	return NULL;
}

const char *cheats_load(const char *path)
{
	return load(path);
}

int cheats_count(void)
{
	return cheat_total;
}

const char *cheats_name(int index)
{
	return index >= 0 && index < cheat_total ? cheats[index].name : "";
}

bool cheats_usable(int index)
{
	return index >= 0 && index < cheat_total && cheats[index].usable && !cheats[index].needs_value;
}

bool cheats_on(int index)
{
	return index >= 0 && index < cheat_total && cheats[index].on;
}

void cheats_set(int index, bool on)
{
	if (index < 0 || index >= cheat_total || !cheats[index].usable)
		return;
	if (on && !cheats[index].on)
		for (int write = cheats[index].first; write < cheats[index].first + cheats[index].count; write++)
			writes[write].done = 0;    /* (its "on" writes once more) */
	cheats[index].on = on;
}

bool cheats_any_on(void)
{
	for (int index = 0; index < cheat_total; index++)
		if (cheats[index].on)
			return true;
	return false;
}

void cheats_off(void)
{
	for (int index = 0; index < cheat_total; index++)
		cheats[index].on = 0;
}

const char *cheats_enable(const char *path, const char *request)
{
	static char problem[256];
	char wanted[128], *equals;
	const char *parameter = NULL, *failed = load(path);
	int found = -1;

	if (failed)
		return failed;
	snprintf(wanted, sizeof(wanted), "%s", request);
	if ((equals = strchr(wanted, '=')) != NULL)
	{
		*equals = '\0';
		parameter = equals + 1;
	}
	for (int index = 0; index < cheat_total && found < 0; index++)
		if (contains_nocase(cheats[index].name, wanted))
			found = index;
	if (found < 0)
	{
		snprintf(problem, sizeof(problem), "no cheat named like \"%s\" in %s", wanted, path);
		return problem;
	}
	if (!cheats[found].usable)
	{
		snprintf(problem, sizeof(problem), "the cheat \"%s\" patches the program or uses MAME's expressions, "
			"which the decompiled game does not have", cheats[found].name);
		return problem;
	}
	if (cheats[found].needs_value)
	{
		if (!parameter)
		{
			snprintf(problem, sizeof(problem), "the cheat \"%s\" needs a value: --cheat \"%s=VALUE\"",
				cheats[found].name, cheats[found].name);
			return problem;
		}
		cheats[found].value = (uint32_t)strtoul(parameter, NULL, 0);
		cheats[found].needs_value = 0;
	}
	cheats_set(found, true);
	return NULL;
}

void cheats_apply(void)
{
	for (int index = 0; index < cheat_total; index++)
	{
		if (!cheats[index].on)
			continue;
		for (int at = cheats[index].first; at < cheats[index].first + cheats[index].count; at++)
		{
			cheat_write *const write = &writes[at];
			const uint32_t value = write->param ? cheats[index].value : write->value;

			if (!write->every_frame && write->done)
				continue;
			write->done = 1;
			if (write->bytes == 1)
				cave_write16(write->address & ~1u, (uint16_t)((value & 0xFFu) * 0x101u),
					(write->address & 1u) ? 0x00FFu : 0xFF00u);
			else if (write->bytes == 2)
				cave_write16(write->address, (uint16_t)value, 0xFFFFu);
			else
			{
				cave_write16(write->address, (uint16_t)(value >> 16), 0xFFFFu);
				cave_write16(write->address + 2u, (uint16_t)value, 0xFFFFu);
			}
		}
	}
}
