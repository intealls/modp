// Copyright intealls
// License: GPL v3

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdint.h>

#include "../3rdparty/libsidplayfp/libsidplayfp_wrap.h"
#include "SIDRenderer.h"
#include "Globals.h"
#include "Utils.h"

typedef struct SIDRenderer_Data {
	struct ReSIDfpBuilder* resid_builder;
	struct SidTune* sid_tune;
	struct sidplayfp* sid_engine;
	char title[MODP_STR_LENGTH];
	char info[MODP_STR_LENGTH];
	_Atomic size_t total_frames_rendered;
	unsigned int songs;
	int current_track;
	int track_length;
	int fs, bits, channels;
} SIDRenderer_Data;

#define DataObject(a, b) \
	SIDRenderer_Data* (a) = (SIDRenderer_Data*) (b)->data; \
	assert((a)); \
	DebugPrint((b));

static void SIDRenderer_UnLoad(const AudioRenderer*);
static int  SIDRenderer_SetTrack(const AudioRenderer*, int);
void SIDRenderer_DeleteInterfaces(const AudioRenderer*);

static void
SIDRenderer_LogFunc(const char* message)
{
	if (message)
		fprintf(stderr, "SIDRenderer: %s\n", message);
}

static int
SIDRenderer_Load(const AudioRenderer* obj,
                 const char* filename,
                 const void* data,
                 const size_t len)
{
	DataObject(rndr_data, obj);
	rndr_data->resid_builder = newReSIDfpBuilder();
	rndr_data->sid_engine = newSidEngine();
	rndr_data->sid_tune = newSidTune(data, len);

	if (!initSidEngine(rndr_data->sid_engine, rndr_data->resid_builder, rndr_data->channels, rndr_data->fs)) {
		SIDRenderer_LogFunc("error initializing libsidplayfp engine");
		SIDRenderer_DeleteInterfaces(obj);
		return 1;
	}

	if (!loadSidTune(rndr_data->sid_tune, rndr_data->sid_engine)) {
		SIDRenderer_LogFunc("failed to load sid tune");
		SIDRenderer_DeleteInterfaces(obj);
		return 1;
	}

	rndr_data->songs = songsSidTune(rndr_data->sid_tune);
	StrCpy(rndr_data->title, MODP_STR_LENGTH, filename);
	return SIDRenderer_SetTrack(obj, 0);
}

void SIDRenderer_DeleteInterfaces(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);
	deleteReSIDfpBuilder(rndr_data->resid_builder);
	deleteSideEngine(rndr_data->sid_engine);
	deleteSidTune(rndr_data->sid_tune);
	rndr_data->resid_builder = NULL;
	rndr_data->sid_engine = NULL;
	rndr_data->sid_tune = NULL;
}

static bool
SIDRenderer_CanLoad(const AudioRenderer* obj,
                    const void* data,
                    const size_t len)
{
	(void) obj;
	// check for 4B sid magic id in file header
	if (len > 4 && (!memcmp(data, "PSID", 4) || !memcmp(data, "RSID", 4))) {
		return true;
	}
	return false;
}

static bool
SIDRenderer_Loaded(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->sid_tune != NULL) {
		return true;
	}
	return false;
}

static void
SIDRenderer_UnLoad(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);
	SIDRenderer_DeleteInterfaces(obj);
	rndr_data->title[0] = '\0';
	rndr_data->current_track = -1;
	rndr_data->track_length = -1;
	rndr_data->total_frames_rendered = 0;
	rndr_data->songs = 0;
}

static int
SIDRenderer_Render(const AudioRenderer* obj,
                   void* buf,
                   const size_t len)
{
	DataObject(rndr_data, obj);
	int16_t* rndr_buf = (int16_t*) buf;
	size_t rendered = 0;
	size_t to_render = rndr_data->fs / rndr_data->channels;
		
	while (rendered < to_render) {
		rendered += playSidEngine(rndr_data->sid_engine, rndr_buf, to_render);
	}

	assert(((int) rendered - to_render) == 0);
	rndr_data->total_frames_rendered += rendered / rndr_data->channels;

	return rendered;
}

static const char*
SIDRenderer_Title(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->sid_tune != NULL)
		return rndr_data->title;
	else
		return NULL;
}

static const char*
SIDRenderer_Info(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->sid_tune != NULL)
		return rndr_data->info;
	else
		return NULL;
}

static int
SIDRenderer_Track(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);
	return rndr_data->current_track;
}

static int
SIDRenderer_NTracks(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->sid_tune != NULL)
		return songsSidTune(rndr_data->sid_tune);

	return -1;
}

static int
SIDRenderer_AppendInfo(char* dest,
                       int offset,
                       const char* src,
                       const char* desc)
{
	int n = snprintf(dest + offset,
	                 MODP_STR_LENGTH - offset,
	                 "%s%s%s",
	                 *src ? desc : "",
	                 *src ? src : "",
	                 *src ? "\n" : "");

	assert(n < MODP_STR_LENGTH - offset && n >= 0);

	return n;
}

static int
SIDRenderer_SetTrack(const AudioRenderer* obj, int track)
{
	DataObject(rndr_data, obj);
	if (rndr_data->sid_tune != NULL) {
		char* info_prefix = NULL;
		size_t n = 0;
		size_t infostr_len;
		rndr_data->current_track = track;

		if (!selectSongSidTune(rndr_data->sid_tune, track)) {
			return -1;
		}

		if (!loadSidTune(rndr_data->sid_tune, rndr_data->sid_engine)) {
			SIDRenderer_LogFunc("failed to load song");
			return -1;
		}

		infostr_len = numberOfInfoStringsSidTune(rndr_data->sid_tune) - 1;

		for (size_t i = 0; i <= infostr_len; i++) {
			const char* info_str = infoStringSidTune(rndr_data->sid_tune, i);
			switch (i) {
				case 0: info_prefix = "Title: ";
						StrCpy(rndr_data->title, MODP_STR_LENGTH, info_str);
						break;
				case 1: info_prefix = "Author: ";
						break;
				case 2: info_prefix = "Released: ";
						break;
			}
			n += SIDRenderer_AppendInfo(rndr_data->info, n, info_str, info_prefix);
		}
		return SIDRenderer_Track(obj);
	}

	return -1;
}

static int
SIDRenderer_PlayTime(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);
	return rndr_data->total_frames_rendered / rndr_data->fs;
}

static int
SIDRenderer_Length(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);
	/* todo:  sids do not set track length, 
	* hvsc provides a db: https://www.hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.txt
	* perhaps some libcurl fetch, store and use
	*/
	if (rndr_data->sid_engine) {
		if (rndr_data->track_length < 0)
			return (int) rndr_data->total_frames_rendered / rndr_data->fs + 1;
		else
			return (int) rndr_data->track_length / rndr_data->fs;
	}

	return 0;
}

static void
SIDRenderer_Destroy(AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	SIDRenderer_UnLoad((const AudioRenderer*) obj);

	free(rndr_data);
	free(obj);
}

AudioRenderer*
SIDRenderer_Create(int fs, int bits, int channels)
{
	AudioRenderer* arndr;
	SIDRenderer_Data* rndr_data;

	static AudioRenderer_VTable _vtable;
	static bool _initialized = false;

	if (!_initialized) {
		memset((void*) &_vtable, 0, sizeof(AudioRenderer_VTable));

		_vtable.Load     = (*SIDRenderer_Load);
		_vtable.CanLoad  = (*SIDRenderer_CanLoad);
		_vtable.Loaded   = (*SIDRenderer_Loaded);
		_vtable.UnLoad   = (*SIDRenderer_UnLoad);
		_vtable.Render   = (*SIDRenderer_Render);
		_vtable.Title    = (*SIDRenderer_Title);
		_vtable.Info     = (*SIDRenderer_Info);
		_vtable.Track    = (*SIDRenderer_Track);
		_vtable.NTracks  = (*SIDRenderer_NTracks);
		_vtable.SetTrack = (*SIDRenderer_SetTrack);
		_vtable.PlayTime = (*SIDRenderer_PlayTime);
		_vtable.Length   = (*SIDRenderer_Length);
		_vtable.Destroy  = (*SIDRenderer_Destroy);

		_initialized = true;
	}

	arndr = (AudioRenderer*) calloc(1, sizeof(AudioRenderer));
	assert(arndr);

	rndr_data = (SIDRenderer_Data*) calloc(1, sizeof(SIDRenderer_Data));
	assert(rndr_data);

	rndr_data->fs = fs;
	rndr_data->bits = bits;
	rndr_data->channels = channels;
	rndr_data->resid_builder = NULL;
	rndr_data->sid_tune = NULL;
	rndr_data->sid_engine = NULL;

	rndr_data->current_track = -1;
	rndr_data->track_length = -1;

	arndr->vtable = &_vtable;
	arndr->data = (void*) rndr_data;

	return arndr;
}
