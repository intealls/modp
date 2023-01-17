// Copyright intealls
// License: GPL v3

#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <sys/stat.h>

#include <inttypes.h>

#include <tinydir.h>

#include "Directory.h"
#include "Globals.h"

typedef struct LocalDir_Entry LocalDir_Entry;

struct LocalDir_Entry {
	LocalDir_Entry* next, *prev;

	char path[_TINYDIR_PATH_MAX];
	size_t subdir_idx;

	tinydir_file* files;

	size_t n_dirs,
	       n_files;
};

typedef struct LocalDir_Data {
	LocalDir_Entry* root, *head;
} LocalDir_Data;

#define DataObjects(a, b, c) \
	LocalDir_Data* (a) = (LocalDir_Data*) (c)->data; \
	assert((a)); \
	LocalDir_Entry* (b) = (a)->head; \
	assert((b));

static LocalDir_Entry*
LocalDir_MakeEntry(const char* path);

static int
LocalDir_StrCpy(char dest[_TINYDIR_PATH_MAX],
                const char src[_TINYDIR_PATH_MAX])
{
	char* d_len_p = NULL;

	d_len_p = (char*) memccpy(dest, src, '\0', _TINYDIR_PATH_MAX);
	assert(d_len_p - dest < _TINYDIR_PATH_MAX);

	return d_len_p - dest - 1;
}

static int
LocalDir_Append(char* dest,
                int offset,
                const char* src)
{
	int r = snprintf(dest + offset,
	                 _TINYDIR_PATH_MAX - offset,
	                 "%s%s",
	                 DIRSEP_STR, src);

	assert(r < _TINYDIR_PATH_MAX - offset && r >= 0);

	return r;
}

static int
LocalDir_LoadDir(const Directory* obj,
                 size_t idx)
{
	DataObjects(dir_data, e, obj);

	LocalDir_Entry* new_e;
	char new_path[_TINYDIR_PATH_MAX];
	int np_len = 0;

	if (idx >= e->n_dirs + e->n_files || !e->files[idx].is_dir)
		return -1;

	np_len = LocalDir_StrCpy(new_path, e->path);

	if (strncmp(e->files[idx].name, "..", _TINYDIR_PATH_MAX - 1) == 0) {
		if (e->prev != NULL) {
			// we have entered this directory previously
			dir_data->head = e->prev;
			dir_data->head->next = NULL;
		} else {
			// we need to make a new root entry of the cwd parent
			// TODO: konstig cast
			size_t sep_idx = (int) (strrchr(new_path, DIRSEP) - new_path);
			if (sep_idx >= _TINYDIR_PATH_MAX - 1)
				return -1;
			new_path[sep_idx] = '\0';
			if (strchr((const char*) new_path, DIRSEP) == NULL) {
				new_path[sep_idx] = DIRSEP;
				new_path[sep_idx + 1] = '\0';
			}
			new_e = LocalDir_MakeEntry((const char*) new_path);
			assert(new_e);
			dir_data->root = dir_data->head = new_e;
		}
		free(e->files);
		free(e);
	} else {
		LocalDir_Append(new_path, np_len, e->files[idx].name);
		new_e = LocalDir_MakeEntry((const char*) new_path);
		assert(new_e);
		dir_data->head->subdir_idx = idx;
		new_e->prev = dir_data->head;
		dir_data->head->next = new_e;
		dir_data->head = new_e;
	}

	return 0;
}

static size_t
LocalDir_NDirs(const Directory* obj)
{
	DataObjects(dir_data, e, obj);

	return e->n_dirs;
}

static size_t
LocalDir_NFiles(const Directory* obj)
{
	DataObjects(dir_data, e, obj);

	return e->n_files;
}

static size_t
LocalDir_NTotal(const Directory* obj)
{
	DataObjects(dir_data, e, obj);

	return e->n_dirs + e->n_files;
}

static size_t
LocalDir_SubDirIdx(const Directory* obj)
{
	DataObjects(dir_data, e, obj);

	return e->subdir_idx;
}

static const char*
LocalDir_GetName(const Directory* obj,
                 size_t idx,
                 bool* isdir)
{
	DataObjects(dir_data, e, obj);

	if (idx >= e->n_dirs + e->n_files)
		return NULL;

	if (isdir != NULL)
		*isdir = e->files[idx].is_dir;

	return (const char*) e->files[idx].name;
}

static void
LocalDir_FullPath(const Directory* obj,
                  char path[_TINYDIR_PATH_MAX],
                  size_t idx)
{
	DataObjects(dir_data, e, obj);

	int p_len = LocalDir_StrCpy(path, e->path);

	LocalDir_Append(path, p_len, e->files[idx].name);

	// TODO: make sure this is a file, otherwise blank path or return NULL or something
}

void*
LocalDir_ReadFile(const char* path,
                  size_t* len,
                  size_t max_len)
{
	struct stat st;
	char* data;
	FILE* f;

	if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)
	                          || st.st_size <= 0
	                          || (size_t) st.st_size >= max_len
	                          || (f = fopen(path, "rb")) == NULL)
		return NULL;

	data = (char*) calloc(st.st_size, 1);
	assert(fread(data, 1, st.st_size, f) == (size_t) st.st_size);

	*len = st.st_size;

	if (ferror(f) != 0) {
		free(data);
		data = NULL;
		*len = 0;
	}

	fclose(f);

	return (void*) data;
}

static void*
LocalDir_GetFile(const Directory* obj,
                 size_t idx,
                 size_t* len,
                 size_t max_len)
{
	DataObjects(dir_data, e, obj);

	void* data;
	char f_abspath[_TINYDIR_PATH_MAX];

	if (idx >= e->n_dirs + e->n_files)
		return NULL;

	LocalDir_FullPath(obj, f_abspath, idx);
	data = LocalDir_ReadFile(f_abspath, len, max_len);

	return data;
}

static void
LocalDir_Print(const Directory* obj)
{
	DataObjects(dir_data, e, obj);

	fprintf(stdout, "n dirs: %" PRIu64 ", n files: %" PRIu64 "\n", e->n_dirs,
	            e->n_files);

	for (size_t i = 0; i < e->n_dirs + e->n_files; i++) {
		fprintf(stdout, "[%" PRIu64 "] %s%s\n", i,
		        e->files[i].is_dir ? DIRSEP_STR : " ",
		        e->files[i].name);
	}
}

static void
LocalDir_PrintRecursive(const LocalDir_Entry* e, size_t depth)
{
	for (size_t i = 0; i < e->n_dirs + e->n_files; i++) {
		for (size_t j = 0; j < depth; j++)
			fprintf(stdout, "  ");

		fprintf(stdout, "%s%s\n", e->files[i].is_dir ? DIRSEP_STR : " ",
		        e->files[i].name);

		if (e->subdir_idx > 0 && e->subdir_idx == i && e->next != NULL)
			LocalDir_PrintRecursive(e->next, depth + 1);
	}
}

static void
LocalDir_PrintTree(const Directory* obj)
{
	DataObjects(dir_data, e, obj);

	LocalDir_PrintRecursive(e, 0);
}

static void
LocalDir_Destroy(Directory* obj)
{
	DataObjects(dir_data, e, obj);

	while (e != NULL) {
		LocalDir_Entry* temp = e->prev;
		free(e->files);
		free(e);
		e = temp;
	}

	free(dir_data);
	free(obj);
}

static int
LocalDir_FileCmp(const void* a, const void* b)
{
	const tinydir_file* fa = (const tinydir_file*) a;
	const tinydir_file* fb = (const tinydir_file*) b;

	if (fa->is_dir && !strncmp(fa->name, "..", 2))
		return -1;
	else if (fb->is_dir && !strncmp(fb->name, "..", 2))
		return 1;

	if (fa->is_dir != fb->is_dir)
		return -(fa->is_dir - fb->is_dir);

	return strncasecmp(fa->name, fb->name, _TINYDIR_FILENAME_MAX);
}

static LocalDir_Entry*
LocalDir_MakeEntry(const char* path)
{
	LocalDir_Entry* e;
	tinydir_dir dir;

	e = (LocalDir_Entry*) calloc(1, sizeof(LocalDir_Entry));
	assert(e);

	e->subdir_idx = 0;
	e->next = e->prev = NULL;
	e->files = (tinydir_file*) malloc(sizeof(tinydir_file));
	assert(e->files);

	if (-1 == tinydir_open(&dir, path))
		goto fail;

	while (dir.has_next) {
		tinydir_file f;

		if (-1 == tinydir_readfile(&dir, &f))
			goto fail;

		if (-1 == tinydir_next(&dir))
			goto fail;

		if (strncmp(f.name, ".", _TINYDIR_PATH_MAX - 1) == 0)
			continue;

		if (f.is_dir || f.is_reg)
			e->files[e->n_files + e->n_dirs] = f;

		if (f.is_dir)
			e->n_dirs++;
		else if (f.is_reg)
			e->n_files++;

		e->files = (tinydir_file*) realloc(e->files,
		                                   (1 + e->n_files + e->n_dirs) *
		                                       sizeof(tinydir_file));
		assert(e->files);
	}

	tinydir_close(&dir);

	LocalDir_StrCpy(e->path, path);

	qsort(e->files,
	      e->n_files + e->n_dirs,
	      sizeof(tinydir_file),
	      LocalDir_FileCmp);

	return e;

fail:
	free(e->files);
	free(e);
	tinydir_close(&dir);

	return NULL;
}

Directory*
LocalDir_Create(const char* path)
{
	Directory* dir;
	LocalDir_Data* dir_data;

	static Directory_VTable _vtable;
	static bool _initialized = false;

	char* resolved_path[_TINYDIR_PATH_MAX];

	if (!_initialized) {
		memset((void*) &_vtable, 0, sizeof(Directory_VTable));

		_vtable.LoadDir   = (*LocalDir_LoadDir);
		_vtable.SubDirIdx = (*LocalDir_SubDirIdx);
		_vtable.NDirs     = (*LocalDir_NDirs);
		_vtable.NFiles    = (*LocalDir_NFiles);
		_vtable.NTotal    = (*LocalDir_NTotal);
		_vtable.GetName   = (*LocalDir_GetName);
		_vtable.FullPath  = (*LocalDir_FullPath);
		_vtable.GetFile   = (*LocalDir_GetFile);
		_vtable.Print     = (*LocalDir_Print);
		_vtable.PrintTree = (*LocalDir_PrintTree);
		_vtable.Destroy   = (*LocalDir_Destroy);

		_initialized = true;
	}

	dir =  (Directory*) calloc(1, sizeof(Directory));
	assert(dir);

	dir_data = (LocalDir_Data*) calloc(1, sizeof(LocalDir_Data));
	assert(dir_data);

	assert(strnlen(path, _TINYDIR_PATH_MAX) < _TINYDIR_PATH_MAX);
	assert(realpath(path, (char* restrict) resolved_path));

	dir_data->root = LocalDir_MakeEntry((const char*) resolved_path);
	assert(dir_data->root);

	dir_data->head = dir_data->root;

	dir->vtable = &_vtable;
	dir->data = (void*) dir_data;

	return dir;
}
