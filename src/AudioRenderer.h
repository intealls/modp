// Copyright intealls
// License: GPL v3

#ifndef AUDIORENDERER_H_
#define AUDIORENDERER_H_

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

typedef struct AudioRenderer AudioRenderer;
typedef struct AudioRenderer_VTable AudioRenderer_VTable;

struct AudioRenderer {
	AudioRenderer_VTable* vtable;
	void* data;
};

struct AudioRenderer_VTable {
	int         (*Load)     (const AudioRenderer*,
	                         const char*,
	                         const void*,
	                         const size_t);
	bool        (*CanLoad)  (const AudioRenderer*,
	                         const void*,
	                         const size_t);
	bool        (*Loaded)   (const AudioRenderer*);
	void        (*UnLoad)   (const AudioRenderer*);
	int         (*Render)   (const AudioRenderer*,
	                         void*,
	                         const size_t);
	const char* (*Title)    (const AudioRenderer*);
	const char* (*Info)     (const AudioRenderer*);
	int         (*Track)    (const AudioRenderer*);
	int         (*NTracks)  (const AudioRenderer*);
	int         (*SetTrack) (const AudioRenderer*, int);
	int         (*PlayTime) (const AudioRenderer*);
	int         (*Length)   (const AudioRenderer*);
	void        (*Destroy)  (AudioRenderer*);
};

static int
AudioRenderer_Load(const AudioRenderer* obj,
                   const char* filename,
                   const void* data,
                   const size_t len)
{
	assert(obj);

	return obj->vtable->Load(obj, filename, data, len);
}

static bool
AudioRenderer_CanLoad(const AudioRenderer* obj,
                      const void* data,
                      const size_t len)
{
	assert(obj);

	return obj->vtable->CanLoad(obj, data, len);
}

static bool
AudioRenderer_Loaded(const AudioRenderer* obj)
{
	assert(obj);

	return obj->vtable->Loaded(obj);
}

static void
AudioRenderer_UnLoad(const AudioRenderer* obj)
{
	assert(obj);

	obj->vtable->UnLoad(obj);
}

static int
AudioRenderer_Render(const AudioRenderer* obj,
                     void* buf,
                     const size_t len)
{
	assert(obj);

	return obj->vtable->Render(obj, buf, len);
}

static const char*
AudioRenderer_Title(const AudioRenderer* obj)
{
	assert(obj);

	return obj->vtable->Title(obj);
}

static const char*
AudioRenderer_Info(const AudioRenderer* obj)
{
	assert(obj);

	return obj->vtable->Info(obj);
}

static int
AudioRenderer_Track(const AudioRenderer* obj)
{
	assert(obj);

	return obj->vtable->Track(obj);
}

static int
AudioRenderer_NTracks(const AudioRenderer* obj)
{
	assert(obj);

	return obj->vtable->NTracks(obj);
}

static int
AudioRenderer_SetTrack(const AudioRenderer* obj, int track)
{
	assert(obj);

	return obj->vtable->SetTrack(obj, track);
}

static int
AudioRenderer_PlayTime(const AudioRenderer* obj)
{
	assert(obj);

	return obj->vtable->PlayTime(obj);
}

static int
AudioRenderer_Length(const AudioRenderer* obj)
{
	assert(obj);

	return obj->vtable->Length(obj);
}

static void
AudioRenderer_Destroy(AudioRenderer* obj)
{
	assert(obj);

	obj->vtable->Destroy(obj);
}

#endif
