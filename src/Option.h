// Copyright: intealls
// Licence: GPL v3

#ifndef OPTION_H_
#define OPTION_H_

#include "tinydir.h"

typedef enum OptionType {
	OPT_STRING,
	OPT_BOOL,
	OPT_UINT,
	OPT_FLOAT,
	OPT_NULL
} OptionType;

typedef struct Option {
	const char long_name[_TINYDIR_PATH_MAX];
	const char description[_TINYDIR_PATH_MAX];
	OptionType type;
	int has_arg;
	bool in_cfg_file;
	union {
		char str[_TINYDIR_PATH_MAX];
		bool b;
		size_t u;
		float f;
	} initial;
	union {
		size_t u;
		float f;
	} min;
	union {
		size_t u;
		float f;
	} max;
	void* dest;
} Option;

int Option_Init(int, char*[], Option*, size_t);

#endif
