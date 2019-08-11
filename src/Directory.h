// Copyright intealls
// License: GPL v3

#ifndef DIRECTORY_H_
#define DIRECTORY_H_

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

typedef struct Directory_VTable Directory_VTable;

typedef struct Directory {
	Directory_VTable* vtable;
	void* data;
} Directory;

struct Directory_VTable {
	int         (*LoadDir)   (const Directory*,
	                          size_t);
	size_t      (*SubDirIdx) (const Directory*);
	size_t      (*NDirs)     (const Directory*);
	size_t      (*NFiles)    (const Directory*);
	size_t      (*NTotal)    (const Directory*);
	const char* (*GetName)   (const Directory*, size_t, bool*);
	void        (*FullPath)  (const Directory*, char*, size_t);
	void*       (*GetFile)   (const Directory*, size_t, size_t*, size_t);
	void        (*Print)     (const Directory*);
	void        (*PrintTree) (const Directory*);
	void        (*Destroy)   (Directory*);
};

static int
Directory_LoadDir(const Directory* obj,
                  size_t idx)
{
	assert(obj);

	return obj->vtable->LoadDir(obj, idx);
}

static size_t
Directory_SubDirIdx(const Directory* obj)
{
	assert(obj);

	return obj->vtable->SubDirIdx(obj);
}

static size_t
Directory_NDirs(const Directory* obj)
{
	assert(obj);

	return obj->vtable->NDirs(obj);
}

static size_t
Directory_NFiles(const Directory* obj)
{
	assert(obj);

	return obj->vtable->NFiles(obj);
}

static size_t
Directory_NTotal(const Directory* obj)
{
	assert(obj);

	return obj->vtable->NTotal(obj);
}

static const char*
Directory_GetName(const Directory* obj,
                  size_t idx,
                  bool* isdir)
{
	assert(obj);

	return obj->vtable->GetName(obj, idx, isdir);
}

static void
Directory_FullPath(const Directory* obj,
                   char* path,
                   size_t idx)
{
	assert(obj);

	obj->vtable->FullPath(obj, path, idx);
}

static void*
Directory_GetFile(const Directory* obj,
                  size_t idx,
                  size_t* len,
                  size_t max_len)
{
	assert(obj);

	return obj->vtable->GetFile(obj, idx, len, max_len);
}

static void
Directory_Print(const Directory* obj)
{
	assert(obj);

	obj->vtable->Print(obj);
}

static void
Directory_PrintTree(const Directory* obj)
{
	assert(obj);

	obj->vtable->PrintTree(obj);
}

static void
Directory_Destroy(Directory* obj)
{
	assert(obj);

	obj->vtable->Destroy(obj);
}

#endif /* DIRECTORY_H_ */
