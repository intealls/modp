// Copyright intealls
// License: GPL v3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include "Config.h"
#include "Font.h"
#include "GL.h"
#include "GLWindow.h"

#include "Player.h"
#include "Globals.h"
#include "MinMax.h"

#ifdef NDEBUG
#error NDEBUG should not be defined
#endif

void
Usage(Options* o, const char* name)
{
	fprintf(stdout,
	        "\n%s [OPTIONS]\n\n"
	        "-p    Initial path, default is \"%s\"\n"
	        "-f    Path to a BDF font, default is an internal font\n"
	        "-v    Pixel-double font vertically, default is %u\n"
	        "-a    Auto increment at min length/song end, default is %u\n"
	        "-n    Random song at auto increment, default is %u\n"
	        "-m    Song minimum length, default is %" PRIu64 "\n"
	        "-w    Window width, default is %" PRIu64 "\n"
	        "-e    Window height, default is %" PRIu64 "\n"
	        "-l    Framerate limit, default is %.2f\n"
	        "-r    Background color red component, default is %.2f\n"
	        "-g    Background color green component, default is %.2f\n"
	        "-b    Background color blue component, default is %.2f\n"
	        "-c    Config file, default is $HOME/modp.toml\n\n"
	        "-h    Show default command line options\n\n",
	        name,
	        o->path,
	        o->font_dbl,
	        o->auto_inc,
	        o->auto_rnd,
	        o->min_length,
	        o->wdw_width,
	        o->wdw_height,
	        o->fps_limit,
	        o->clr_r,
	        o->clr_g,
	        o->clr_b,
	        o->configfile);
}

void
ParseOptions(Options* o, int argc, char* argv[])
{
	int c, tmp;
	while ((c = getopt(argc, argv, "p:f:v:a:n:m:w:e:l:r:g:b:c:")) != -1) {
		switch (c) {
			case 'p':
				strcpy(o->path, optarg);
				break;
			case 'f':
				strcpy(o->fontpath, optarg);
				break;
			case 'v':
				if (sscanf(optarg, "%d", &tmp) != 1) goto error;
				o->font_dbl = tmp ? true : false;
				break;
			case 'a':
				if (sscanf(optarg, "%d", &tmp) != 1) goto error;
				o->auto_inc = tmp ? true : false;
				break;
			case 'n':
				if (sscanf(optarg, "%d", &tmp) != 1) goto error;
				o->auto_rnd = tmp ? true : false;
				break;
			case 'm':
				if (sscanf(optarg, "%d", &tmp) != 1) goto error;
				o->min_length = tmp / 15 * 15;
				o->min_length = min_int(o->min_length, 999 / 15 * 15);
				break;
			case 'w':
				if (sscanf(optarg, "%d", &tmp) != 1) goto error;
				o->wdw_width = (size_t) tmp;
				o->wdw_width = min_int(max_int(o->wdw_width, 640), 3840);
				break;
			case 'e':
				if (sscanf(optarg, "%d", &tmp) != 1) goto error;
				o->wdw_height = (size_t) tmp;
				o->wdw_height = min_int(max_int(o->wdw_height, 360), 2160);
				break;
			case 'l':
				if (sscanf(optarg, "%f", &o->fps_limit) != 1) goto error;
				o->fps_limit = min_float(max_float(o->fps_limit, 3), 240);
				break;
			case 'r':
				if (sscanf(optarg, "%f", &o->clr_r) != 1) goto error;
				o->clr_r = min_float(max_float(o->clr_r, 0.f), 1.f);
				break;
			case 'g':
				if (sscanf(optarg, "%f", &o->clr_g) != 1) goto error;
				o->clr_g = min_float(max_float(o->clr_g, 0.f), 1.f);
				break;
			case 'b':
				if (sscanf(optarg, "%f", &o->clr_b) != 1) goto error;
				o->clr_b = min_float(max_float(o->clr_b, 0.f), 1.f);
				break;
			case 'c':
				strcpy(o->configfile, optarg);
				break;
			default:
				goto error;
		}
	}
	return;
error:
	Usage(o, argv[0]);
	exit(1);
}

int
CheckOptions(int argc, char* argv[])
{
	for (int i = 0; i < argc; i++) {
		if (strlen(argv[i]) >= _TINYDIR_PATH_MAX - 1)
			return 1;
		else if (strcmp(argv[i], "-h") == 0)
			return 1;
	}

	return 0;
}

int
main(int argc, char* argv[])
{
	GLWindow_State* wdw = NULL;
	Player_State* ps = NULL;
	bool running = true;
	Uint32 t_prev = 0;

	Options opt = { .path = ".",
	                .configfile = "",
	                .fontpath = "",
	                .font_dbl = false,
	                .wdw_width = 800,
	                .wdw_height = 480,
	                .auto_inc = true,
	                .auto_rnd = false,
	                .min_length = 0,
	                .fps_limit = 60.f,
	                .clr_r = 0.0f,
	                .clr_g = 0.33f,
	                .clr_b = 0.67f,
    };

	if (CheckOptions(argc, argv)) {
		Usage(&opt, argv[0]);
		exit(1);
	}

	ParseOptions(&opt, argc, argv);

	Config* cfg = malloc(sizeof(Config));
	ParseConfig(cfg, opt.configfile);

	ps = Player_Init(48e3, 16, 2, opt.min_length,
	                 opt.auto_inc, opt.auto_rnd, opt.path);
	assert(ps);

	wdw = GLWindow_Init(cfg, &opt, ps);
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
	free(cfg);
	return 0;
}
