// two smokes, let's go!
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>

#include "../3rdparty/hvl/hvl_replay.h"
#include "HVLRenderer.h"
#include "Globals.h"

#define HVL_FREQ 48000
#define HIVELY_LEN HVL_FREQ/50

typedef struct HVLRenderer_Data {
	struct hvl_tune* hvl;
	size_t hivelyIndex;
	int16 hivelyLeft[HIVELY_LEN];
	int16 hivelyRight[HIVELY_LEN];
	char title[MODP_STR_LENGTH];
	char info[MODP_STR_LENGTH];
	_Atomic size_t total_frames_rendered;
	int current_track;
	int track_length;
	int fs, bits, channels;
} HVLRenderer_Data;

#define DataObject(a, b) \
	HVLRenderer_Data* (a) = (HVLRenderer_Data*) (b)->data; \
	assert((a)); \
	DebugPrint((b));

static void
HVLRenderer_LogFunc(const char* message, void* userdata)
{
	(void) userdata;

	if (message)
		fprintf(stderr, "HVLRenderer: %s\n", message);
}

static int
HVLRenderer_Load(const AudioRenderer* obj,
                 const char* filename,
                 const void* data,
                 const size_t len)
{
	const char* hvl_str = NULL;
	const char* source = NULL;

	DataObject(rndr_data, obj);

	rndr_data->hvl = hvl_LoadData(data, len, HVL_FREQ, 4);

	if (rndr_data->hvl == NULL)
		return 1;

	hvl_str = rndr_data->hvl->ht_Name;
	source = (hvl_str != NULL && *hvl_str) ? hvl_str : filename;
	assert(memccpy(rndr_data->title, source, '\0', MODP_STR_LENGTH) != NULL);

	return 0;
}

static bool
HVLRenderer_CanLoad(const AudioRenderer* obj,
                    const void* data,
                    const size_t len)
{
	(void) obj;

	if (len > 2 && (!memcmp(data, "THX", 3) || !memcmp(data, "HVL", 3)))
		return true;

	return false;
}

static bool
HVLRenderer_Loaded(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->hvl != NULL)
		return true;

	return false;
}

static void
HVLRenderer_UnLoad(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->hvl != NULL)
		hvl_FreeTune(rndr_data->hvl);

	rndr_data->hvl = NULL;
	rndr_data->title[0] = '\0';
	rndr_data->total_frames_rendered = 0;
	rndr_data->hivelyIndex = 0;
	rndr_data->track_length = -1;
}

static int
HVLRenderer_Render(const AudioRenderer* obj,
                   void* buf,
                   const size_t len)
{
	DataObject(rndr_data, obj);
	/* -- 8< -- 8< --
	size_t byte_scale = (rndr_data->bits / 8) * rndr_data->channels;
	size_t rendered = 0;
	size_t to_render = len / byte_scale;
	*/
    int16 *out;
    int i;
	int length;
	size_t streamPos = 0;
	length = len >> 1;
	out = (int16*) buf;

	if (rndr_data->hvl) {
		// Mix to 16bit interleaved stereo
		out = (int16*) buf;

		// Flush remains of previous frame
		for (i = rndr_data->hivelyIndex; i < (HIVELY_LEN); i++) {
			out[streamPos++] = rndr_data->hivelyLeft[i];
			out[streamPos++] = rndr_data->hivelyRight[i];
		}

		while (streamPos < length) {
			hvl_DecodeFrame(rndr_data->hvl,
			                (int8*) rndr_data->hivelyLeft,
			                (int8*) rndr_data->hivelyRight,
			                2);

			if (rndr_data->hvl->ht_SongEndReached && rndr_data->track_length == -1)
				rndr_data->track_length = rndr_data->total_frames_rendered;

			for (i = 0; i < (HIVELY_LEN) && streamPos < length; i++) {
				out[streamPos++] = rndr_data->hivelyLeft[i];
				out[streamPos++] = rndr_data->hivelyRight[i];
			}
		}
		rndr_data->hivelyIndex = i;
	}

	rndr_data->total_frames_rendered += streamPos / 2; // not sure this is correct

	return streamPos;
}

static const char*
HVLRenderer_Title(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->hvl != NULL)
		return rndr_data->title;
	else
		return NULL;
}

static const char*
HVLRenderer_Info(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->hvl != NULL)
		return rndr_data->info;
	else
		return NULL;
}

static int
HVLRenderer_PlayTime(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	return rndr_data->total_frames_rendered / rndr_data->fs;
}

static int
HVLRenderer_Length(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->hvl) {
		if (rndr_data->track_length < 0)
			return (int) rndr_data->total_frames_rendered / rndr_data->fs + 1;
		else
			return (int) rndr_data->track_length / rndr_data->fs;
	}

	return 0;
}

static void
HVLRenderer_Destroy(AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	HVLRenderer_UnLoad((const AudioRenderer*) obj);

	free(rndr_data);
	free(obj);
}

static int
HVLRenderer_Track(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->hvl != NULL)
		return 0;

	return -1;
}

static int
HVLRenderer_NTracks(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->hvl != NULL)
		return 1;

	return -1;
}

static int
HVLRenderer_SetTrack(const AudioRenderer* obj, int track)
{
	assert(obj);

	(void) track;

	return HVLRenderer_Track(obj);
}

AudioRenderer*
HVLRenderer_Create(int fs, int bits, int channels)
{
	AudioRenderer* arndr;
	HVLRenderer_Data* rndr_data;
	hvl_InitReplayer();
	static AudioRenderer_VTable _vtable;
	static bool _initialized = false;

	if (!_initialized) {
		memset((void*) &_vtable, 0, sizeof(AudioRenderer_VTable));

		_vtable.Load     = (*HVLRenderer_Load);
		_vtable.CanLoad  = (*HVLRenderer_CanLoad);
		_vtable.Loaded   = (*HVLRenderer_Loaded);
		_vtable.UnLoad   = (*HVLRenderer_UnLoad);
		_vtable.Render   = (*HVLRenderer_Render);
		_vtable.Title    = (*HVLRenderer_Title);
		_vtable.Info     = (*HVLRenderer_Info);
		_vtable.Track    = (*HVLRenderer_Track);
		_vtable.NTracks  = (*HVLRenderer_NTracks);
		_vtable.SetTrack = (*HVLRenderer_SetTrack);
		_vtable.PlayTime = (*HVLRenderer_PlayTime);
		_vtable.Length   = (*HVLRenderer_Length);
		_vtable.Destroy  = (*HVLRenderer_Destroy);

		_initialized = true;
	}

	arndr = (AudioRenderer*) calloc(1, sizeof(AudioRenderer));
	assert(arndr);

	rndr_data = (HVLRenderer_Data*) calloc(1, sizeof(HVLRenderer_Data));
	assert(rndr_data);

	rndr_data->fs = fs;
	rndr_data->bits = bits;
	rndr_data->channels = channels;

	rndr_data->current_track = -1;
	rndr_data->track_length = -1;

	arndr->vtable = &_vtable;
	arndr->data = (void*) rndr_data;

	return arndr;
}
