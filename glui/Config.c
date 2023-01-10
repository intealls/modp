#include <SDL2/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <SDL2/SDL_video.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include "Config.h"
#include "../deps/tomlc99/toml.h"

// #include <toml.h>

void
SetDefaultConfig(Config* cfg) {
    char* user_home = getenv("HOME");
    char* font_path = "/.modp/fonts";
    char* cursor_path = "/.modp/cursor.png";
    cfg->g_startpath = "";

    cfg->gfx_fontpath = malloc((strlen(user_home)+strlen(font_path)+1));
    cfg->gfx_cursor = malloc((strlen(user_home)+strlen(cursor_path)+1));
    strcpy(cfg->gfx_fontpath, user_home);
    strcpy(cfg->gfx_cursor, user_home);
    strcat(cfg->gfx_fontpath, font_path);
    strcat(cfg->gfx_cursor, cursor_path);
    cfg->gfx_sdl_window = SDL_WINDOW_OPENGL;
}

void
ParseConfig(Config* cfg, char* configfile)
{
    FILE* fp;
    char errbuf[200];

    SetDefaultConfig(cfg);
    // use default configfile if not set
    if ((configfile != NULL) && (configfile[0] == '\0')) {
        configfile = getenv("HOME");
        strcat(configfile, "/.modp/modp.toml");
    }

    fp = fopen(configfile, "r");
    if (!fp) {
        SDL_Log("unable to open %s, %i", configfile, errno);
        return;
    }

    toml_table_t* config = toml_parse_file(fp, errbuf, sizeof(errbuf));
    if (!config) {
        SDL_Log("failed to parse %s, %s", configfile, errbuf);
        return;
    }

    toml_table_t* general = toml_table_in(config, "general");
    if (general) {
        toml_datum_t g_startpath = toml_string_in(general, "startpath");
        if (g_startpath.ok) {
            cfg->g_startpath = g_startpath.u.s;
        }
    }

    toml_table_t* gfx = toml_table_in(config, "gfx");
    if (gfx) {
        toml_datum_t gfx_api = toml_string_in(gfx, "api");
        toml_datum_t gfx_fontpath = toml_string_in(gfx, "fontpath");
        toml_datum_t gfx_cursor = toml_string_in(gfx, "cursor");

        if (gfx_api.ok) {
            if (strcmp(gfx_api.u.s, "opengl") == 0) {
                cfg->gfx_sdl_window = SDL_WINDOW_OPENGL;
            }
            if (strcmp(gfx_api.u.s, "vulkan") == 0) {
                SDL_Log("no support for vulkan yet, sorry.");
                // cfg->gfx_sdl_window = SDL_WINDOW_VULKAN;
            }
        }
        if (gfx_fontpath.ok) {
            cfg->gfx_fontpath = gfx_fontpath.u.s;
        }
        if (gfx_cursor.ok) {
            cfg->gfx_cursor = gfx_cursor.u.s;
        }
    }
    toml_free(config);
}
