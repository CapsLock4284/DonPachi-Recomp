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

#define MAX_WRITES 64

typedef struct
{
	uint32_t address;
	uint32_t value;
	int bytes;           /* 1, 2 or 4 */
	int every_frame;     /* run: every frame; on: once */
	int done;
} cheat_write;

static cheat_write writes[MAX_WRITES];
static int write_count;

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

const char *cheats_enable(const char *path, const char *request)
{
	static char problem[256];
	char wanted[128], *equals, name[128];
	const char *at, *end, *found = NULL;
	char *text = read_text(path);
	const char *parameter = NULL;

	if (!text)
	{
		snprintf(problem, sizeof(problem), "no cheat file %s", path);
		return problem;
	}
	snprintf(wanted, sizeof(wanted), "%s", request);
	if ((equals = strchr(wanted, '=')) != NULL)
	{
		*equals = '\0';
		parameter = equals + 1;
	}
	for (at = text; (at = strstr(at, "<cheat ")) != NULL; at++)
		if (attribute(at, "desc", name, sizeof(name)) && strspn(name, " ") != strlen(name)
			&& contains_nocase(name, wanted))
		{
			found = at;
			break;
		}
	if (!found)
	{
		snprintf(problem, sizeof(problem), "no cheat named like \"%s\" in %s", wanted, path);
		free(text);
		return problem;
	}
	end = cheat_end(found);
	for (at = found; (at = strstr(at, "<script")) != NULL && at < end; at++)
	{
		char state[16] = "run";
		const char *script_end = strstr(at, "</script>");

		attribute(at, "state", state, sizeof(state));
		if (!strcmp(state, "off"))
			continue;
		for (const char *action = at; (action = strstr(action, "<action>")) != NULL
				&& (!script_end || action < script_end); action++)
		{
			const char *body = action + 8;
			char kind;
			unsigned long address;
			char *after;
			uint32_t value;

			while (*body == ' ')
				body++;
			if (strncmp(body, "maincpu.p", 9) != 0 || !strchr("bwd", body[9]) || body[10] != '@')
			{
				snprintf(problem, sizeof(problem), "the cheat \"%s\" patches the program or uses MAME's expressions, "
					"which the decompiled game does not have", name);
				write_count = 0;
				free(text);
				return problem;
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
			if (!strncmp(after, "param", 5))
			{
				if (!parameter)
				{
					snprintf(problem, sizeof(problem), "the cheat \"%s\" needs a value: --cheat \"%s=VALUE\"", name, name);
					free(text);
					return problem;
				}
				value = (uint32_t)strtoul(parameter, NULL, 0);
			}
			else
				value = (uint32_t)strtoul(after, NULL, 16);
			if (write_count == MAX_WRITES)
				break;
			writes[write_count++] = (cheat_write){ (uint32_t)address & 0xFFFFFFu, value,
				kind == 'b' ? 1 : kind == 'w' ? 2 : 4, strcmp(state, "on") != 0, 0 };
		}
	}
	free(text);
	return NULL;
}

void cheats_apply(void)
{
	for (int index = 0; index < write_count; index++)
	{
		cheat_write *const write = &writes[index];

		if (!write->every_frame && write->done)
			continue;
		write->done = 1;
		if (write->bytes == 1)
			cave_write16(write->address & ~1u, (uint16_t)((write->value & 0xFFu) * 0x101u),
				(write->address & 1u) ? 0x00FFu : 0xFF00u);
		else if (write->bytes == 2)
			cave_write16(write->address, (uint16_t)write->value, 0xFFFFu);
		else
		{
			cave_write16(write->address, (uint16_t)(write->value >> 16), 0xFFFFu);
			cave_write16(write->address + 2u, (uint16_t)write->value, 0xFFFFu);
		}
	}
}
