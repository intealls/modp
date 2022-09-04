// Copyright intealls
// License: GPL v3

#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <stdio.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define MODP_STR_LENGTH      (1024)
#define MODP_MAX_FILESIZE    (4 * 1024 * 1024)
#define MODP_MAX_SILENCE_MS  (3000)
#define MODP_RNDR_BUF_SEC    (1)

#define DebugPrint(ptr) \
	do { \
		if (DEBUG) { \
			 fprintf(stderr, "%p: %s\n", (void*) ptr, __func__); \
			 fflush(stderr); \
		} \
	} while (0)

#endif /* GLOBALS_H_ */
