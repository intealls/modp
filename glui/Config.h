// boop boop!
#ifndef GLUI_CONFIG_H_
#define GLUI_CONFIG_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

typedef struct Config {
    char* g_startpath;
    char* gfx_fontpath;
    char* gfx_cursor;
    int gfx_sdl_window;

} Config;
void SetDefaultConfig(Config* cfg);
void ParseConfig(Config* cfg, char* configfile);
#endif /* GLUI_CONFIG_H_ */
