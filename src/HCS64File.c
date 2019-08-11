// Copyright intealls
// License: GPL v3

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <assert.h>

#include <archive.h>
#include <archive_entry.h>
#include <gme/gme.h>

#include <tinydir.h>

#include "Globals.h"

typedef struct M3UEntry {
	bool use;
	char name[_TINYDIR_PATH_MAX];
	char* data;
	size_t len;
} M3UEntry;

typedef struct M3UEntries {
	M3UEntry* list;
	size_t n,
	       size,
	       realloc_add;
} M3UEntries;

static M3UEntries*
CreateM3UEntries(size_t size,
                 size_t realloc_add)
{
	M3UEntries* ents = (M3UEntries*) malloc(sizeof(M3UEntries));
	assert(ents);

	ents->list = (M3UEntry*) malloc(size * sizeof(M3UEntry));
	assert(ents->list);

	ents->n = 0;
	ents->size = size;
	ents->realloc_add = realloc_add;

	return ents;
}

static void
AddM3UEntry(M3UEntries* ents,
            char const* name,
            char* data,
            size_t len)
{
	assert(ents);

	M3UEntry* entry;

	if (ents->n + 1 == ents->size) {
		ents->list = (M3UEntry*) realloc(ents->list, sizeof(M3UEntry) *
		                                 (ents->size + ents->realloc_add));
		assert(ents->list);
		ents->size += ents->realloc_add;
	}

	entry = &(ents->list[ents->n]);

	assert(memccpy(entry->name, name, '\0', _TINYDIR_PATH_MAX) != NULL);

	entry->use = false;
	entry->data = data;
	entry->len = len;

	ents->n++;
}

static int
M3UCmp(const void* a, const void* b)
{
	M3UEntry* ma = (M3UEntry*) a;
	M3UEntry* mb = (M3UEntry*) b;

	return strcmp(ma->name, mb->name);
}

static bool
FindM3UFileName(char dest[_TINYDIR_PATH_MAX],
                const char* m3u,
                size_t len)
{
	char* p = (char*) m3u;
	char* fnamep = NULL;

	bool newline = true;
	bool escape = false;

	while (p < m3u + len) {
		if (newline && *p == '#') {
			while (*p++ != '\n');
			newline = true;
			continue;
		} else if (*p == '\n') {
			newline = true;
		} else if (*p == '\\') {
			escape = true;
		} else if (escape) {
			escape = false;
		} else if (newline) {
			fnamep = p;
			newline = false;
		} else if (*p == ',' && p + 1 < m3u + len) {
			/* TODO: make sure this won't fail */
			char* u = p + 1;

			// check for "filename::type,[$/0-9]"
			if (fnamep != NULL && (*u == '$' || isdigit(*u))) {
				char* sysp;
				size_t fnlen = p - fnamep;

				assert(fnlen < _TINYDIR_PATH_MAX);
				memcpy(dest, fnamep, fnlen);
				dest[fnlen] = '\0';

				sysp = strrchr((const char*) dest, ':');

				if (sysp != NULL && sysp > dest && *(sysp - 1) == ':') {
					*(sysp - 1) = '\0';
					return true;
				}
			}
		}
		p++;
	}

	return false;
}

static char*
CreateM3U(M3UEntries* ents,
          char song_fname[_TINYDIR_PATH_MAX],
          size_t* len)
{
	size_t tot_size = 0;
	char m3u_fname[_TINYDIR_PATH_MAX];
	char* m3u, *p;

	if (ents->n == 0)
		return NULL;

	if (ents->n > 1)
		qsort(ents->list, ents->n, sizeof(M3UEntry), M3UCmp);

	for (size_t i = 0; i < ents->n; i++) {
		bool r = FindM3UFileName(m3u_fname,
		                         ents->list[i].data,
		                         ents->list[i].len);

		if (r && strncasecmp(m3u_fname,
		                     song_fname,
		                     _TINYDIR_PATH_MAX) == 0) {
			ents->list[i].use = true;
			tot_size += ents->list[i].len;
		}
	}

	tot_size += ents->n; // newlines

	m3u = (char*) malloc(tot_size);
	assert(m3u);

	p = m3u;

	for (size_t i = 0; i < ents->n; i++) {
		if (ents->list[i].use) {
			memcpy(p, ents->list[i].data, ents->list[i].len);
			p += ents->list[i].len;
			*p = '\n';
			p++;
		}
	}

	*len = tot_size;

	return m3u;
}

static void
FreeM3UEntries(M3UEntries* ents)
{
	for (size_t i = 0; i < ents->n; i++)
		free(ents->list[i].data);

	free(ents->list);
	free(ents);
}

static bool
HasExtension(const char* name,
             const char ext_cmp[_TINYDIR_PATH_MAX])
{
	char* p = strrchr(name, '.');

	if (p == NULL)
		return false;

	p++;

	return strncasecmp(p, ext_cmp, strlen(ext_cmp)) == 0;
}

bool
TryOpenHCS64(const void* data,
             size_t len,
             char** song,
             size_t* song_len,
             char** m3u,
             size_t* m3u_len)
{
	struct archive* a;
	struct archive_entry* entry;
	int r;

	char song_fname[_TINYDIR_PATH_MAX];
	bool got_song = false;

	*m3u = 0;
	*m3u_len = 0;

	M3UEntries* ents;

	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);

	r = archive_read_open_memory(a, data, len);

	if (r != ARCHIVE_OK)
		goto error;

	ents = CreateM3UEntries(4, 4);

	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		size_t len = archive_entry_size(entry);
		const char* name = archive_entry_pathname(entry);

		if (name == NULL)
			continue;

		if (!got_song && (HasExtension(name, "gbs") ||
		                  HasExtension(name, "nsf") ||
		                  HasExtension(name, "hes") ||
		                  HasExtension(name, "kss"))) {
			// This uses the first found song in the archive,
			// multiple songs in a single archive is not supported
			*song = (char*) malloc(len);
			assert(*song);

			*song_len = len;
			archive_read_data(a, *song, *song_len);

			assert(memccpy(song_fname, name, '\0', _TINYDIR_PATH_MAX) != NULL);

			got_song = true;
		} else if (HasExtension(name, "m3u")) {
			char* m3u_entry = (char*) malloc(len);
			assert(m3u_entry);

			archive_read_data(a, m3u_entry, len);

			AddM3UEntry(ents, name, m3u_entry, len);
		}
	}

	archive_read_free(a);

	if (got_song)
		*m3u = CreateM3U(ents, song_fname, m3u_len);

	FreeM3UEntries(ents);

	return got_song;
error:
	archive_read_free(a);
	return false;
}
