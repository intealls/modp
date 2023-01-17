// Copyright intealls
// License: GPL v3

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>

#include <gme/gme.h>
#include <tinydir.h>

#include "HCS64File.h"
#include "GMERenderer.h"
#include "Globals.h"
#include "Utils.h"

#define GME_TRACK_LENGTH 90000

typedef struct GMERenderer_Data {
	Music_Emu* emu;
	gme_err_t err;
	char title[MODP_STR_LENGTH];
	char info[MODP_STR_LENGTH];
	int current_track;
	int track_length;
	int fs, bits, channels;
} GMERenderer_Data;

#define DataObject(a, b) \
	GMERenderer_Data* (a) = (GMERenderer_Data*) (b)->data; \
	assert((a)); \
	DebugPrint((b));

static void GMERenderer_UnLoad(const AudioRenderer*);
static int  GMERenderer_SetTrack(const AudioRenderer*, int);

static int
GMERenderer_Load(const AudioRenderer* obj,
                 const char* filename,
                 const void* data,
                 const size_t len)
{
	DataObject(rndr_data, obj);

	rndr_data->err = gme_open_data(data, len, &rndr_data->emu, rndr_data->fs);

	if (rndr_data->err != NULL) {
		char* song;
		size_t song_len;

		char* m3u;
		size_t m3u_len = 0;

		gme_delete(rndr_data->emu);

		if (TryOpenHCS64(data, len, &song, &song_len, &m3u, &m3u_len)) {
load_song:
			rndr_data->err = gme_open_data(song,
			                               song_len,
			                               &rndr_data->emu,
			                               rndr_data->fs);

			if (rndr_data->err != NULL) {
				fprintf(stderr, "%s\n", rndr_data->err);
			} else if (m3u_len > 0) {
				rndr_data->err = gme_load_m3u_data(rndr_data->emu,
				                                   m3u,
				                                   m3u_len);

				m3u_len = 0;
				free(m3u);

				if (rndr_data->err != NULL) {
					fprintf(stderr, "%s\n", rndr_data->err);
					gme_delete(rndr_data->emu);
					goto load_song;
				}
			}

			free(song);
		}
	}

	if (rndr_data->err == NULL)
		StrCpy(rndr_data->title, MODP_STR_LENGTH, filename);

	return GMERenderer_SetTrack(obj, 0);
}

static bool
GMERenderer_CanLoad(const AudioRenderer* obj,
                    const void* data,
                    const size_t len)
{
	DataObject(rndr_data, obj);

	Music_Emu* tmp;

	char* song = NULL;
	size_t song_len;

	char* m3u = NULL;
	size_t m3u_len;

	gme_err_t err;

	err = gme_open_data(data, len, &tmp, rndr_data->fs);

	gme_delete(tmp);

	if (err == NULL)
		return true;

	if (TryOpenHCS64(data, len, &song, &song_len, &m3u, &m3u_len)) {
		err = gme_open_data(song, song_len, &tmp, rndr_data->fs);

		if (song != NULL)
			free(song);
		if (m3u != NULL)
			free(m3u);

		gme_delete(tmp);
	}

	if (err == NULL)
		return true;

	return false;
}

static bool
GMERenderer_Loaded(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->emu != NULL)
		return true;

	return false;
}

static void
GMERenderer_UnLoad(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->emu != NULL)
		gme_delete(rndr_data->emu);

	rndr_data->emu = NULL;
	rndr_data->title[0] = '\0';
	rndr_data->current_track = rndr_data->track_length = -1;
}

static int
GMERenderer_Render(const AudioRenderer* obj,
                   void* buf,
                   const size_t len)
{
	DataObject(rndr_data, obj);

	rndr_data->err = gme_play(rndr_data->emu, len / sizeof(T), buf);

	if (rndr_data->err != NULL)
		fprintf(stderr, "%s\n", rndr_data->err);

	return len;
}

static const char*
GMERenderer_Title(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->emu != NULL)
		return rndr_data->title;
	else
		return NULL;
}

static const char*
GMERenderer_Info(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->emu != NULL)
		return rndr_data->info;
	else
		return NULL;
}

static int
GMERenderer_Track(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	return rndr_data->current_track;
}

static int
GMERenderer_NTracks(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->emu != NULL)
		return gme_track_count(rndr_data->emu);

	return -1;
}

static int
GMERenderer_AppendInfo(char* dest,
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
GMERenderer_SetTrack(const AudioRenderer* obj, int track)
{
	DataObject(rndr_data, obj);

	gme_info_t* info = NULL;

	if (rndr_data->emu != NULL) {
		size_t n = 0;
		size_t songlen;

		rndr_data->current_track = track;

		rndr_data->err = gme_start_track(rndr_data->emu,
		                                 rndr_data->current_track);

		if (rndr_data->err != NULL) goto error;

		rndr_data->err = gme_track_info(rndr_data->emu,
		                                &info,
		                                rndr_data->current_track);

		if (rndr_data->err != NULL) goto error;

		n  = GMERenderer_AppendInfo(rndr_data->info, n,
									info->author, "Author: ");
		n += GMERenderer_AppendInfo(rndr_data->info, n,
									info->comment, "Comment: ");
		n += GMERenderer_AppendInfo(rndr_data->info, n,
									info->copyright, "Copyright: ");
		n += GMERenderer_AppendInfo(rndr_data->info, n,
									info->dumper, "Dumper: ");
		GMERenderer_AppendInfo(rndr_data->info, n,
							   info->system, "System: ");

		if (info->length != -1)
			rndr_data->track_length = info->length;
		else if (info->intro_length != -1 && info->loop_length != -1)
			rndr_data->track_length = info->play_length;
		else
			rndr_data->track_length = GME_TRACK_LENGTH;

		songlen = strnlen(info->song, MODP_STR_LENGTH - 1);

		if (songlen > 0)
			StrCpy(rndr_data->title, MODP_STR_LENGTH, info->song);

error:
		if (info != NULL)
			gme_free_info(info);

		if (rndr_data->err != NULL) {
			fprintf(stderr, "%s\n", rndr_data->err);
			rndr_data->title[0] = '\0';
			rndr_data->info[0] = '\0';
			rndr_data->track_length = MODP_MAX_SILENCE_MS;
		}

		return GMERenderer_Track(obj);
	}

	return -1;
}

static int
GMERenderer_PlayTime(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	if (rndr_data->emu != NULL)
		return gme_tell(rndr_data->emu) / 1000;

	return 0;
}

static int
GMERenderer_Length(const AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	return rndr_data->track_length / 1000;
}

static void
GMERenderer_Destroy(AudioRenderer* obj)
{
	DataObject(rndr_data, obj);

	GMERenderer_UnLoad((const AudioRenderer*) obj);

	free(rndr_data);
	free(obj);
}

AudioRenderer*
GMERenderer_Create(int fs, int bits, int channels)
{
	AudioRenderer* arndr;
	GMERenderer_Data* rndr_data;

	static AudioRenderer_VTable _vtable;
	static bool _initialized = false;

	if (!_initialized) {
		memset((void*) &_vtable, 0, sizeof(AudioRenderer_VTable));

		_vtable.Load     = (*GMERenderer_Load);
		_vtable.CanLoad  = (*GMERenderer_CanLoad);
		_vtable.Loaded   = (*GMERenderer_Loaded);
		_vtable.UnLoad   = (*GMERenderer_UnLoad);
		_vtable.Render   = (*GMERenderer_Render);
		_vtable.Title    = (*GMERenderer_Title);
		_vtable.Info     = (*GMERenderer_Info);
		_vtable.Track    = (*GMERenderer_Track);
		_vtable.NTracks  = (*GMERenderer_NTracks);
		_vtable.SetTrack = (*GMERenderer_SetTrack);
		_vtable.PlayTime = (*GMERenderer_PlayTime);
		_vtable.Length   = (*GMERenderer_Length);
		_vtable.Destroy  = (*GMERenderer_Destroy);

		_initialized = true;
	}

	arndr = (AudioRenderer*) calloc(1, sizeof(AudioRenderer));
	assert(arndr);

	rndr_data = (GMERenderer_Data*) calloc(1, sizeof(GMERenderer_Data));
	assert(rndr_data);

	rndr_data->fs = fs;
	rndr_data->bits = bits;
	rndr_data->channels = channels;

	rndr_data->emu = NULL;
	rndr_data->err = NULL;
	rndr_data->current_track = -1;
	rndr_data->track_length = -1;

	arndr->vtable = &_vtable;
	arndr->data = (void*) rndr_data;

	return arndr;
}
