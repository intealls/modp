// Copyright intealls
// License: GPL v3

#ifndef SRC_AUDIOMANAGER_H_
#define SRC_AUDIOMANAGER_H_

#include <stdatomic.h>

#include <SDL2/SDL_thread.h>
#include <portaudio.h>

#include "RingBuffer.h"
#include "AudioRenderer.h"

typedef enum CallbackMessage {
	CBM_NONE,
	CBM_CLR_BUF
} CallbackMessage;

typedef enum RenderThreadMessage {
	RTM_NONE,
	RTM_AUTO_INC
} RenderThreadMessage;

typedef struct AudioManager {
	// render_buf is fed samples which are fetched by PortAudio
	RingBuffer* render_buf;
	// playback_buf is only intended to be used for visualization
	RingBuffer* playback_buf;

	_Atomic CallbackMessage cb_msg;
	SDL_Thread* thread;
	SDL_mutex* mutex;
	SDL_sem* sem;

	// TODO: make replacements to atomic_load/atomic_store
	_Atomic bool running,
	             playing;

	AudioRenderer* active_ar;
	AudioRenderer** ars;
	int fs, bits, channels;

	_Atomic RenderThreadMessage rt_msg;
	size_t max_silence;

	PaStream* stream;
	PaError pa_err;
} AudioManager;

AudioManager*  AudioManager_Create(int, int, int);
void           AudioManager_Destroy(AudioManager*);
AudioRenderer* AudioManager_CanLoad(AudioManager*, void*, size_t);
int            AudioManager_Load(AudioManager*,
                                 AudioRenderer*,
                                 const char*,
                                 void*,
                                 size_t);
void           AudioManager_PlayPause(AudioManager*);
bool           AudioManager_AlterSubTrack(AudioManager*, int);
bool           AudioManager_SilenceDetected(AudioManager*);

#endif /* SRC_AUDIOMANAGER_H_ */
