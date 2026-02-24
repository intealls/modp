// Copyright: intealls
// Licence: GPL v3

#include <SDL2/SDL_assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#ifndef _WIN32
#include <wordexp.h>
#endif
#include <SDL2/SDL_log.h>

#include "Option.h"
#include "Globals.h"
#include "Utils.h"
#include "toml.h"

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
resolve_path(char* real_path, const char* path)
{
	if (!real_path || !path) {
		return -1;
	}

	char resolved_path[_TINYDIR_PATH_MAX] = { 0 };
	char tmp[_TINYDIR_PATH_MAX] = { 0 };
	char* result;
	SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "incoming: %s\n", path);

#ifndef _WIN32
	// expand "~" with proper error handling and security flags
	wordexp_t exp_result;
	int ret = wordexp(path, &exp_result, WRDE_NOCMD);
	if (ret != 0 || exp_result.we_wordc == 0) {
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM,
		             "Failed to expand path: %s\n", path);
		return -1;
	}

	// Check length before copying
	if (strlen(exp_result.we_wordv[0]) >= _TINYDIR_PATH_MAX) {
		wordfree(&exp_result);
		return -1;
	}

	StrCpy(resolved_path, _TINYDIR_PATH_MAX, exp_result.we_wordv[0]);
	wordfree(&exp_result);
#else
	StrCpy(resolved_path, _TINYDIR_PATH_MAX, path);
#endif

	// if no DIRSEP, prefix with "./"
	result = strchr(resolved_path, DIRSEP);
	if (result == NULL) {
		size_t tmp_len = strlen(tmp);
		size_t space_remaining = sizeof(tmp) - tmp_len - 1;

		// Safe string concatenation with bounds checking
		if (space_remaining > 0) {
			tmp[tmp_len] = '.';
			tmp_len++;
			space_remaining--;
		}

		if (space_remaining > strlen(DIRSEP_STR)) {
			strcpy(tmp + tmp_len, DIRSEP_STR);
			tmp_len += strlen(DIRSEP_STR);
			space_remaining -= strlen(DIRSEP_STR);
		}

		if (strlen(resolved_path) < space_remaining) {
			strcpy(tmp + tmp_len, resolved_path);
		}

		StrCpy(resolved_path, sizeof(resolved_path), tmp);
	}

	SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "after ~ and ./ expansion: %s\n", resolved_path);
	StrCpy(real_path, _TINYDIR_PATH_MAX, resolved_path);
	return 0;
}

static int
create_config(Option* option, size_t n_opts, const char* path)
{
	char* resolved_path = calloc(_TINYDIR_PATH_MAX, sizeof(char));
	if (!resolved_path) {
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to allocate memory\n");
		return -1;
	}

	if (resolve_path(resolved_path, path) != 0) {
		free(resolved_path);
		return -1;
	}

	char realpath_resolved_path[_TINYDIR_PATH_MAX] = { 0 };
	char tmp[_TINYDIR_PATH_MAX] = { 0 };
	char filename[_TINYDIR_PATH_MAX] = { 0 };
	FILE* cfg_file;
	struct stat st = { 0 };
	char* result;
	char* pos;

	result = strrchr(resolved_path, DIRSEP);
	if (!result || strlen(result) <= 1) {
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Invalid path format\n");
		free(resolved_path);
		return -1;
	}

	StrCpy(filename, sizeof(filename), result + 1);
	if (strlen(filename) == 0) {
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Empty filename\n");
		free(resolved_path);
		return -1;
	}
	SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "filename: %s\n", filename);

	pos = resolved_path;

	while (1) {
		memset(tmp, 0, sizeof(tmp));
		pos = strchr(pos, DIRSEP);

		if (pos == NULL)
			break;

		memcpy(tmp, resolved_path, pos - resolved_path + 1);
		pos += 1;

		result = realpath(tmp, realpath_resolved_path);

		SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "tmp: %s\n", tmp);
		SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "realpath_resolved_path: %s\n", realpath_resolved_path);

		if (stat(tmp, &st) == -1) {
#ifndef _WIN32
			int status = mkdir(tmp, 0700);
#else
			int status = mkdir(tmp);
#endif
			if (status != 0 && errno != EEXIST) {
				SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to create directory: %s\n", tmp);
				free(resolved_path);
				return -1;
			}
			continue;
		}

		if (result == NULL)
			break;
	}

#ifndef _WIN32
	size_t path_len = strlen(realpath_resolved_path);
	if (path_len < _TINYDIR_PATH_MAX - 1) {
		realpath_resolved_path[path_len] = DIRSEP;
		realpath_resolved_path[path_len + 1] = '\0';
	}
#endif

	if (strlen(realpath_resolved_path) + strlen(filename) >= _TINYDIR_PATH_MAX) {
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Path too long\n");
		free(resolved_path);
		return -1;
	}

	strcat(realpath_resolved_path, filename);

	cfg_file = fopen(realpath_resolved_path, "w");
	if (!cfg_file) {
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM,
		             "Failed to create config file: %s\n", realpath_resolved_path);
		free(resolved_path);
		return -1;
	}

	for (size_t i = 0; i < n_opts; i++)
		write_opt(&option[i], cfg_file);

	if (fclose(cfg_file) != 0) {
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to close config file\n");
		free(resolved_path);
		return -1;
	}

	SDL_Log("Created configuration file at %s\n", realpath_resolved_path);

	free(resolved_path);
	return 0;
}

static int
set_dest_from_value(Option* option, void* value)
{
	switch (option->type) {
		size_t tmp_uint;
		float tmp_float;

		case OPT_STRING:
			StrCpy((char*) option->dest, _TINYDIR_PATH_MAX, (char*) value);
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
			ret = sscanf(string, "%zu", &tmp_uint);
			if (ret != 1) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				             "Invalid unsigned integer value: %s\n", string);
				break;
			}
			set_dest_from_value(option, &tmp_uint);
			break;
		case OPT_FLOAT:
			ret = sscanf(string, "%f", &tmp_float);
			if (ret != 1) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				             "Invalid float value: %s\n", string);
				break;
			}
			set_dest_from_value(option, &tmp_float);
			break;
		default:
			break;
	}

	return 0;
}

static int
config_lookup_set_dest(toml_table_t* config, Option* option)
{
	if (!config)
		return 0;

	toml_datum_t d;
	switch (option->type) {
		case OPT_STRING:
        d = toml_string_in(config, option->long_name);
        if (d.ok) {
            if ((strcmp(d.u.s, "") != 0) && (strcmp(d.u.s, option->initial.str) != 0)) {
			    StrCpy((char*) option->dest, _TINYDIR_PATH_MAX, d.u.s);
            }
            free(d.u.s);
        }
			break;
		case OPT_BOOL:
        d = toml_bool_in(config, option->long_name);
        if (d.ok) {
            if (d.u.b != option->initial.b) {
	            *((bool*) option->dest) = d.u.b;
            }
        }
			break;
		case OPT_UINT:
            d = toml_int_in(config, option->long_name);
            if (d.ok) {
                if (d.u.i != option->initial.u) {
                    *((size_t*) option->dest) = d.u.i;
                }
            }
			break;
		case OPT_FLOAT:
            d = toml_double_in(config, option->long_name);
            if (d.ok) {
                if (d.u.d != option->initial.f) {
                    *((float*) option->dest) = d.u.d;
                }
            }
			break;
		default:
			return 1;
			break;
	}
	return 0;
}

static char*
get_config_from_opts(int argc, char* argv[], Option* option, size_t n_opts)
{
    //TODO make this function less weird
	struct option long_options[n_opts + 1];
	Option* option_iter = option;
	memset(&long_options, 0, sizeof(struct option) * (n_opts + 1));

	for (size_t i = 0; i < n_opts;  i++) {
		long_options[i].name = option_iter->long_name;
		long_options[i].has_arg = option_iter->has_arg;
		switch (option_iter->type) {
			case OPT_STRING:
                if (strcmp(option_iter->long_name, "config") == 0) {
				    set_dest_from_value(option_iter, (void*) option_iter->initial.str);
                }
                break;
			default:
				break;
		}
        if (strcmp(option_iter->long_name, "config") == 0) {
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
                if (strcmp(tmp_option->long_name, "config") == 0) {
                    return optarg;
                }
            default:
				break;
        }
	}
    return option_iter->initial.str;
}

int
Option_Init(int argc, char* argv[], Option* option, size_t n_opts)
{
	struct option long_options[n_opts + 1];
	Option* option_iter = option;
	char* config_path = calloc(_TINYDIR_PATH_MAX, sizeof(char));
	// Option* opt_cfg = malloc(sizeof(Option) * (n_opts + 1));
	char* cfgpath = get_config_from_opts(argc, argv, option, n_opts);
	optind = 1;
	toml_table_t* config = NULL;
	FILE* fd;
	char errbuf[256];

	resolve_path(config_path, cfgpath);

	fd = fopen(config_path, "r+");
	if (!fd) {
	    SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "failed to open configuration %s: %s\n", config_path, strerror(errno));
	} else {
	    config = toml_parse_file(fd, errbuf, sizeof(errbuf));
	    fclose(fd);
    }

	if (!config) {
	    SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "failed to parse %s: %s\n", config_path, strerror(errno));
	}

	memset(&long_options, 0, sizeof(struct option) * (n_opts + 1));

	for (size_t i = 0; i < n_opts; i++) {
		// Populate getopt_long structure
		long_options[i].name = option_iter->long_name;
		long_options[i].has_arg = option_iter->has_arg;

		// Populate with default value
		switch (option_iter->type) {
			case OPT_STRING:
				set_dest_from_value(option_iter, (void*) option_iter->initial.str);
				config_lookup_set_dest(config, option_iter);
				break;
			case OPT_BOOL:
				set_dest_from_value(option_iter, (void*) &option_iter->initial.b);
				config_lookup_set_dest(config, option_iter);
				break;
			case OPT_UINT:
				set_dest_from_value(option_iter, (void*) &option_iter->initial.u);
				config_lookup_set_dest(config, option_iter);
				break;
			case OPT_FLOAT:
				set_dest_from_value(option_iter, (void*) &option_iter->initial.f);
				config_lookup_set_dest(config, option_iter);
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
					if (config) toml_free(config);
					free(config_path);
					exit(0);
				}
				if (!strcmp("createconfig", tmp_option->long_name)) {
					create_config(option, n_opts, (const char*) config_path);
					if (config) toml_free(config);
					free(config_path);
					exit(0);
				}
				if (!strcmp("showconfig", tmp_option->long_name)) {
					for (size_t i = 0; i < n_opts; i++)
						write_opt(&option[i], stdout);
					if (config) toml_free(config);
					free(config_path);
					exit(0);
				}
				break;
		}
	}

	if (config) toml_free(config);
    free(config_path);
	return 0;
}
