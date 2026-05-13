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
	char cfgpath[_TINYDIR_PATH_MAX];
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
	 float font_shake_factor;
		float font_zoom_factor;
		float font_rotation_factor;
		float bg_flash_factor;
		float perturb_waterfall_factor;
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
	float mean_energy_band_div16;
	fftwf_complex* result;

	/* Reactive background: bass→R, mids→G, treble→B,
	   smoothly lerped each frame toward spectral targets. */
	float reactive_color[3];

	/* FFT waterfall (spectrogram) visualization */
	unsigned char* wf_buf;      // pixel data: wf_width * wf_height * 4 (RGBA)
	size_t         wf_width;    // horizontal resolution (FFT bins)
	size_t         wf_height;   // vertical scroll depth (rows)
	GLuint         wf_tex;      // OpenGL 2D texture handle
};

typedef enum Vis { VIS_FFT = 0, VIS_SCOPE = 1, VIS_WATERFALL = 2, VIS_NONE = 3 } Vis;

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

	float clrcolor[3];
	float font_shake_factor;
	float font_zoom_factor;
	float font_rotation_factor;
	float bg_flash_factor;
	float perturb_waterfall_factor;

	// Mouse/hover state
	int mouse_x;
	int mouse_y;
	int hover_item;    // Browser item index under cursor, -1 if none

	// Computed layout for hit testing
	int layout_browser_x;
	int layout_browser_y;
	int layout_item_height;
	int layout_browser_height;
	int layout_status_y;
	int layout_fkey_x[5];  // F1-F5 x positions in status bar
	int layout_fkey_w[5];  // F1-F5 widths in status bar
};

bool            GLWindow_ProcessEvents(GLWindow_State*, bool*);
GLWindow_State* GLWindow_Init(Options*, Player_State*);
void            GLWindow_Destroy(GLWindow_State*);

void            GLUI_Draw(GLWindow_State*);

#endif /* GLUI_GLWINDOW_H_ */
