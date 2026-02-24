// Copyright intealls
// License: GPL v3

#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <stdio.h>

#ifndef DEBUG
#define DEBUG 0
#endif

// String length constants
#define MODP_STR_LENGTH      (1024)

// File size limits (4MB)
#define MODP_MAX_FILESIZE    (4 * 1024 * 1024)

// Silence detection threshold (3 seconds)
#define MODP_MAX_SILENCE_MS  (3000)

// Ring buffer sizes
#define MODP_RNDR_BUF_SEC    (1)
#define RINGBUFFER_RENDER_RESERVE  (4096)
#define RINGBUFFER_PLAYBACK_RESERVE (512)

// PortAudio configuration
#define PORTAUDIO_FRAMES_PER_BUFFER (1536)

// HVL maximum track length (10 minutes) for safety
#define HVL_MAX_FRAMES (48000 * 60 * 10)

// Silence detection
#define SILENCE_THRESHOLD_FRAMES (4)

#define DebugPrint(ptr) \
	do { \
		if (DEBUG) { \
			 fprintf(stderr, "%p: %s\n", (void*) ptr, __func__); \
			 fflush(stderr); \
		} \
	} while (0)

#ifdef _WIN32
#define DIRSEP '\\'
#define DIRSEP_STR "\\"
#define realpath(N,R) _fullpath((R),(N), _TINYDIR_PATH_MAX)
#else
#define DIRSEP '/'
#define DIRSEP_STR "/"
#endif

#endif /* GLOBALS_H_ */
