// Copyright intealls
// License: GPL v3

#ifndef GLUI_GLWINDOW_H_
#define GLUI_GLWINDOW_H_

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <fftw3.h>

#include <tinydir.h>

typedef struct GLWindow_State GLWindow_State;
typedef struct Vis_State Vis_State;

#include "Font.h"
#include "RingBuffer.h"
#include "Player.h"

typedef struct Options {
	char path[_TINYDIR_PATH_MAX];
	char fontpath[_TINYDIR_PATH_MAX];
	bool font_dbl;
	size_t wdw_width;
	size_t wdw_height;
	bool auto_inc;
	bool auto_rnd;
	size_t min_length;
	float fps_limit;
	float clr_r;
	float clr_g;
	float clr_b;
} Options;

typedef struct Star {
	int speed_x, speed_y,
	    xpos, ypos,
	    size,
	    phase, phase_inc,
	    rotation, rotation_inc;

	bool in_front, visible;
} Star;

struct Vis_State {
	T* vis_buf;
	size_t vis_len;

	Star* stars;
	size_t nstars;

	size_t nsamples;
	size_t fft_len;
	fftwf_plan plan;
	float* signal;
	float* spectrum;
	float* window;
	fftwf_complex* result;
};

typedef enum Vis { VIS_FFT = 0, VIS_SCOPE = 1, VIS_NONE = 2 } Vis;

struct GLWindow_State {
	SDL_Window* sdl_wdw;
	Vis vis;
	Vis_State* v;

	size_t width,
	       height;

	float fps_limit;

	Font* font;
	size_t max_items;

	Player_State* ps;
};

bool            GLWindow_ProcessEvents(GLWindow_State*, bool*);
GLWindow_State* GLWindow_Init(Options*, Player_State*);
void            GLWindow_Destroy(GLWindow_State*);

void            GLUI_Draw(GLWindow_State*);

#endif /* GLUI_GLWINDOW_H_ */
