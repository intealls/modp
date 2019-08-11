// Copyright intealls
// License: GPL v3

#ifndef SRC_MINMAX_H_
#define SRC_MINMAX_H_

static inline int
min_int(int a, int b) { return (a < b) ? a : b; }
static inline int
max_int(int a, int b) { return (a > b) ? a : b; }

static inline short
min_short(short a, short b) { return (a < b) ? a : b; }
static inline short
max_short(short a, short b) { return (a > b) ? a : b; }

static inline float
min_float(float a, float b) { return (a < b) ? a : b; }
static inline float
max_float(float a, float b) { return (a > b) ? a : b; }

#endif /* SRC_MINMAX_H_ */
