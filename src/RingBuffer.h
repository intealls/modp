// Copyright intealls
// License: GPL v3

#ifndef SRC_RINGBUFFER_H_
#define SRC_RINGBUFFER_H_

#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

#include "Globals.h"
#include "MinMax.h"

typedef short T;

typedef struct RingBuffer {
	T* buffer;

	_Atomic int size,
	            reserve;

	_Atomic int playpos,
	            writepos;
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
	assert(rb);

	rb->playpos = rb->writepos = 0;
	rb->reserve = reserve;
	rb->size = size + reserve;

	assert(rb->size > 0);

	rb->buffer = (T*) calloc(rb->size, sizeof(T));
	assert(rb->buffer);

	return rb;
}

static void
RingBuffer_ConsumerClear(RingBuffer* rb)
{
	int n = RingBuffer_Count(rb);

	rb->playpos = (rb->playpos + n) % rb->size;
}

static void
RingBuffer_Destroy(RingBuffer* rb)
{
	assert(rb);
	assert(rb->buffer);

	free(rb->buffer);
	free(rb);
}

static int
RingBuffer_Count(const RingBuffer* rb)
{
	int diff = rb->writepos - rb->playpos;
	return diff < 0 ? rb->size + diff : diff;
}

static int
RingBuffer_Write(RingBuffer* rb,
                 const T* src,
                 int n)
{
	n = min_int(n, rb->size - rb->reserve - RingBuffer_Count(rb));
	n = n < 0 ? 0 : n;

	if (rb->writepos + n > rb->size) {
		memcpy(rb->buffer + rb->writepos,
		       src,
		       sizeof(T) * (rb->size - rb->writepos));
		memcpy(rb->buffer,
		       src + (rb->size - rb->writepos),
		       sizeof(T) * (n - (rb->size - rb->writepos)));
	} else {
		memcpy(rb->buffer + rb->writepos, src, sizeof(T) * n);
	}

	rb->writepos = (rb->writepos + n) % rb->size;

	return n;
}

static int
RingBuffer_Read(RingBuffer* rb,
                T* dst,
                int n)
{
	n = min_int(n, RingBuffer_Count(rb));
	n = n < 0 ? 0 : n;

	if (rb->playpos + n > rb->size) {
		memcpy(dst,
		       rb->buffer + rb->playpos,
		       sizeof(T) * (rb->size - rb->playpos));
		memcpy(dst + (rb->size - rb->playpos),
		       rb->buffer,
		       sizeof(T) * (n - (rb->size - rb->playpos)));
	} else {
		memcpy(dst, rb->buffer + rb->playpos, sizeof(T) * n);
	}

	rb->playpos = (rb->playpos + n) % rb->size;

	return n;
}

#endif /* SRC_RINGBUFFER_H_ */
