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
#include "Utils.h"
#include "UIOptions.h"

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
	                 { .long_name = "fontstretch",
	                   .description = "Pixel double font vertically",
	                   .type = OPT_BOOL,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.b = false,
	                   .dest = &o.font_dbl },
	                 /* UI visual options - initialized below from ui_options[] */
	                 { .long_name = "bg_red",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.f,
	                   .min.f = 0.f,
	                   .max.f = 1.f,
	                   .dest = &o.ui.clr[0] },
	                 { .long_name = "bg_green",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.f,
	                   .min.f = 0.f,
	                   .max.f = 1.f,
	                   .dest = &o.ui.clr[1] },
	                 { .long_name = "bg_blue",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.f,
	                   .min.f = 0.f,
	                   .max.f = 1.f,
	                   .dest = &o.ui.clr[2] },
	                 { .long_name = "font_shake_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 4.f,
	                   .min.f = 0.f,
	                   .max.f = 100.f,
	                   .dest = &o.ui.font_shake_factor },
	                 { .long_name = "font_zoom_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 4.f,
	                   .min.f = 0.f,
	                   .max.f = 100.f,
	                   .dest = &o.ui.font_zoom_factor },
	                 { .long_name = "font_rotation_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 50.f,
	                   .min.f = 0.f,
	                   .max.f = 100.f,
	                   .dest = &o.ui.font_rotation_factor },
	                 { .long_name = "perturb_waterfall_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 2.f,
	                   .min.f = 0.f,
	                   .max.f = 100.f,
	                   .dest = &o.ui.perturb_waterfall_factor },
	                 { .long_name = "bg_flash_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 2.f,
	                   .min.f = 0.f,
	                   .max.f = 100.f,
	                   .dest = &o.ui.bg_flash_factor },
	                 { .long_name = "circ_pulse_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 4.f,
	                   .min.f = 0.f,
	                   .max.f = 100.f,
	                   .dest = &o.ui.circ_pulse_factor },
	                 { .long_name = "circ_spin_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 2.f,
	                   .min.f = 0.f,
	                   .max.f = 100.f,
	                   .dest = &o.ui.circ_spin_factor },
	                 { .long_name = "trail_factor",
	                   .description = "",
	                   .type = OPT_FLOAT,
	                   .has_arg = true,
	                   .in_cfg_file = true,
	                   .initial.f = 0.f,
	                   .min.f = 0.f,
	                   .max.f = 0.3f,
	                   .dest = &o.ui.trail_factor },
	                 { .long_name = "help",
	                   .description = "Display help",
	                   .type = OPT_NULL,
	                   .has_arg = false,
	                   .in_cfg_file = false,
	                   .dest = NULL } };

	/* Initialize UI option descriptions from unified ui_options[] array */
	/* This ensures single source of truth for UI option metadata */
	const size_t first_ui_opt = 12;  /* Index of bg_red in opt[] */
	for (size_t i = 0; i < NUM_UI_OPTIONS && (first_ui_opt + i) < sizeof(opt) / sizeof(opt[0]); i++) {
		strcpy(opt[first_ui_opt + i].long_name, (char*)ui_options[i].config_name);
		strcpy(opt[first_ui_opt + i].description, (char*)ui_options[i].description);
		opt[first_ui_opt + i].min.f = ui_options[i].min_val;
		opt[first_ui_opt + i].max.f = ui_options[i].max_val;
	}

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
