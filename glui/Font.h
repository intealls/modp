// Copyright intealls
// License: GPL v3

#ifndef FONT_H_
#define FONT_H_

#include <stdbool.h>
#include <SDL2/SDL_opengl.h>

typedef struct Font Font;

#include "GLWindow.h"

struct Font {
	int tex_width,
	    tex_height;

	GLuint tex_handle;

	int font_width,
	    font_height;

	GLubyte color[4];

	unsigned char* pixels;
};

void  Font_DrawString(GLWindow_State*, const char*, int, int, int);
void  Font_Destroy(Font*);
Font* Font_Init(char*, bool);

#endif
