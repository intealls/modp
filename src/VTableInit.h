// Copyright intealls
// License: GPL v3

#ifndef SRC_VTABLEINIT_H_
#define SRC_VTABLEINIT_H_

#include <SDL2/SDL_atomic.h>
#include <stdbool.h>

// Helper macro for thread-safe vtable initialization
// Usage: VTABLE_INIT_ONCE(Type, vtable_ptr, init_fn)
// where init_fn sets a static vtable and returns a pointer to it
#define VTABLE_INIT_ONCE(Type, ptr, init_fn) \
	do { \
		static SDL_SpinLock _init_lock = 0; \
		static bool _initialized = false; \
		\
		SDL_AtomicLock(&_init_lock); \
		if (!_initialized) { \
			(ptr) = (Type*)(init_fn)(); \
			_initialized = true; \
		} \
		SDL_AtomicUnlock(&_init_lock); \
	} while (0)

#endif /* SRC_VTABLEINIT_H_ */
