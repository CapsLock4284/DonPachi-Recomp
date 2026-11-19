/**
 *    Name:        updater.c
 *    Description: the program updating itself (updater.h)
 *    Version:     0.1.0
 *    Created:     18-11-2026
 *
 *    license:BSD-3-Clause
 *    The package: a .zip (Windows: deflated or stored entries, names with / or \) or a .tar.gz (Linux: GNU or ustar
 *    tar, its files' executable bits kept), its files most often in one top folder (DonPachi-0.9.0-win64\...), which
 *    is left off: the files go to the program's own folder. The download checked against the SHA-256 GitHub gives
 *    for the asset ("digest"), when it gives one.
 */
#include "updater.h"

#include <SDL.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <direct.h>
#define make_dir(path) _mkdir(path)
#define SEPARATOR '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(path) mkdir(path, 0755)
#define SEPARATOR '/'
#endif

/* the runtime's (runtime/src/inflate.c): the raw DEFLATE stream 'in' into exactly 'out_size' bytes, 1 when it does */
int inflate_raw(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size);

#define AGENT       "Recomp-updater"
#define LIST_FILE   "update_old.txt"
#define NEW_FOLDER  "update_new"
#define MOST_BYTES  (256u * 1024u * 1024u)    /* (a download or a file no bigger than this) */
#define MOST_FILES  256

static char folder[1024], repository[128], suffix[64], version[32];
static char latest[32], asset_url[1024], asset_digest[65], problem[160];
static SDL_atomic_t state, progress;
static char *new_files[MOST_FILES];    /* (the unpacked files, as the package names them, with '/') */
static int new_count;
static size_t top_length;              /* (the top folder they share, "Name/", left off) */
static char program[1200];             /* (the program's own path, found before its file was renamed) */
static bool installed;

static void fail(const char *why)
{
	snprintf(problem, sizeof(problem), "%s", why);
	SDL_AtomicSet(&state, UPDATER_FAILED);
}

int updater_state(void)
{
	return SDL_AtomicGet(&state);
}

const char *updater_latest(void)
{
	return latest;
}

int updater_progress(void)
{
	return SDL_AtomicGet(&progress);
}

const char *updater_problem(void)
{
	return problem;
}

/* ---- the download: 'url' into memory (a malloc'd buffer and its size), the progress kept as it comes ---- */

#ifdef _WIN32
static unsigned char *fetch(const char *url, size_t *size, bool count_progress)
{
	wchar_t wide[1200], host[256], path[1100], extra[1100];
	URL_COMPONENTSW parts;
	HINTERNET session = NULL, connection = NULL, request = NULL;
	unsigned char *data = NULL;
	size_t have = 0, room = 0;
	DWORD status = 0, status_size = sizeof(status), length = 0, length_size = sizeof(length);
	bool ok = false;

	*size = 0;
	if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wide, (int)(sizeof(wide) / sizeof(wide[0]))))
		return NULL;
	memset(&parts, 0, sizeof(parts));
	parts.dwStructSize = sizeof(parts);
	parts.lpszHostName = host;
	parts.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
	parts.lpszUrlPath = path;
	parts.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(path[0]));
	parts.lpszExtraInfo = extra;
	parts.dwExtraInfoLength = (DWORD)(sizeof(extra) / sizeof(extra[0]));
	if (!WinHttpCrackUrl(wide, 0, 0, &parts))
		return NULL;
	if (wcslen(path) + wcslen(extra) + 1u > sizeof(path) / sizeof(path[0]))
		return NULL;
	wcscat(path, extra);    /* (a query, kept) */
	session = WinHttpOpen(L"" AGENT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)    /* (Windows before 8.1: no automatic proxy) */
		session = WinHttpOpen(L"" AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);
	if (session)
		connection = WinHttpConnect(session, host, parts.nPort, 0);
	if (connection)
		request = WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
	if (request && WinHttpSendRequest(request, L"Accept: application/vnd.github+json, */*\r\n", (DWORD)-1L,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, NULL)
			&& WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX) && status == 200)
	{
		if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &length, &length_size, WINHTTP_NO_HEADER_INDEX))
			length = 0;
		ok = true;
		for (;;)
		{
			DWORD available = 0, read = 0;

			if (!WinHttpQueryDataAvailable(request, &available))
			{
				ok = false;
				break;
			}
			if (available == 0)
				break;
			if (have + available > MOST_BYTES)
			{
				ok = false;
				break;
			}
			if (have + available > room)
			{
				const size_t more = (have + available) * 2u;
				unsigned char *const bigger = realloc(data, more);

				if (!bigger)
				{
					ok = false;
					break;
				}
				data = bigger;
				room = more;
			}
			if (!WinHttpReadData(request, data + have, available, &read))
			{
				ok = false;
				break;
			}
			have += read;
			if (count_progress && length)
				SDL_AtomicSet(&progress, (int)((uint64_t)have * 100u / length));
		}
	}
	if (request)
		WinHttpCloseHandle(request);
	if (connection)
		WinHttpCloseHandle(connection);
	if (session)
		WinHttpCloseHandle(session);
	if (!ok || have == 0)
	{
		free(data);
		return NULL;
	}
	*size = have;
	return data;
}
#else
/* (the curl command: its output in a file of the program's folder, then read; its progress from the file's size) */
static unsigned char *fetch(const char *url, size_t *size, bool count_progress)
{
	char command[2600], path[1200];
	FILE *file;
	unsigned char *data = NULL;
	long length;

	(void)count_progress;
	*size = 0;
	for (const char *c = url; *c; c++)
		if (*c == '\'' || (unsigned char)*c < ' ')
			return NULL;    /* (not passed to a shell) */
	for (const char *c = folder; *c; c++)
		if (*c == '\'')
			return NULL;
	snprintf(path, sizeof(path), "%supdate_download.tmp", folder);
	snprintf(command, sizeof(command), "curl -s -L --fail --max-filesize %u -A '" AGENT "' "
		"-H 'Accept: application/vnd.github+json, */*' -o '%s' '%s'", MOST_BYTES, path, url);
	if (system(command) != 0 || !(file = fopen(path, "rb")))
	{
		remove(path);
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) > 0 && (size_t)length <= MOST_BYTES
			&& fseek(file, 0, SEEK_SET) == 0 && (data = malloc((size_t)length)) != NULL
			&& fread(data, 1, (size_t)length, file) == (size_t)length)
		*size = (size_t)length;
	else
	{
		free(data);
		data = NULL;
	}
	fclose(file);
	remove(path);
	SDL_AtomicSet(&progress, 100);
	return data;
}
#endif

/* ---- SHA-256 (FIPS 180-4): the download checked ---- */

static uint32_t rotate(uint32_t value, int by)
{
	return value >> by | value << (32 - by);
}

static void sha256_block(uint32_t hash[8], const unsigned char block[64])
{
	static const uint32_t k[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};
	uint32_t w[64], v[8];

	for (int at = 0; at < 16; at++)
		w[at] = (uint32_t)block[at * 4] << 24 | (uint32_t)block[at * 4 + 1] << 16 | (uint32_t)block[at * 4 + 2] << 8
			| block[at * 4 + 3];
	for (int at = 16; at < 64; at++)
		w[at] = w[at - 16] + (rotate(w[at - 15], 7) ^ rotate(w[at - 15], 18) ^ w[at - 15] >> 3) + w[at - 7]
			+ (rotate(w[at - 2], 17) ^ rotate(w[at - 2], 19) ^ w[at - 2] >> 10);
	memcpy(v, hash, sizeof(v));
	for (int at = 0; at < 64; at++)
	{
		const uint32_t t1 = v[7] + (rotate(v[4], 6) ^ rotate(v[4], 11) ^ rotate(v[4], 25))
			+ ((v[4] & v[5]) ^ (~v[4] & v[6])) + k[at] + w[at];
		const uint32_t t2 = (rotate(v[0], 2) ^ rotate(v[0], 13) ^ rotate(v[0], 22))
			+ ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));

		memmove(v + 1, v, sizeof(uint32_t) * 7);
		v[4] += t1;
		v[0] = t1 + t2;
	}
	for (int at = 0; at < 8; at++)
		hash[at] += v[at];
}

/* 'data''s SHA-256 as 64 small hex digits in 'out' */
static void sha256_hex(const unsigned char *data, size_t size, char out[65])
{
	uint32_t hash[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
		0x5be0cd19 };
	unsigned char last[128] = { 0 };
	const size_t whole = size / 64u * 64u, rest = size - whole, tail = rest < 56u ? 64u : 128u;
	const uint64_t bits = (uint64_t)size * 8u;

	for (size_t at = 0; at < whole; at += 64u)
		sha256_block(hash, data + at);
	memcpy(last, data + whole, rest);
	last[rest] = 0x80;
	for (int at = 0; at < 8; at++)
		last[tail - 1u - (size_t)at] = (unsigned char)(bits >> (at * 8));
	sha256_block(hash, last);
	if (tail == 128u)
		sha256_block(hash, last + 64);
	for (int at = 0; at < 8; at++)
		snprintf(out + at * 8, 9, "%08x", (unsigned)hash[at]);
}

/* ---- the release: its tag and the package's asset, from GitHub's JSON ---- */

/* the string value of the next "key": in 'json' into 'out'; the place after it, or NULL */
static const char *json_string(const char *json, const char *key, char *out, size_t size)
{
	char pattern[64];
	const char *at;
	size_t length = 0;

	snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	if (!(at = strstr(json, pattern)))
		return NULL;
	at += strlen(pattern);
	while (*at == ' ' || *at == ':' || *at == '\t' || *at == '\r' || *at == '\n')
		at++;
	if (*at != '"')
		return NULL;
	for (at++; *at && *at != '"' && length + 1u < size; at++)
		out[length++] = *at == '\\' && at[1] ? *++at : *at;
	out[length] = '\0';
	return *at == '"' ? at + 1 : NULL;
}

/* "v0.3.0" or "0.3.0": its three numbers; false if it is not one */
static bool parse_version(const char *text, int numbers[3])
{
	if (*text == 'v' || *text == 'V')
		text++;
	return sscanf(text, "%d.%d.%d", &numbers[0], &numbers[1], &numbers[2]) == 3;
}

static bool newer(const char *candidate, const char *current)
{
	int a[3], b[3];

	if (!parse_version(candidate, a) || !parse_version(current, b))
		return false;
	for (int at = 0; at < 3; at++)
		if (a[at] != b[at])
			return a[at] > b[at];
	return false;
}

/* GitHub's answer read: the release's version, the package's address, the state */
static void read_release(const char *json)
{
	char tag[sizeof(latest)];

	if (!json_string(json, "tag_name", tag, sizeof(tag)))
	{
		fail("the newest release has no version");
		return;
	}
	snprintf(latest, sizeof(latest), "%s", tag[0] == 'v' || tag[0] == 'V' ? tag + 1 : tag);
	asset_url[0] = asset_digest[0] = '\0';
	for (const char *at = json, *before = json; at && !asset_url[0]; before = at)
	{
		char found[1024];
		const size_t length = strlen(suffix);

		at = json_string(at, "browser_download_url", found, sizeof(found));
		if (at && strlen(found) >= length && !strcmp(found + strlen(found) - length, suffix))
		{
			/* (its asset's "digest", given before the address: "sha256:" and 64 hex digits) */
			char digest[80];
			const char *field = before, *next;

			snprintf(asset_url, sizeof(asset_url), "%s", found);
			while ((next = strstr(field, "\"digest\"")) != NULL && next < at)
			{
				if (json_string(next, "digest", digest, sizeof(digest)) && !strncmp(digest, "sha256:", 7)
						&& strlen(digest + 7) == 64u)
				{
					for (int c = 0; c < 64; c++)
						asset_digest[c] = (char)tolower((unsigned char)digest[7 + c]);
					asset_digest[64] = '\0';
				}
				field = next + 8;
			}
		}
	}
	if (!newer(latest, version))
		SDL_AtomicSet(&state, UPDATER_UP_TO_DATE);
	else if (!asset_url[0])
		fail("the newest release has no package for this system");
	else
		SDL_AtomicSet(&state, UPDATER_AVAILABLE);
}

static int check_thread(void *unused)
{
	char url[256];
	size_t size;
	unsigned char *json, *ended;

	(void)unused;
	snprintf(url, sizeof(url), "https://api.github.com/repos/%s/releases/latest", repository);
	if (!(json = fetch(url, &size, false)))
	{
		fail("the newest release could not be looked up");
		return 0;
	}
	if (!(ended = realloc(json, size + 1u)))
	{
		free(json);
		fail("no memory");
		return 0;
	}
	ended[size] = '\0';
	read_release((const char *)ended);
	free(ended);
	return 0;
}

void updater_check(const char *program_folder, const char *repo, const char *package_suffix, const char *current)
{
	SDL_Thread *thread;

	snprintf(folder, sizeof(folder), "%s", program_folder);
	snprintf(repository, sizeof(repository), "%s", repo);
	snprintf(suffix, sizeof(suffix), "%s", package_suffix);
	snprintf(version, sizeof(version), "%s", current);
	SDL_AtomicSet(&state, UPDATER_CHECKING);
	if ((thread = SDL_CreateThread(check_thread, "updater check", NULL)) != NULL)
		SDL_DetachThread(thread);
	else
		fail("no thread for the check");
}

/* ---- unpacking: the package's files into update_new/ ---- */

static uint32_t little16(const unsigned char *at)
{
	return (uint32_t)at[0] | (uint32_t)at[1] << 8;
}

static uint32_t little32(const unsigned char *at)
{
	return little16(at) | little16(at + 2) << 16;
}

/* the folders of 'path' made (not its last part) */
static void make_parents(const char *path)
{
	char partial[1600];
	const size_t start = strlen(folder);

	for (size_t at = 0; path[at] && at + 1u < sizeof(partial); at++)
	{
		partial[at] = path[at];
		if (at > start && (path[at] == '/' || path[at] == '\\'))
		{
			partial[at] = '\0';
			make_dir(partial);
			partial[at] = path[at];
		}
	}
}

/* a name of the package's safe to write: no parent folders, no root, no drive */
static bool safe_name(const char *name)
{
	if (!name[0] || name[0] == '/' || name[0] == '\\' || strchr(name, ':'))
		return false;
	for (const char *at = name; (at = strstr(at, "..")) != NULL; at += 2)
		if ((at == name || at[-1] == '/' || at[-1] == '\\') && (at[2] == '\0' || at[2] == '/' || at[2] == '\\'))
			return false;
	return true;
}

/* 'name' (the package's, with / or \) as a path in 'folder'/'within' */
static void local_path(char *out, size_t size, const char *within, const char *name)
{
	char *c;

	snprintf(out, size, "%s%s%s", folder, within, name);
	for (c = out + strlen(folder); *c; c++)
		if (*c == '/' || *c == '\\')
			*c = SEPARATOR;
}

/* one file of the package written into update_new/ */
static bool store(const char *raw_name, const unsigned char *data, size_t length, bool executable)
{
	char name[512], path[1700];
	FILE *file;
	bool ok;

	snprintf(name, sizeof(name), "%s", raw_name);
	for (char *c = name; *c; c++)
		if (*c == '\\')
			*c = '/';
	while (name[0] == '.' && name[1] == '/')
		memmove(name, name + 2, strlen(name + 2) + 1u);
	if (!name[0] || name[strlen(name) - 1] == '/')
		return true;    /* (a folder) */
	if (!safe_name(name) || new_count >= MOST_FILES)
		return false;
	local_path(path, sizeof(path), NEW_FOLDER "/", name);
	make_parents(path);
	if (!(file = fopen(path, "wb")))
		return false;
	ok = fwrite(data, 1, length, file) == length;
	ok = fclose(file) == 0 && ok;
#ifndef _WIN32
	chmod(path, executable ? 0755 : 0644);
#else
	(void)executable;
#endif
	if (!ok || !(new_files[new_count] = malloc(strlen(name) + 1u)))
		return false;
	strcpy(new_files[new_count++], name);
	return true;
}

static bool unpack_zip(const unsigned char *zip, size_t size)
{
	size_t end, entry;
	uint32_t entries;

	if (size < 22)
		return false;
	for (end = size - 22;; end--)
	{
		if (little32(zip + end) == 0x06054b50u)
			break;
		if (end == 0 || size - end > 22u + 65535u)
			return false;
	}
	entries = little16(zip + end + 10);
	entry = little32(zip + end + 16);
	for (uint32_t index = 0; index < entries; index++)
	{
		char name[512];
		uint32_t method, packed, unpacked, length;
		size_t local;
		unsigned char *data;
		bool ok;

		if (entry + 46u > size || little32(zip + entry) != 0x02014b50u)
			return false;
		method = little16(zip + entry + 10);
		packed = little32(zip + entry + 20);
		unpacked = little32(zip + entry + 24);
		length = little16(zip + entry + 28);
		local = little32(zip + entry + 42);
		if (length >= sizeof(name) || entry + 46u + length > size)
			return false;
		memcpy(name, zip + entry + 46, length);
		name[length] = '\0';
		entry += 46u + length + little16(zip + entry + 30) + little16(zip + entry + 32);
		if (length && (name[length - 1] == '/' || name[length - 1] == '\\'))
			continue;    /* (a folder) */
		if (local + 30u > size || little32(zip + local) != 0x04034b50u)
			return false;
		local += 30u + little16(zip + local + 26) + little16(zip + local + 28);
		if (local > size || packed > size - local || unpacked > MOST_BYTES || !(data = malloc(unpacked ? unpacked : 1u)))
			return false;
		ok = method == 0u ? packed == unpacked && (memcpy(data, zip + local, unpacked), true)
			: method == 8u && inflate_raw(zip + local, packed, data, unpacked) == 1;
		ok = ok && store(name, data, unpacked, false);
		free(data);
		if (!ok)
			return false;
	}
	return true;
}

static uint64_t octal(const unsigned char *field, size_t length)
{
	uint64_t value = 0;

	for (size_t at = 0; at < length && field[at] >= '0' && field[at] <= '7'; at++)
		value = value * 8u + (uint64_t)(field[at] - '0');
	return value;
}

static bool unpack_tar(const unsigned char *tar, size_t size)
{
	char long_name[512] = "";

	for (size_t at = 0; at + 512u <= size;)
	{
		const unsigned char *const header = tar + at;
		const uint64_t length = octal(header + 124, 12);
		const char type = (char)header[156];
		char name[512];

		if (header[0] == 0)
			break;    /* (the end) */
		if (length > size - at - 512u)
			return false;
		if (long_name[0])
		{
			snprintf(name, sizeof(name), "%s", long_name);
			long_name[0] = '\0';
		}
		else if (!memcmp(header + 257, "ustar", 5) && header[345])
			snprintf(name, sizeof(name), "%.155s/%.100s", (const char *)header + 345, (const char *)header);
		else
			snprintf(name, sizeof(name), "%.100s", (const char *)header);
		if (type == 'L')    /* (GNU: the next one's long name) */
			snprintf(long_name, sizeof(long_name), "%.*s", (int)(length < sizeof(long_name) ? length : 511u),
				(const char *)header + 512);
		else if (type == 'x')    /* (pax: the next one's path, if it gives one) */
		{
			for (size_t record = 0; record < length;)
			{
				const char *const text = (const char *)header + 512 + record;
				const unsigned long record_length = strtoul(text, NULL, 10);
				const char *const key = memchr(text, ' ', length - record);

				if (record_length == 0 || record_length > length - record)
					break;
				if (key && !strncmp(key + 1, "path=", 5))
					snprintf(long_name, sizeof(long_name), "%.*s",
						(int)(text + record_length - 1 - (key + 6)), key + 6);
				record += record_length;
			}
		}
		else if ((type == '0' || type == '\0') && !store(name, header + 512, (size_t)length,
				(octal(header + 100, 8) & 0111u) != 0))
			return false;
		at += 512u + ((size_t)length + 511u) / 512u * 512u;
	}
	return true;
}

static bool unpack_tar_gz(const unsigned char *gz, size_t size)
{
	size_t at = 10, out_size;
	unsigned char *tar;
	bool ok;

	if (size < 18 || gz[0] != 0x1f || gz[1] != 0x8b || gz[2] != 8)
		return false;
	if (gz[3] & 4)
		at += 2u + little16(gz + 10);
	if (gz[3] & 8)
		while (at < size && gz[at++])
			;
	if (gz[3] & 16)
		while (at < size && gz[at++])
			;
	if (gz[3] & 2)
		at += 2;
	out_size = little32(gz + size - 4);
	if (at + 8u > size || out_size == 0 || out_size > MOST_BYTES || !(tar = malloc(out_size)))
		return false;
	ok = inflate_raw(gz + at, size - at - 8u, tar, out_size) == 1 && unpack_tar(tar, out_size);
	free(tar);
	return ok;
}

/* the package unpacked; the top folder its files share (if all are in one) found */
static void clear_new_folder(void);

static bool unpack(const unsigned char *package, size_t size)
{
	const char *slash;

	clear_new_folder();    /* (what a download before left) */
	for (int index = 0; index < new_count; index++)
		free(new_files[index]);
	new_count = 0;
	top_length = 0;
	if (!(size >= 4 && little32(package) == 0x04034b50u ? unpack_zip(package, size)
			: unpack_tar_gz(package, size)) || new_count == 0)
		return false;
	if ((slash = strchr(new_files[0], '/')) != NULL)
	{
		top_length = (size_t)(slash - new_files[0]) + 1u;
		for (int index = 1; index < new_count; index++)
			if (strncmp(new_files[index], new_files[0], top_length))
				top_length = 0;
	}
	return true;
}

static int download_thread(void *unused)
{
	size_t size;
	unsigned char *package;

	(void)unused;
	if (!(package = fetch(asset_url, &size, true)))
	{
		fail("the download failed");
		return 0;
	}
	if (asset_digest[0])
	{
		char digest[65];

		sha256_hex(package, size, digest);
		if (strcmp(digest, asset_digest))
		{
			free(package);
			fail("the download was damaged (its SHA-256 is not the release's)");
			return 0;
		}
	}
	if (!unpack(package, size))
	{
		free(package);
		fail("the package could not be unpacked");
		return 0;
	}
	free(package);
	SDL_AtomicSet(&state, UPDATER_READY);
	return 0;
}

void updater_download(void)
{
	SDL_Thread *thread;
	const int now = updater_state();

	if (now != UPDATER_AVAILABLE && !(now == UPDATER_FAILED && asset_url[0]))
		return;
	SDL_AtomicSet(&progress, 0);
	SDL_AtomicSet(&state, UPDATER_DOWNLOADING);
	if ((thread = SDL_CreateThread(download_thread, "updater download", NULL)) != NULL)
		SDL_DetachThread(thread);
	else
		fail("no thread for the download");
}

/* ---- putting the new files in place ---- */

static bool move_file(const char *from, const char *to)
{
#ifdef _WIN32
	return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
#else
	return rename(from, to) == 0;
#endif
}

static bool exists(const char *path)
{
	FILE *const file = fopen(path, "rb");

	if (file)
		fclose(file);
	return file != NULL;
}

/* the update_new/ folder's files and folders taken away (what is left after the files were moved) */
static void clear_new_folder(void)
{
	char path[1700];

	for (int index = 0; index < new_count; index++)
	{
		local_path(path, sizeof(path), NEW_FOLDER "/", new_files[index]);
		remove(path);
		/* (its folders, deepest first: removing a folder that is not empty does nothing) */
		for (char *cut = strrchr(path, SEPARATOR); cut && cut > path + strlen(folder); cut = strrchr(path, SEPARATOR))
		{
			*cut = '\0';
#ifdef _WIN32
			_rmdir(path);
#else
			rmdir(path);
#endif
		}
	}
}

bool updater_install(void)
{
	char list_path[1200];
	FILE *list;
	int placed = 0;

	if (updater_state() != UPDATER_READY)
		return false;
	/* (the program's own path first: once it is renamed, the running program is the .old) */
#ifdef _WIN32
	if (!GetModuleFileNameA(NULL, program, sizeof(program)))
		program[0] = '\0';
#else
	{
		const ssize_t length = readlink("/proc/self/exe", program, sizeof(program) - 1u);

		program[length > 0 ? length : 0] = '\0';
	}
#endif
	snprintf(list_path, sizeof(list_path), "%s" LIST_FILE, folder);
	if (!(list = fopen(list_path, "a")))
	{
		fail("the program's folder can't be written");
		return false;
	}
	for (; placed < new_count; placed++)
	{
		char target[1700], old[1720], fresh[1700];

		local_path(target, sizeof(target), "", new_files[placed] + top_length);
		local_path(fresh, sizeof(fresh), NEW_FOLDER "/", new_files[placed]);
		snprintf(old, sizeof(old), "%s.old", target);
		make_parents(target);
		remove(old);
		if (exists(target))
		{
			if (!move_file(target, old))
				break;
			fprintf(list, "%s\n", new_files[placed] + top_length);
		}
		if (!move_file(fresh, target))
		{
			move_file(old, target);    /* (back) */
			break;
		}
	}
	fclose(list);
	if (placed < new_count)
	{
		/* (some could not be put in place: the ones that were go back) */
		for (int back = placed - 1; back >= 0; back--)
		{
			char target[1700], old[1720];

			local_path(target, sizeof(target), "", new_files[back] + top_length);
			snprintf(old, sizeof(old), "%s.old", target);
			if (exists(old))
				move_file(old, target);
		}
		fail("the new files could not be put in place (is the folder read-only?)");
		return false;
	}
	clear_new_folder();
	installed = true;
	return true;
}

bool updater_restart(char **argv)
{
	if (!installed || !program[0])
		return false;
#ifdef _WIN32
	{
		char command[4096];
		STARTUPINFOA startup;
		PROCESS_INFORMATION process;
		size_t at;

		at = (size_t)snprintf(command, sizeof(command), "\"%s\"", program);
		for (int index = 1; argv && argv[index] && at < sizeof(command); index++)
			at += (size_t)snprintf(command + at, sizeof(command) - at, " \"%s\"", argv[index]);
		memset(&startup, 0, sizeof(startup));
		startup.cb = sizeof(startup);
		if (!CreateProcessA(program, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
			return false;
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return true;
	}
#else
	if (argv && argv[0])
		execv(program, argv);
	return false;
#endif
}

void updater_cleanup(const char *program_folder)
{
	char list_path[1200], line[512], path[1800], kept[MOST_FILES][256];
	int kept_count = 0;
	FILE *list;

	snprintf(list_path, sizeof(list_path), "%s" LIST_FILE, program_folder);
	if (!(list = fopen(list_path, "r")))
		return;
	while (fgets(line, sizeof(line), list))
	{
		line[strcspn(line, "\r\n")] = '\0';
		if (!line[0] || !safe_name(line))
			continue;
		snprintf(path, sizeof(path), "%s%s.old", program_folder, line);
		for (char *c = path + strlen(program_folder); *c; c++)
			if (*c == '/' || *c == '\\')
				*c = SEPARATOR;
		/* (the program before may still be closing: what can't go yet is kept for the next start) */
		if (remove(path) != 0 && exists(path) && kept_count < MOST_FILES && strlen(line) < sizeof(kept[0]))
			strcpy(kept[kept_count++], line);
	}
	fclose(list);
	if (kept_count == 0)
		remove(list_path);
	else if ((list = fopen(list_path, "w")) != NULL)
	{
		for (int index = 0; index < kept_count; index++)
			fprintf(list, "%s\n", kept[index]);
		fclose(list);
	}
}
