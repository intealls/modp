// Copyright intealls
// License: GPL v3

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdint.h>

#include <xmp.h>

#include "XMPRenderer.h"
#include "Globals.h"

typedef struct XMPRenderer_Data {
	char title[MODP_STR_LENGTH];
	char info[MODP_STR_LENGTH];
	xmp_context ctx;
	struct xmp_module_info mod;
	size_t song_length;
	_Atomic size_t total_frames_rendered;
	int fs, bits, channels;
} XMPRenderer_Data;

#define DataObject(a, b) \
	XMPRenderer_Data* (a) = (XMPRenderer_Data*) (b)->data; \
	assert((a)); \
	DebugPrint((b));

static int
XMPRenderer_Load(const AudioRenderer* obj,
                 const char* filename,
                 const void* data,
                 const size_t len)
{
	char* info_ptr = NULL;
	size_t info_copied = 0;

	(void) filename;

	DataObject(rndr_data, obj);

	if (xmp_load_module_from_memory(rndr_data->ctx, data, len))
		return 1;

	info_ptr = rndr_data->info;

	xmp_get_module_info(rndr_data->ctx, &rndr_data->mod);

	assert(memccpy(rndr_data->title,
	               rndr_data->mod.mod->name, '\0', MODP_STR_LENGTH));

	for (int i = 0; i < rndr_data->mod.mod->ins; i++) {
		char* ins_name = rndr_data->mod.mod->xxi[i].name;
		int ins_len = strlen(ins_name);

		if (!ins_len)
			continue;

		if (info_copied + ins_len + 1 >= MODP_STR_LENGTH)
			break;

		assert(memccpy(info_ptr + info_copied,
		               ins_name, '\0', MODP_STR_LENGTH - info_copied));

		info_copied += ins_len + 1;

		info_ptr[info_copied - 1] = '\n';
		info_ptr[info_copied] = '\0';
	}

	rndr_data->song_length = rndr_data->mod.seq_data[0].duration / 1000;

	xmp_start_player(rndr_data->ctx, rndr_data->fs, 0);

	return 0;
}

static bool
XMPRenderer_CanLoad(const AudioRenderer* obj,
                    const void* data,
                    const size_t len)
{
	struct xmp_test_info test_info;

	DataObject(rndr_data, obj);

	return xmp_test_module_from_memory(data, len, &test_info) == 0;
}

static bool
XMPRenderer_Loaded(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (xmp_get_player(rndr_data->ctx, XMP_STATE_LOADED))
		return true;

	return false;
}

static void
XMPRenderer_UnLoad(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (xmp_get_player(rndr_data->ctx, XMP_STATE_LOADED)) {
		xmp_stop_module(rndr_data->ctx);
		xmp_release_module(rndr_data->ctx);
		rndr_data->title[0] = '\0';
		rndr_data->info[0] = '\0';
		rndr_data->song_length = 0;
		rndr_data->total_frames_rendered = 0;
	}
}

static int
XMPRenderer_Render(const AudioRenderer* obj,
                   void* buf,
                   const size_t len)
{
	DataObject(rndr_data, obj);

	int16_t* rndr_buf = (int16_t*) buf;

	size_t byte_scale = (rndr_data->bits / 8) * rndr_data->channels;
	size_t to_render = len / byte_scale;

	xmp_play_buffer(rndr_data->ctx, rndr_buf, len, 0);

	rndr_data->total_frames_rendered += to_render;

	return to_render;
}

static const char*
XMPRenderer_Title(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (xmp_get_player(rndr_data->ctx, XMP_STATE_LOADED))
		return rndr_data->title;

	return NULL;
}

static const char*
XMPRenderer_Info(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (xmp_get_player(rndr_data->ctx, XMP_STATE_LOADED))
		return rndr_data->info;

	return NULL;
}

static int
XMPRenderer_PlayTime(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	return rndr_data->total_frames_rendered / rndr_data->fs;
}

static int
XMPRenderer_Length(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (xmp_get_player(rndr_data->ctx, XMP_STATE_LOADED))
		return rndr_data->song_length;

	return 0;
}

static void
XMPRenderer_Destroy(AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	XMPRenderer_UnLoad((const AudioRenderer*) obj);

	xmp_free_context(rndr_data->ctx);

	free(rndr_data);
	free(obj);
}

static int
XMPRenderer_Track(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (xmp_get_player(rndr_data->ctx, XMP_STATE_LOADED))
		return 1;

	return -1;
}

static int
XMPRenderer_NTracks(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (xmp_get_player(rndr_data->ctx, XMP_STATE_LOADED))
		return 1;

	return -1;
}

static int
XMPRenderer_SetTrack(const AudioRenderer* obj, int track)
{
	assert(obj);

	(void) track;

	return XMPRenderer_Track(obj);
}

AudioRenderer*
XMPRenderer_Create(int fs, int bits, int channels)
{
	AudioRenderer* arndr;
	XMPRenderer_Data* rndr_data;

	static AudioRenderer_VTable _vtable;
	static bool _initialized = false;

	if (!_initialized) {
		memset((void*) &_vtable, 0, sizeof(AudioRenderer_VTable));

		_vtable.Load     = (*XMPRenderer_Load);
		_vtable.CanLoad  = (*XMPRenderer_CanLoad);
		_vtable.Loaded   = (*XMPRenderer_Loaded);
		_vtable.UnLoad   = (*XMPRenderer_UnLoad);
		_vtable.Render   = (*XMPRenderer_Render);
		_vtable.Title    = (*XMPRenderer_Title);
		_vtable.Info     = (*XMPRenderer_Info);
		_vtable.Track    = (*XMPRenderer_Track);
		_vtable.NTracks  = (*XMPRenderer_NTracks);
		_vtable.SetTrack = (*XMPRenderer_SetTrack);
		_vtable.PlayTime = (*XMPRenderer_PlayTime);
		_vtable.Length   = (*XMPRenderer_Length);
		_vtable.Destroy  = (*XMPRenderer_Destroy);

		_initialized = true;
	}

	arndr = (AudioRenderer*) calloc(1, sizeof(AudioRenderer));
	assert(arndr);

	rndr_data = (XMPRenderer_Data*) calloc(1, sizeof(XMPRenderer_Data));
	assert(rndr_data);

	rndr_data->fs = fs;
	rndr_data->bits = bits;
	rndr_data->channels = channels;

	arndr->vtable = &_vtable;
	arndr->data = (void*) rndr_data;

	rndr_data->ctx = xmp_create_context();
	assert(rndr_data->ctx);

	xmp_set_player(rndr_data->ctx, XMP_PLAYER_DEFPAN, 50);
	xmp_set_player(rndr_data->ctx, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);
	xmp_set_player(rndr_data->ctx, XMP_PLAYER_DSP, XMP_DSP_LOWPASS);

	return arndr;
}
