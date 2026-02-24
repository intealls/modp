// Copyright intealls
// License: GPL v3

#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include "NcursesWindow.h"
#include "Player.h"
#include "Globals.h"
#include "MinMax.h"
#include "Option.h"

#ifdef NDEBUG
#error NDEBUG should not be defined
#endif

int
main(int argc, char* argv[])
{
	NcursesWindow_State* wdw = NULL;
	Player_State* ps = NULL;
	bool running = true;
	Uint32 t_prev = 0;

	typedef struct {
		char path[_TINYDIR_PATH_MAX];
		char cfgpath[_TINYDIR_PATH_MAX];
		bool auto_inc;
		bool auto_rnd;
		size_t min_length;
	} NcursesOptions;

	NcursesOptions o = { 0 };

	Option opt[] = { { .long_name = "path",
	                   .description = "Initial path",
	                   .type = OPT_STRING,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.str = ".",
	                   .dest = &o.path },
	                 { .long_name = "config",
	                   .description = "Configuration file path",
	                   .type = OPT_STRING,
	                   .has_arg = true,
	                   .in_cfg_file = false,
#ifndef _WIN32
	                   .initial.str = "~/.modp/modp.toml",
#else
	                   .initial.str = "modp.toml",
#endif
	                   .dest = &o.cfgpath },
	                 { .long_name = "createconfig",
	                   .description = "Create configuration file in config path",
	                   .type = OPT_NULL,
	                   .has_arg = false,
	                   .in_cfg_file = false,
	                   .dest = NULL },
	                 { .long_name = "showconfig",
	                   .description = "Show current config",
	                   .type = OPT_NULL,
	                   .has_arg = false,
	                   .in_cfg_file = false,
	                   .dest = NULL },
	                 { .long_name = "auto_increment",
	                   .description = "Auto increment at min length/song end",
	                   .type = OPT_BOOL,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.b = true,
	                   .dest = &o.auto_inc },
	                 { .long_name = "random_auto_increment",
	                   .description = "Random song at auto increment",
	                   .type = OPT_BOOL,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.b = false,
	                   .dest = &o.auto_rnd },
	                 { .long_name = "song_min_length",
	                   .description = "Song minimum length",
	                   .type = OPT_UINT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.u = 0,
	                   .min.u = 0,
	                   .max.u = 999,
	                   .dest = &o.min_length },
	                 { .long_name = "help",
	                   .description = "Display help",
	                   .type = OPT_NULL,
	                   .has_arg = false,
	                   .in_cfg_file = false,
	                   .dest = NULL } };

	Option_Init(argc, argv, opt, sizeof(opt) / sizeof(Option));

	ps = Player_Init(48e3, 16, 2, o.min_length,
	                 o.auto_inc, o.auto_rnd, o.path);
	if (!ps) {
		fprintf(stderr, "Failed to initialize player\n");
		return 1;
	}

	wdw = NcursesWindow_Init(ps);
	if (!wdw) {
		fprintf(stderr, "Failed to initialize ncurses window\n");
		Player_Destroy(ps);
		return 1;
	}

	t_prev = SDL_GetTicks();

	// Target 60 FPS for smooth UI (16ms frame time)
	const int frame_delay = 16;

	while (running) {
		int sleep;
		bool got_input = false;
		Uint32 t_now;

		running = NcursesWindow_ProcessEvents(wdw, &got_input);
		if (!running) {
			break;  // Exit immediately without drawing or further processing
		}
		Player_UpdateAutoInc(wdw->ps, got_input);
		NcursesUI_Draw(wdw);

		t_now = SDL_GetTicks();

		sleep = frame_delay - (int)(t_now - t_prev);

		if (sleep > 0)
			SDL_Delay((Uint32)sleep);

		t_prev += frame_delay;
	}

	NcursesWindow_Destroy(wdw);
	Player_Destroy(ps);

	return 0;
}
