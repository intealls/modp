// Copyright intealls
// License: GPL v3

#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include "Font.h"
#include "GL.h"
#include "GLWindow.h"

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
	GLWindow_State* wdw = NULL;
	Player_State* ps = NULL;
	bool running = true;
	Uint32 t_prev = 0;
	Options o = { 0 };

	Option opt[] = { { .long_name = "path",
	                   .description = "Initial path",
	                   .type = OPT_STRING,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.str = ".",
	                   .dest = &o.path },
	                 { .long_name = "font",
	                   .description = "Path to a BDF font",
	                   .type = OPT_STRING,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.str = "",
	                   .dest = &o.fontpath },
	                 { .long_name = "cursor",
	                   .description = "Path to a custom cursor",
	                   .type = OPT_STRING,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.str = "",
	                   .dest = &o.cursorpath },
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
	                 { .long_name = "width",
	                   .description = "Window width",
	                   .type = OPT_UINT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.u = 800,
	                   .min.u = 320,
	                   .max.u = 1280,
	                   .dest = &o.wdw_width },
	                 { .long_name = "height",
	                   .description = "Window height",
	                   .type = OPT_UINT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.u = 480,
	                   .min.u = 240,
	                   .max.u = 960,
	                   .dest = &o.wdw_height },
	                 { .long_name = "framelimit",
	                   .description = "Framerate limit",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 60,
	                   .min.f = 1.f,
	                   .max.f = 240.f,
	                   .dest = &o.fps_limit },
	                 { .long_name = "bg_red",
	                   .description = "Background color red component",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0,
	                   .min.f = 0.f,
	                   .max.f = 1.f,
	                   .dest = &o.clr_r },
	                 { .long_name = "bg_green",
	                   .description = "Background color green component",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.33,
	                   .min.f = 0.f,
	                   .max.f = 1.f,
	                   .dest = &o.clr_g },
	                 { .long_name = "bg_blue",
	                   .description = "Background color blue component",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.67,
	                   .min.f = 0.f,
	                   .max.f = 1.f,
	                   .dest = &o.clr_b },
	                 { .long_name = "fontstretch",
	                   .description = "Pixel double font vertically",
	                   .type = OPT_BOOL,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.b = false,
	                   .dest = &o.font_dbl },
	                 { .long_name = "font_shake_factor",
	                   .description = "Shake on-screen text in tune with music",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.f,
	                   .min.f = 0.f,
	                   .max.f = 9000.f,
	                   .dest = &o.font_shake_factor },
	                 { .long_name = "font_zoom_factor",
	                   .description = "Zoom on-screen text in tune with music",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.f,
	                   .min.f = 0.f,
	                   .max.f = 9000.f,
	                   .dest = &o.font_zoom_factor },
	                 { .long_name = "help",
	                   .description = "Display help",
	                   .type = OPT_NULL,
	                   .has_arg = false,
	                   .in_cfg_file = false,
	                   .dest = NULL } };

	Option_Init(argc, argv, opt, sizeof(opt) / sizeof(Option));

	ps = Player_Init(48e3, 16, 2, o.min_length,
	                 o.auto_inc, o.auto_rnd, o.path);
	assert(ps);

	wdw = GLWindow_Init(&o, ps);
	assert(wdw);

	t_prev = SDL_GetTicks();

	while (running) {
		int sleep;
		bool got_input = false;
		Uint32 t_now;

		running = GLWindow_ProcessEvents(wdw, &got_input);
		Player_UpdateAutoInc(wdw->ps, got_input);
		GL_Clear();
		GLUI_Draw(wdw);
		SDL_GL_SwapWindow(wdw->sdl_wdw);

		t_now = SDL_GetTicks();

		sleep = (1e3 / wdw->fps_limit) - (t_now - t_prev);

		if (sleep > 0)
			SDL_Delay(sleep);

		t_prev = SDL_GetTicks();
	}

	Player_Destroy(wdw->ps);
	GLWindow_Destroy(wdw);

	return 0;
}
