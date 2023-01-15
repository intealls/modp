// Copyright: intealls
// Licence: GPL v3

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <assert.h>
#include <pwd.h>
#include <unistd.h>
#include <wordexp.h>

#include "Option.h"
#include "Globals.h"

static void
print_value_description(Option* opt)
{
	switch (opt->type) {
		case OPT_STRING:
			printf(", default is \"%s\"\n", (char*) opt->dest);
			break;
		case OPT_BOOL:
			printf(", default is %s\n", *((bool*) opt->dest) ? "true" : "false");
			break;
		case OPT_UINT:
			printf(", default is %lu (%lu->%lu)\n", opt->initial.u, opt->min.u, opt->max.u);
			break;
		case OPT_FLOAT:
			printf(", default is %.1f (%.1f->%.1f)\n", opt->initial.f, opt->min.f, opt->max.f);
			break;
		case OPT_NULL:
			printf("\n");
			break;
	}
}

static void
print_help(Option* option, size_t n_opts)
{
	printf("\n  %s %s\n\n", PACKAGE, VERSION);
	for (size_t i = 0; i < n_opts; i++) {
		printf("  --%-25s%s", option[i].long_name, option[i].description);
		print_value_description(&option[i]);
	}
	printf("\n");
}

static Option*
find_option_from_long_name(Option* option, size_t n_opts, const char* long_name)
{
	for (size_t i = 0; i < n_opts; i++)
		if (!strcmp(option[i].long_name, long_name))
			return &option[i];

	return NULL;
}

static void
write_opt(Option* opt, FILE* out)
{
	int ret;
	if (opt->type == OPT_NULL || !opt->in_cfg_file)
		return;

	ret = fprintf(out, "# %s\n", opt->description);
	assert(ret >= 0);
	ret = fprintf(out, "%s = ", opt->long_name);
	assert(ret >= 0);

	switch (opt->type) {
		case OPT_STRING:
			ret = fprintf(out, "\"%s\"", (char*) opt->dest);
			break;
		case OPT_BOOL:
			ret = fprintf(out, "%s", *((bool*) opt->dest) ? "true" : "false");
			break;
		case OPT_UINT:
			ret = fprintf(out, "%lu", *((size_t*) opt->dest));
			break;
		case OPT_FLOAT:
			ret = fprintf(out, "%f", *((float*) opt->dest));
			break;
		default:
			break;
	}
	assert(ret >= 0);

	ret = fprintf(out, "\n\n");
	assert(ret >= 0);
}

static int
create_config(Option* option, size_t n_opts, const char* path)
{
	char resolved_path[_TINYDIR_PATH_MAX] = { 0 };
	char realpath_resolved_path[_TINYDIR_PATH_MAX] = { 0 };
	char tmp[_TINYDIR_PATH_MAX] = { 0 };
	char filename[_TINYDIR_PATH_MAX] = { 0 };
	FILE* cfg_file;
	struct stat st = { 0 };
	char* result;
	char* pos;

#if DEBUG
	printf("incoming: %s\n", path);
#endif

	// expand "~"
    wordexp_t exp_result;
    wordexp(path, &exp_result, 0);

    result = memccpy(resolved_path, exp_result.we_wordv[0], '\0', _TINYDIR_PATH_MAX);
    assert(result);
    wordfree(&exp_result);

    // if no DIRSEP, prefix with "./"
    result = strchr(resolved_path, DIRSEP);
    if (result == NULL) {
    	strncat(tmp, ".", sizeof(tmp) - 1);
    	strncat(tmp, DIRSEP_STR, sizeof(tmp) - 2);
    	strncat(tmp, resolved_path, sizeof(tmp) - strlen(tmp));
    	memccpy(resolved_path, tmp, '\0', sizeof(resolved_path));
    }

#if DEBUG
	printf("after ~ and ./ expansion: %s\n", resolved_path);
#endif

	result = strrchr(resolved_path, DIRSEP);
	assert(result && strlen(result) > 0);
	sscanf(result + 1, "%s", filename);
	assert(strlen(filename));

#if DEBUG
	printf("filename: %s\n", filename);
#endif

	pos = resolved_path;

    while (1) {
    	memset(tmp, 0, sizeof(tmp));
		pos = strchr(pos, DIRSEP);

		if (pos == NULL)
			break;

		memcpy(tmp, resolved_path, pos - resolved_path + 1);
		pos += 1;

		result = realpath(tmp, realpath_resolved_path);

#if DEBUG
		printf("tmp: %s\n", tmp);
		printf("realpath_resolved_path: %s\n", realpath_resolved_path);
#endif

		if (!result) {
			if (stat(tmp, &st) == -1) {
			    int status = mkdir(tmp, 0700);
			    assert(status == 0);
			    continue;
			}
		}

		if (result == NULL)
			break;
    }

    result = strncat(realpath_resolved_path, DIRSEP_STR, _TINYDIR_PATH_MAX - strlen(realpath_resolved_path));
    assert(result);
    result = strncat(realpath_resolved_path, filename, _TINYDIR_PATH_MAX - strlen(realpath_resolved_path));
    assert(result);

	cfg_file = fopen(realpath_resolved_path, "w");
	assert(cfg_file);

	for (size_t i = 0; i < n_opts; i++)
		write_opt(&option[i], cfg_file);

	fclose(cfg_file);

	printf("Created configuration file at %s\n", realpath_resolved_path);

	return 0;
}

static int
set_dest_from_value(Option* option, void* value)
{
	switch (option->type) {
		char* tmp_str;
		size_t tmp_uint;
		float tmp_float;

		case OPT_STRING:
			tmp_str = memccpy((char*) option->dest, (char*) value, '\0', _TINYDIR_PATH_MAX);
			assert(tmp_str);
			break;
		case OPT_BOOL:
			*((bool*) option->dest) = *((bool*) value);
			break;
		case OPT_UINT:
			tmp_uint = *((size_t*) value);
			tmp_uint = tmp_uint > option->max.u ? option->max.u : tmp_uint;
			tmp_uint = tmp_uint < option->min.u ? option->min.u : tmp_uint;
			*((size_t*) option->dest) = tmp_uint;
			break;
		case OPT_FLOAT:
			tmp_float = *((float*) value);
			tmp_float = tmp_float > option->max.f ? option->max.f : tmp_float;
			tmp_float = tmp_float < option->min.f ? option->min.f : tmp_float;
			*((float*) option->dest) = tmp_float;
			break;
		default:
			return 1;
			break;
	}

	return 0;
}

static int
set_dest_from_string(Option* option, char* string)
{
	switch (option->type) {
		bool tmp_bool;
		size_t tmp_uint;
		float tmp_float;
		int ret;

		case OPT_STRING:
			set_dest_from_value(option, string);
			break;
		case OPT_BOOL:
			if (tolower(string[0]) == 'f' || string[0] == '0') {
				tmp_bool = false;
				set_dest_from_value(option, &tmp_bool);
			} else if (tolower(string[0]) == 't' || string[0] == '1') {
				tmp_bool = true;
				set_dest_from_value(option, &tmp_bool);
			}
			break;
		case OPT_UINT:
			ret = sscanf(string, "%ld", &tmp_uint);
			assert(ret == 1);
			set_dest_from_value(option, &tmp_uint);
			break;
		case OPT_FLOAT:
			ret = sscanf(string, "%f", &tmp_float);
			assert(ret == 1);
			set_dest_from_value(option, &tmp_float);
			break;
		default:
			break;
	}

	return 0;
}

int
Option_Init(int argc, char* argv[], Option* option, size_t n_opts)
{
	struct option long_options[n_opts + 1];
	Option* option_iter = option;

	memset(&long_options, 0, sizeof(struct option) * (n_opts + 1));

	for (size_t i = 0; i < n_opts; i++) {
		// Populate getopt_long structure
		long_options[i].name = option_iter->long_name;
		long_options[i].has_arg = option_iter->has_arg;

		// Populate with default value
		switch (option_iter->type) {
			case OPT_STRING:
				set_dest_from_value(option_iter, (void*) option_iter->initial.str);
				break;
			case OPT_BOOL:
				set_dest_from_value(option_iter, (void*) &option_iter->initial.b);
				break;
			case OPT_UINT:
				set_dest_from_value(option_iter, (void*) &option_iter->initial.u);
				break;
			case OPT_FLOAT:
				set_dest_from_value(option_iter, (void*) &option_iter->initial.f);
				break;
			default:
				break;
		}

		option_iter++;
	}

	int c;
	while (1) {
		int option_index = 0;
		Option* tmp_option;

		c = getopt_long(argc, argv, "0", long_options, &option_index);

		if (c != 0)
			break;

		tmp_option = &option[option_index];

		switch (tmp_option->type) {
			case OPT_STRING:
			case OPT_BOOL:
			case OPT_UINT:
			case OPT_FLOAT:
				set_dest_from_string(tmp_option, optarg);
				break;
			case OPT_NULL:
				if (!strcmp("help", tmp_option->long_name)) {
					print_help(option, n_opts);
					exit(0);
				}
				if (!strcmp("createconfig", tmp_option->long_name)) {
					Option* path = find_option_from_long_name(option, n_opts, "config");
					create_config(option, n_opts, (const char*) path->dest);
					exit(0);
				}
				if (!strcmp("showconfig", tmp_option->long_name)) {
					for (size_t i = 0; i < n_opts; i++)
						write_opt(&option[i], stdout);
					exit(0);
				}
				break;
		}
	}

	return 0;
}
