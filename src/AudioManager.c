// Copyright intealls
// License: GPL v3

#include <stdio.h>

#include <portaudio.h>

#include "AudioManager.h"
#include "OpenMPTRenderer.h"
#include "GMERenderer.h"

void
AudioManager_PlayPause(AudioManager* am)
{
	PaError active, stopped;
	assert(am);

	if (!AudioRenderer_Loaded(am->active_ar))
		return;

	active = Pa_IsStreamActive(am->stream);
	stopped = Pa_IsStreamStopped(am->stream);

	assert(active >= paNoError);
	assert(stopped >= paNoError);

	if (active) {
		am->pa_err = Pa_AbortStream(am->stream);
		am->playing = false;
		//atomic_store(&am->cb_msg, CBM_CLR_BUF);
	} else if (stopped) {
		am->pa_err = Pa_StartStream(am->stream);
		am->playing = true;
	}
}

bool
AudioManager_CanLoad(AudioManager* am,
                     void* data,
                     size_t len)
{
	bool r = false;
	assert(am);

	AudioRenderer** p;

	SDL_LockMutex(am->mutex);

	p = am->ars;

	while (*p != NULL) {
		if (AudioRenderer_CanLoad(*p, data, len)) {
			r = true;
			break;
		}

		p++;
	}

	SDL_UnlockMutex(am->mutex);

	return r;
}

int
AudioManager_Load(AudioManager* am,
                  const char* filename,
                  void* data,
                  size_t len)
{
	int r = 1;
	assert(am);

	AudioRenderer** p;

	SDL_LockMutex(am->mutex);

	AudioRenderer_UnLoad(am->active_ar);

	p = am->ars;

	while (*p != NULL) {
		if (!AudioRenderer_Load(*p, filename, data, len)) {
			am->pa_err = Pa_StartStream(am->stream);
			am->playing = true;
			am->active_ar = *p;
			atomic_store(&am->cb_msg, CBM_CLR_BUF);
			r = 0;
			break;
		} else {
			am->pa_err = Pa_StopStream(am->stream);
			am->playing = false;
		}
		p++;
	}

	SDL_UnlockMutex(am->mutex);

	return r;
}

bool
AudioManager_AlterSubTrack(AudioManager* am, int val)
{
	int track_sel;
	int ntracks;
	int r = -1;

	assert(am);
	assert(am->active_ar);

	ntracks = AudioRenderer_NTracks(am->active_ar);

	if (ntracks < 2)
		return false;

	SDL_LockMutex(am->mutex);

	track_sel = AudioRenderer_Track(am->active_ar) + val;

	if (track_sel >= 0 && track_sel < ntracks) {
		r = AudioRenderer_SetTrack(am->active_ar, track_sel);
		atomic_store(&am->cb_msg, CBM_CLR_BUF);
	}

	SDL_UnlockMutex(am->mutex);

	return (r != -1) && (r == track_sel);
}

bool
AudioManager_SilenceDetected(AudioManager* am)
{
	bool r = false;

	assert(am);

	if (atomic_load(&am->rt_msg) == RTM_AUTO_INC)
		r = true;

	atomic_store(&am->rt_msg, RTM_NONE);

	return r;
}

static int
RenderThread(void* data)
{
	AudioManager* am = (AudioManager*) data;
	assert(am);

	size_t silence_count = 0;

	T* temp = (T*) calloc(am->fs * am->channels, sizeof(T));
	assert(temp);

	while (am->running) {
		size_t rb_ct = RingBuffer_Count(am->render_buf);
		size_t samples = am->fs * am->channels / 4;
		size_t rendered = 0;

		SDL_SemWait(am->sem);

		SDL_LockMutex(am->mutex);

		if (atomic_load(&am->playing) && rb_ct < samples
		        && AudioRenderer_Loaded(am->active_ar)) {
			rendered = AudioRenderer_Render(am->active_ar,
			                                temp,
			                                samples * sizeof(T));

			RingBuffer_Write(am->render_buf, temp, samples);
		}

		SDL_UnlockMutex(am->mutex);

		if (atomic_load(&am->rt_msg) == RTM_NONE
		        && atomic_load(&am->playing)) {
			rendered /= sizeof(T);
			for (size_t i = 0; rendered > 0 && i <= rendered - 4; i += 2) {
				int l_diff = temp[i + 0] - temp[i + 2];
				int r_diff = temp[i + 1] - temp[i + 3];

				if (l_diff == 0 && r_diff == 0)
					silence_count++;
				else
					silence_count = 0;
			}
		}

		if (silence_count > am->max_silence) {
			atomic_store(&am->rt_msg, RTM_AUTO_INC);
			silence_count = 0;
		}
	}

	free(temp);

	return 0;
}

static int
PortAudio_Callback(const void* in, void* out,
                   unsigned long frames,
                   const PaStreamCallbackTimeInfo* time_info,
                   PaStreamCallbackFlags status_flags,
                   void* user)
{
	AudioManager* am = (AudioManager*) user;
	T* pa_out = (T*) out;

	(void) in;
	(void) time_info;
	(void) status_flags;

	int to_write;
	int written;

	if (atomic_load(&am->cb_msg) == CBM_CLR_BUF) {
		RingBuffer_ConsumerClear(am->render_buf);
		atomic_store(&am->cb_msg, CBM_NONE);
	}

	to_write = frames * am->channels;
	written = RingBuffer_Read(am->render_buf, pa_out, to_write);

	if (written < to_write)
		memset(pa_out + written, 0, (to_write - written) * sizeof(T));

	if (RingBuffer_Count(am->render_buf) < (int) frames * 2 * am->channels)
		SDL_SemPost(am->sem);

	RingBuffer_Write(am->playback_buf, out, to_write);

	return 0;
}

static void
PortAudio_Init(AudioManager* am)
{
	assert(am->fs == 48e3);
	assert(am->bits == 16);

	am->pa_err = Pa_Initialize();

	assert(am->pa_err == paNoError);

	am->pa_err = Pa_OpenDefaultStream(&am->stream,
	                                  0,
	                                  am->channels,
	                                  paInt16,
	                                  am->fs,
	                                  1536,
	                                  PortAudio_Callback,
	                                  (void*) am);

	assert(am->pa_err == paNoError);
}

void
AudioManager_Destroy(AudioManager* am)
{
	AudioRenderer** p;
	int status;

	assert(am);

	if (am->playing) {
		am->pa_err = Pa_StopStream(am->stream);
		assert(am->pa_err == paNoError);
	}

	am->playing = false;

	am->pa_err = Pa_CloseStream(am->stream);
	assert(am->pa_err == paNoError);

	Pa_Terminate();

	SDL_LockMutex(am->mutex);

	am->running = false;

	SDL_UnlockMutex(am->mutex);

	SDL_SemPost(am->sem);

	SDL_WaitThread(am->thread, &status);

	SDL_DestroyMutex(am->mutex);

	SDL_DestroySemaphore(am->sem);

	RingBuffer_Destroy(am->render_buf);
	RingBuffer_Destroy(am->playback_buf);

	p = am->ars;

	while (*p != NULL)
		AudioRenderer_Destroy(*p++);

	free(am->ars);
	free(am);

	return;
}

AudioManager*
AudioManager_Create(int fs, int bits, int channels)
{
	AudioManager* am;

	am = (AudioManager*) calloc(1, sizeof(AudioManager));
	assert(am);

	am->fs = fs;
	am->bits = bits;
	am->channels = channels;
	am->running = true;
	am->max_silence = fs * MODP_MAX_SILENCE_MS / 1000;

	atomic_store(&am->rt_msg, RTM_NONE);

	PortAudio_Init(am);

	am->ars = (AudioRenderer**) calloc(3, sizeof(AudioRenderer*));
	assert(am->ars);

	am->ars[0] = GMERenderer_Create(fs, bits, channels);
	am->ars[1] = OpenMPTRenderer_Create(fs, bits, channels);
	am->ars[2] = NULL;

	am->active_ar = am->ars[0];

	// TODO: Fix buffer sizes and set them to sane values
	am->render_buf = RingBuffer_Create(fs, 2);
	am->playback_buf = RingBuffer_Create(fs / 16, 2);

	am->mutex = SDL_CreateMutex();
	assert(am->mutex);
	am->sem = SDL_CreateSemaphore(0);
	assert(am->sem);
	am->thread = SDL_CreateThread(RenderThread, NULL, (void*) am);
	assert(am->thread);

	return am;
}
