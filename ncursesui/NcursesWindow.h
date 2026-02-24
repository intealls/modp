// Copyright intealls
// License: GPL v3

#ifndef NCURSESUI_NCURSESWINDOW_H_
#define NCURSESUI_NCURSESWINDOW_H_

#include <stdbool.h>
#include <fftw3.h>
#include <ncurses.h>

#include "Player.h"
#include "RingBuffer.h"

// Forward declaration to avoid including RingBuffer in header
// typedef short T;  // T is defined in RingBuffer.h

typedef enum NcursesVis {
	NCURSES_VIS_FFT = 0,
	NCURSES_VIS_SCOPE = 1,
	NCURSES_VIS_NONE = 2
} NcursesVis;

typedef struct Vis_State {
	T* vis_buf;
	size_t vis_len;
	size_t nsamples;
	size_t fft_len;
	fftwf_plan plan;
	float* signal;
	float* spectrum;
	float* window;
	fftwf_complex* result;
	T* flush_buf;      // Reusable buffer for ring buffer draining
	int* scope_yrow;   // Reusable y-row buffer for scope visualization
	int* scope_colors; // Reusable color buffer for scope visualization
	size_t scope_w;    // Cached width for scope buffer validity

} Vis_State;

typedef struct NcursesWindow_State {
	int width;
	int height;
	int max_items;

	NcursesVis vis;
	Vis_State* v;
	Player_State* ps;

	// Layout calculations
	int vis_height;
	int status_height;
	int browser_height;
	int browser_y;
	int browser_x;

	// Color pair indices
	int color_dir;
	int color_file;
	int color_status;
	int color_title;
	int color_time;

	// Status bar colors (for inverted background)
	int color_status_on;    // Green for "on" state
	int color_status_off;   // Red for "off" state
	int color_status_value; // Green for numeric values
	int color_status_vis;   // Yellow for visualization mode
} NcursesWindow_State;

bool            NcursesWindow_ProcessEvents(NcursesWindow_State*, bool*);
NcursesWindow_State* NcursesWindow_Init(Player_State*);
void            NcursesWindow_Destroy(NcursesWindow_State*);

void            NcursesUI_Draw(NcursesWindow_State*);

#endif /* NCURSESUI_NCURSESWINDOW_H_ */
