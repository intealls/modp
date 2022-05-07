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

struct hvl_tune *tune;
size_t hivelyIndex;
int16 hivelyLeft[HIVELY_LEN], hivelyRight[HIVELY_LEN];

typedef struct HVLRenderer_Data {
    struct hvl_tune* hvl;
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
	const char* hvl_str;
	const char* source;

	DataObject(rndr_data, obj);
    // INIIIIIIIIIT!!!!!!!

	rndr_data = hvl_LoadData(data, len, HVL_FREQ, 4);

	if (rndr_data == NULL)
		return 1;

	hvl_str = rndr_data->hvl->ht_Name;
	source = (hvl_str != NULL && *hvl_str) ? hvl_str : filename;
	assert(memccpy(rndr_data->hvl->ht_Name, source, '\0', MODP_STR_LENGTH) != NULL);
	return 0;
}

static bool
HVLRenderer_CanLoad(const AudioRenderer* obj,
                        const unsigned char* data,
                        const size_t len)
{
    // duplicate the header check from hvl_replay because I can't be bothered
	int r = 1;
    if ((memcmp(data, "THX", 3)) == 0) {
        if (data[3] > 2) {
            return r;
        }
	r = 0;
    }

    if ((memcmp(data, "HVL", 3)) == 0) {
        if (data[3] > 1) {
            return r;
        }
	r = 0;
    }
	return r;
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
	rndr_data->current_track = rndr_data->track_length = -1;
}

static int
HVLRenderer_Render(const AudioRenderer* obj,
                    void* buf,
                    const size_t len)
{
	DataObject(rndr_data, obj);
	size_t byte_scale = (rndr_data->bits / 8) * rndr_data->channels;
	size_t rendered = 0;
	size_t to_render = len / byte_scale;

	while (rendered < to_render) {
		hvl_DecodeFrame(rndr_data->hvl, (int8 *) hivelyLeft, (int8 *) hivelyRight, 2 );
		rendered++;
	}

	assert(((int) rendered - to_render) == 0);

	rndr_data->total_frames_rendered += rendered;

	return rendered;
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

	if (rndr_data->hvl)
		return (int) rndr_data->hvl->ht_PlayingTime;
	else
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

	rndr_data->hvl = NULL;
	rndr_data->current_track = -1;
	rndr_data->track_length = -1;

	arndr->vtable = &_vtable;
	arndr->data = (void*) rndr_data;

	return arndr;
}