// Copyright intealls
// License: GPL v3
#ifdef HAVE_OPENMPT
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>

#include <iconv.h>

#include <libopenmpt/libopenmpt.h>

#include "OpenMPTRenderer.h"
#include "Globals.h"

#define openmpt_probe    openmpt_probe_file_header
#define openmpt_load     openmpt_module_create_from_memory2
#define openmpt_read     openmpt_module_read_interleaved_stereo

typedef struct OpenMPTRenderer_Data {
	char title[MODP_STR_LENGTH];
	char info[MODP_STR_LENGTH];
	openmpt_module* mod;
	_Atomic size_t total_frames_rendered;
	int fs, bits, channels;
} OpenMPTRenderer_Data;

#define DataObject(a, b) \
	OpenMPTRenderer_Data* (a) = (OpenMPTRenderer_Data*) (b)->data; \
	assert((a)); \
	DebugPrint((b));

static size_t
OpenMPTRenderer_UTF8ToISO8859_1(char str[MODP_STR_LENGTH])
{
	size_t r = 0;

	char tmp_str[MODP_STR_LENGTH];

	char* tmp_str_p = tmp_str;
	char* dst_str_p = str;

	size_t inbytes = MODP_STR_LENGTH;
	size_t outbytes = MODP_STR_LENGTH;

	iconv_t cd = iconv_open("ISO-8859-1", "UTF-8");

	memcpy(tmp_str, str, MODP_STR_LENGTH);

	r = iconv(cd, &tmp_str_p, &inbytes, &dst_str_p, &outbytes);

	iconv_close(cd);

	return r;
}

static void
OpenMPTRenderer_LogFunc(const char* message, void* userdata)
{
	(void) userdata;

	if (message)
		fprintf(stderr, "OpenMPTRenderer: %s\n", message);
}

static int
OpenMPTRenderer_Load(const AudioRenderer* obj,
                     const char* filename,
                     const void* data,
                     const size_t len)
{
	const char* openmpt_str;
	const char* source;

	DataObject(rndr_data, obj);

	openmpt_module_initial_ctl ctl[2] = {
		{ .ctl = "play.at_end", .value = "continue" },
		{ .ctl = 0, .value = 0 }
	};

	// TODO: maybe some error handling, aside from open fail
	rndr_data->mod = openmpt_load(data, len,
	                              OpenMPTRenderer_LogFunc,
	                              NULL, NULL, NULL, NULL, NULL,
	                              ctl);

	if (rndr_data->mod == NULL)
		return 1;

	openmpt_str = openmpt_module_get_metadata(rndr_data->mod, "title");

	source = (openmpt_str != NULL && *openmpt_str) ? openmpt_str : filename;

	assert(memccpy(rndr_data->title, source, '\0', MODP_STR_LENGTH) != NULL);

	if (openmpt_str != NULL && *openmpt_str)
		OpenMPTRenderer_UTF8ToISO8859_1(rndr_data->title);

	openmpt_free_string(openmpt_str);

	openmpt_str = openmpt_module_get_metadata(rndr_data->mod, "message");

	if (openmpt_str != NULL && *openmpt_str) {
		void* p = memccpy(rndr_data->info, openmpt_str,
		                  '\0', MODP_STR_LENGTH);

		if (p == NULL)
			rndr_data->info[MODP_STR_LENGTH - 1] = '\0';

		OpenMPTRenderer_UTF8ToISO8859_1(rndr_data->info);
	}

	openmpt_free_string(openmpt_str);

	return 0;
}

static bool
OpenMPTRenderer_CanLoad(const AudioRenderer* obj,
                        const void* data,
                        const size_t len)
{
	int r = 1;

	DataObject(rndr_data, obj);

	size_t headerlen = openmpt_probe_file_header_get_recommended_size();

	r = openmpt_probe(OPENMPT_PROBE_FILE_HEADER_FLAGS_DEFAULT,
	                  data, headerlen, len, OpenMPTRenderer_LogFunc,
	                  NULL, NULL, NULL, NULL, NULL);

	return r == OPENMPT_PROBE_FILE_HEADER_RESULT_SUCCESS;
}

static bool
OpenMPTRenderer_Loaded(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->mod != NULL)
		return true;

	return false;
}

static void
OpenMPTRenderer_UnLoad(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->mod != NULL) {
		openmpt_module_destroy(rndr_data->mod);
		rndr_data->mod = NULL;
		rndr_data->title[0] = '\0';
		rndr_data->info[0] = '\0';
		rndr_data->total_frames_rendered = 0;
	}
}

static int
OpenMPTRenderer_Render(const AudioRenderer* obj,
                       void* buf,
                       const size_t len)
{
	DataObject(rndr_data, obj);

	int16_t* rndr_buf = (int16_t*) buf;

	size_t byte_scale = (rndr_data->bits / 8) * rndr_data->channels;
	size_t rendered = 0; // frames
	size_t to_render = len / byte_scale;

	// TODO: if too many frames are rendered, these should be stored
	// and retrieved at next call (can this happen?)
	while (rendered < to_render) {
		rendered += openmpt_read(rndr_data->mod,
		                         rndr_data->fs,
		                         to_render - rendered,
		                         rndr_buf + rendered * rndr_data->channels);
	}

	assert(((int) rendered - to_render) == 0);

	rndr_data->total_frames_rendered += rendered;

	return rendered;
}

static const char*
OpenMPTRenderer_Title(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->mod != NULL)
		return rndr_data->title;
	else
		return NULL;
}

static const char*
OpenMPTRenderer_Info(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->mod != NULL)
		return rndr_data->info;
	else
		return NULL;
}

static int
OpenMPTRenderer_PlayTime(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	return rndr_data->total_frames_rendered / rndr_data->fs;
}

static int
OpenMPTRenderer_Length(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->mod)
		return (int) openmpt_module_get_duration_seconds(rndr_data->mod);
	else
		return 0;
}

static void
OpenMPTRenderer_Destroy(AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	OpenMPTRenderer_UnLoad((const AudioRenderer*) obj);

	free(rndr_data);
	free(obj);
}

static int
OpenMPTRenderer_Track(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->mod != NULL)
		return 0;

	return -1;
}

static int
OpenMPTRenderer_NTracks(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->mod != NULL)
		return 1;

	return -1;
}

static int
OpenMPTRenderer_SetTrack(const AudioRenderer* obj, int track)
{
	assert(obj);

	(void) track;

	return OpenMPTRenderer_Track(obj);
}

AudioRenderer*
OpenMPTRenderer_Create(int fs, int bits, int channels)
{
	AudioRenderer* arndr;
	OpenMPTRenderer_Data* rndr_data;

	static AudioRenderer_VTable _vtable;
	static bool _initialized = false;

	if (!_initialized) {
		memset((void*) &_vtable, 0, sizeof(AudioRenderer_VTable));

		_vtable.Load     = (*OpenMPTRenderer_Load);
		_vtable.CanLoad  = (*OpenMPTRenderer_CanLoad);
		_vtable.Loaded   = (*OpenMPTRenderer_Loaded);
		_vtable.UnLoad   = (*OpenMPTRenderer_UnLoad);
		_vtable.Render   = (*OpenMPTRenderer_Render);
		_vtable.Title    = (*OpenMPTRenderer_Title);
		_vtable.Info     = (*OpenMPTRenderer_Info);
		_vtable.Track    = (*OpenMPTRenderer_Track);
		_vtable.NTracks  = (*OpenMPTRenderer_NTracks);
		_vtable.SetTrack = (*OpenMPTRenderer_SetTrack);
		_vtable.PlayTime = (*OpenMPTRenderer_PlayTime);
		_vtable.Length   = (*OpenMPTRenderer_Length);
		_vtable.Destroy  = (*OpenMPTRenderer_Destroy);

		_initialized = true;
	}

	arndr = (AudioRenderer*) calloc(1, sizeof(AudioRenderer));
	assert(arndr);

	rndr_data = (OpenMPTRenderer_Data*) calloc(1, sizeof(OpenMPTRenderer_Data));
	assert(rndr_data);

	rndr_data->fs = fs;
	rndr_data->bits = bits;
	rndr_data->channels = channels;

	arndr->vtable = &_vtable;
	arndr->data = (void*) rndr_data;

	return arndr;
}
#endif
