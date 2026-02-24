// Copyright intealls
// License: GPL v3

#ifndef SRC_RINGBUFFER_H_
#define SRC_RINGBUFFER_H_

#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <SDL2/SDL_mutex.h>

#include "Globals.h"
#include "MinMax.h"

typedef short T;

typedef struct RingBuffer {
	T* buffer;

	int size;
	int reserve;

	_Atomic int playpos;
	_Atomic int writepos;
	SDL_mutex* mutex;
} RingBuffer;

static RingBuffer* RingBuffer_Create        (int, int);
static void        RingBuffer_ConsumerClear (RingBuffer*);
static void        RingBuffer_Destroy       (RingBuffer*);
static int         RingBuffer_Count         (const RingBuffer*);
static int         RingBuffer_Write         (RingBuffer*, const T*, int);
static int         RingBuffer_Read          (RingBuffer*, T*, int);

static RingBuffer*
RingBuffer_Create(int size,
                  int reserve)
{
	RingBuffer* rb = (RingBuffer*) calloc(1, sizeof(RingBuffer));
	if (!rb) return NULL;

	rb->playpos = rb->writepos = 0;
	rb->reserve = reserve;
	rb->size = size + reserve;

	if (rb->size <= 0 || rb->size > SIZE_MAX / sizeof(T)) {
		free(rb);
		return NULL;
	}

	rb->buffer = (T*) calloc(rb->size, sizeof(T));
	if (!rb->buffer) {
		free(rb);
		return NULL;
	}

	rb->mutex = SDL_CreateMutex();
	if (!rb->mutex) {
		free(rb->buffer);
		free(rb);
		return NULL;
	}

	return rb;
}

static void
RingBuffer_ConsumerClear(RingBuffer* rb)
{
	int n = RingBuffer_Count(rb);

	atomic_store_explicit(&rb->playpos,
	                      (atomic_load_explicit(&rb->playpos, memory_order_relaxed) + n) % rb->size,
	                      memory_order_relaxed);
}

static void
RingBuffer_Destroy(RingBuffer* rb)
{
	assert(rb);
	assert(rb->buffer);

	SDL_DestroyMutex(rb->mutex);
	free(rb->buffer);
	free(rb);
}

static int
RingBuffer_Count(const RingBuffer* rb)
{
	int writepos = atomic_load_explicit(&rb->writepos, memory_order_acquire);
	int playpos = atomic_load_explicit(&rb->playpos, memory_order_acquire);
	int diff = writepos - playpos;
	return diff < 0 ? rb->size + diff : diff;
}

static int
RingBuffer_Write(RingBuffer* rb,
                 const T* src,
                 int n)
{
	SDL_LockMutex(rb->mutex);

	int count = RingBuffer_Count(rb);
	int available_space = rb->size - rb->reserve - count;
	if (available_space < 0) {
		available_space = 0;
	}

	n = min_int(n, available_space);
	if (n < 0) n = 0;

	if (n > 0) {
		int writepos = atomic_load_explicit(&rb->writepos, memory_order_relaxed);

		if (writepos + n > rb->size) {
			size_t first_chunk = rb->size - writepos;
			if (first_chunk > (size_t)rb->size || (size_t)(n - first_chunk) > (size_t)rb->size) {
				n = 0;
			} else {
				memcpy(rb->buffer + writepos, src, sizeof(T) * first_chunk);
				memcpy(rb->buffer, src + first_chunk, sizeof(T) * (n - first_chunk));
			}
		} else {
			if (n > rb->size - writepos) {
				n = rb->size - writepos;
			}
			memcpy(rb->buffer + writepos, src, sizeof(T) * n);
		}

		atomic_store_explicit(&rb->writepos, (writepos + n) % rb->size, memory_order_release);
	}

	SDL_UnlockMutex(rb->mutex);

	return n;
}

static int
RingBuffer_Read(RingBuffer* rb,
                T* dst,
                int n)
{
	SDL_LockMutex(rb->mutex);

	int count = RingBuffer_Count(rb);
	n = min_int(n, count);
	if (n < 0) n = 0;

	if (n > 0) {
		int playpos = atomic_load_explicit(&rb->playpos, memory_order_relaxed);

		if (playpos + n > rb->size) {
			size_t first_chunk = rb->size - playpos;
			if (first_chunk > (size_t)rb->size || (size_t)(n - first_chunk) > (size_t)rb->size) {
				n = 0;
			} else {
				memcpy(dst, rb->buffer + playpos, sizeof(T) * first_chunk);
				memcpy(dst + first_chunk, rb->buffer, sizeof(T) * (n - first_chunk));
			}
		} else {
			if (n > rb->size - playpos) {
				n = rb->size - playpos;
			}
			memcpy(dst, rb->buffer + playpos, sizeof(T) * n);
		}

		atomic_store_explicit(&rb->playpos, (playpos + n) % rb->size, memory_order_release);
	}

	SDL_UnlockMutex(rb->mutex);

	return n;
}

#endif /* SRC_RINGBUFFER_H_ */
