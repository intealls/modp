// Copyright intealls
// License: GPL v3

#include <stdlib.h>
#include <malloc.h>

#include <SDL2/SDL.h>

#include <tinydir.h>

#include "Player.h"
#include "LocalDir.h"
#include "AudioManager.h"
#include "OpenMPTRenderer.h"

int
Player_Perform(Player_State* ps)
{
	const char* filename;
	bool isdir;

	assert(ps);

	filename = Directory_GetName(ps->dir, ps->dir_ofs, &isdir);

	if (isdir) {
		Directory_LoadDir(ps->dir, ps->dir_ofs);
		ps->dir_ofs = Directory_SubDirIdx(ps->dir);
	} else {
		char* data;
		size_t len;

		data = Directory_GetFile(ps->dir,
		                         ps->dir_ofs,
		                         &len,
		                         MODP_MAX_FILESIZE);

		if (data != NULL) {
			if (AudioManager_CanLoad(ps->am, data, len))
				AudioManager_Load(ps->am, filename, data, len);

			free(data);
		}
	}

	return 0;
}

int
Player_DirUp(Player_State* ps)
{
	assert(ps);

	const char* name;
	bool isdir;

	name = Directory_GetName(ps->dir, 0, &isdir);

	if (isdir && strncmp(name, "..", _TINYDIR_PATH_MAX) == 0) {
		Directory_LoadDir(ps->dir, 0);

		ps->dir_ofs = Directory_SubDirIdx(ps->dir);
	}

	return ps->dir_ofs;
}

int
Player_Home(Player_State* ps)
{
	assert(ps);

	ps->dir_ofs = 0;

	return ps->dir_ofs;
}

int
Player_End(Player_State* ps)
{
	assert(ps);

	ps->dir_ofs = Directory_NTotal(ps->dir) - 1;

	return ps->dir_ofs;
}

int
Player_PlayNext(Player_State* ps, bool wrap)
{
	int dir_total;
	int dir_ndirs;

	assert(ps);

	dir_total = (int) Directory_NTotal(ps->dir);
	dir_ndirs = (int) Directory_NDirs(ps->dir);

	if (Directory_NFiles(ps->dir) > 0) {
		ps->dir_ofs += 1;

		if (ps->dir_ofs < dir_ndirs)
			ps->dir_ofs = dir_ndirs;

		if (ps->dir_ofs >= dir_total)
			ps->dir_ofs = wrap ? dir_ndirs : dir_total - 1;

		Player_Perform(ps);
	}

	return ps->dir_ofs;
}

int
Player_PlayPrev(Player_State* ps, bool wrap)
{
	int dir_total;
	int dir_ndirs;

	assert(ps);

	dir_total = (int) Directory_NTotal(ps->dir);
	dir_ndirs = (int) Directory_NDirs(ps->dir);

	if (Directory_NFiles(ps->dir) > 0) {
		if (ps->dir_ofs > 0)
			ps->dir_ofs -= 1;

		if (ps->dir_ofs < dir_ndirs)
			ps->dir_ofs = wrap ? dir_total - 1 : dir_ndirs;

		Player_Perform(ps);
	}

	return ps->dir_ofs;
}

int
Player_PlayRandom(Player_State* ps)
{
	int dir_ndirs;
	int dir_nfiles;

	assert(ps);

	dir_ndirs = (int) Directory_NDirs(ps->dir);
	dir_nfiles = (int) Directory_NFiles(ps->dir);

	if (dir_nfiles > 0) {
		ps->dir_ofs = dir_ndirs + rand() % dir_nfiles;

		Player_Perform(ps);
	}

	return ps->dir_ofs;
}

int
Player_AlterOffset(Player_State* ps, int n)
{
	int dir_total;

	assert(ps);

	dir_total = (int) Directory_NTotal(ps->dir);

	ps->dir_ofs += n;

	ps->dir_ofs = ps->dir_ofs < 0 ? 0 : ps->dir_ofs;
	ps->dir_ofs = ps->dir_ofs > dir_total - 1 ? dir_total - 1 : ps->dir_ofs;

	return ps->dir_ofs;
}

int
Player_AlterMinLength(Player_State* ps, int step)
{
	assert(ps);

	ps->min_length += step;
	ps->min_length = ps->min_length < 0 ? 0 : ps->min_length;

	return ps->min_length;
}

void
Player_ToggleAutoInc(Player_State* ps)
{
	assert(ps);

	ps->auto_inc = !ps->auto_inc;
}

void
Player_ToggleAutoRnd(Player_State* ps)
{
	assert(ps);

	ps->auto_rnd = !ps->auto_rnd;
}

void
Player_UpdateAutoInc(Player_State* ps,
                     bool got_input)
{
	Uint32 t_now;
	assert(ps);

	if (got_input) {
		ps->last_input = SDL_GetTicks();
		return;
	}

	t_now = SDL_GetTicks();

	if ((AudioRenderer_PlayTime(ps->am->active_ar) >=
	        AudioRenderer_Length(ps->am->active_ar) &&
	        (t_now - ps->last_input) / 1e3 > ps->min_length && ps->am->playing) ||
	        (ps->am->playing && AudioManager_SilenceDetected(ps->am))) {
		if (ps->auto_inc) {
			if (ps->auto_rnd) {
				Player_PlayRandom(ps);
			} else {
				if (!AudioManager_AlterSubTrack(ps->am, 1))
					Player_PlayNext(ps, true);
			}

			ps->last_input = SDL_GetTicks();
		}
	}
}

void
Player_PlayPause(Player_State* ps)
{
	assert(ps);

	AudioManager_PlayPause(ps->am);
}

void
Player_AlterSubTrack(Player_State* ps, int val)
{
	assert(ps);

	AudioManager_AlterSubTrack(ps->am, val);
}

int
Player_GetPlaybackData(Player_State* ps,
                       T* buf,
                       int len,
                       bool partial)
{
	int rb_count = RingBuffer_Count(ps->am->playback_buf);

	if (rb_count >= len) {
		RingBuffer_Read(ps->am->playback_buf, buf, len);
	} else if (partial && rb_count > 0 && rb_count < len) {
		memmove(buf, buf + rb_count, (len - rb_count) * sizeof(T));
		RingBuffer_Read(ps->am->playback_buf, buf + len - rb_count, rb_count);
	}

	return rb_count;
}

void
Player_Destroy(Player_State* ps)
{
	assert(ps);

	Directory_Destroy(ps->dir);
	AudioManager_Destroy(ps->am);

	free(ps);
}

Player_State*
Player_Init(int fs, int bits, int channels,
            int min_length, bool auto_inc, bool auto_rnd,
            const char* path)
{
	Player_State* ps;

	ps = (Player_State*) calloc(1, sizeof(Player_State));
	assert(ps);

	ps->min_length = min_length;
	ps->auto_inc = auto_inc;
	ps->auto_rnd = auto_rnd;

	ps->am = AudioManager_Create(fs, bits, channels);
	ps->dir = LocalDir_Create(path);

	ps->last_input = SDL_GetTicks64();
	// TODO: probably not random enough
	srand(ps->last_input);

	return ps;
}
