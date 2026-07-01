// Copyright intealls
// License: GPL v3

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#include <ncurses.h>
#include <fftw3.h>

#include "NcursesWindow.h"
#include "Player.h"
#include "AudioRenderer.h"
#include "Directory.h"
#include "Globals.h"
#include "RingBuffer.h"

// Constants
// NCURSES_VIS_NSAMPLES: Controls window-function length. The actual number of
// samples processed per frame is determined by vis_len/2 (which equals terminal
// width in columns), clamped to fft_len. This constant only matters when the
// terminal is wider than 768 columns (vis_len > 768).
#define NCURSES_VIS_NSAMPLES 384
#define MIN_TERMINAL_WIDTH 80
#define MIN_TERMINAL_HEIGHT 24
#define KEY_DEL 127
#define KEY_ESC 27
#define VIS_COLOR_PAIRS_LOW 6  // Color pairs 6-9: blue→cyan→yellow→red

#define VIS_COLOR_PAIRS_RANDOM 14  // Color pairs 14-19: random colors for scope visualization
#define VIS_NUM_RANDOM_COLORS 6

// FFT visualization constants
#define FFT_SCALE_FACTOR 6.0f
#define FFT_BAR_COUNT 9
#define FFT_ENERGY_NORMALIZE 6.0f

// Scope visualization constants
#define SCOPE_HEADROOM_FACTOR 0.80f

// Vertical bar characters - index 0 is thickest (bottom), index 8 is lightest (top)
static const char fft_chars[] = { '#', '@', '+', '=', '-', '.', ',', ':', ' ' };

// Initialize color pairs and assign to state
static void
InitColorPairs(NcursesWindow_State* wdw)
{
	if (has_colors()) {
		start_color();
		use_default_colors();

		// Color pairs
		init_pair(1, COLOR_BLUE, -1);     // Directories (bold)
		init_pair(2, COLOR_WHITE, -1);    // Files
		init_pair(3, COLOR_BLACK, COLOR_WHITE);  // Status bar (inverted)
		init_pair(4, COLOR_YELLOW, -1);   // Song title
		init_pair(5, COLOR_GREEN, -1);    // Time display
		init_pair(6, COLOR_BLUE, -1);     // FFT: low energy (blue)
		init_pair(7, COLOR_CYAN, -1);     // FFT: medium-low energy
		init_pair(8, COLOR_YELLOW, -1);   // FFT: medium-high energy
		init_pair(9, COLOR_RED, -1);       // FFT: high energy (red)
		// Status bar colors on inverted background
		init_pair(10, COLOR_GREEN, COLOR_WHITE);  // "on" state (green on white)
		init_pair(11, COLOR_RED, COLOR_WHITE);   // "off" state (red on white)
		init_pair(12, COLOR_GREEN, COLOR_WHITE); // Numeric values (green on white)
		init_pair(13, COLOR_YELLOW, COLOR_BLACK); // Vis mode (yellow on black, stands out)
		// Scope visualization random colors
		init_pair(14, COLOR_MAGENTA, -1);  // Random color 1
		init_pair(15, COLOR_CYAN, -1);     // Random color 2
		init_pair(16, COLOR_GREEN, -1);    // Random color 3
		init_pair(17, COLOR_WHITE, -1);    // Random color 4
		init_pair(18, COLOR_YELLOW, -1);   // Random color 5
		init_pair(19, COLOR_BLUE, -1);     // Random color 6

		wdw->color_dir = COLOR_PAIR(1);
		wdw->color_file = COLOR_PAIR(2);
		wdw->color_status = COLOR_PAIR(3);
		wdw->color_title = COLOR_PAIR(4);
		wdw->color_time = COLOR_PAIR(5);
		wdw->color_status_on = COLOR_PAIR(10);    // Green on white
		wdw->color_status_off = COLOR_PAIR(11);   // Red on white
		wdw->color_status_value = COLOR_PAIR(12); // Green on white
		wdw->color_status_vis = COLOR_PAIR(13);   // Yellow (bold will be added)
	} else {
		// Fallback for colorless terminals
		wdw->color_dir = A_BOLD;
		wdw->color_file = A_NORMAL;
		wdw->color_status = A_REVERSE;
		wdw->color_title = A_BOLD;
		wdw->color_time = A_NORMAL;
		// Status bar fallback
		wdw->color_status_on = A_BOLD;     // Bold for "on"
		wdw->color_status_off = A_NORMAL;  // Normal for "off"
		wdw->color_status_value = A_BOLD;  // Bold for values
		wdw->color_status_vis = A_BOLD;     // Bold for vis mode
	}
}

// Helper function for summing stereo samples
static inline float
sum_stereo_samples(const T* buf, size_t idx)
{
	return (float)buf[idx * 2] + (float)buf[idx * 2 + 1];
}

// Map normalized position (0=bottom/center, 1=top/edge) to color pair
static inline int
vis_energy_to_color(float t)
{
	if (t < 0.25f) return VIS_COLOR_PAIRS_LOW;
	if (t < 0.5f)  return VIS_COLOR_PAIRS_LOW + 1;
	if (t < 0.75f) return VIS_COLOR_PAIRS_LOW + 2;
	return VIS_COLOR_PAIRS_LOW + 3;
}

// Check if audio is currently playing
static inline bool
is_playing(NcursesWindow_State* wdw)
{
	return wdw->ps && wdw->ps->am && wdw->ps->am->playing;
}

// Draw an "on/off" status toggle with appropriate coloring
static void
draw_status_toggle(const char* label, bool enabled, NcursesWindow_State* wdw)
{
	addstr(label);
	if (enabled) {
		attron(wdw->color_status_on | A_BOLD);
		addstr("on");
		attroff(wdw->color_status_on | A_BOLD);
	} else {
		attron(wdw->color_status_off);
		addstr("off");
		attroff(wdw->color_status_off);
	}
	attron(wdw->color_status);
}

static void Vis_Destroy(Vis_State* v);

static Vis_State*
Vis_Init(size_t wdw_width, size_t nsamples)
{
	Vis_State* v = calloc(1, sizeof(*v));
	if (!v) return NULL;

	v->fft_len = wdw_width;
	v->nsamples = nsamples;
	v->vis_len = wdw_width * 2;

	v->vis_buf = calloc(v->vis_len, sizeof(*v->vis_buf));
	if (!v->vis_buf) goto fail;
	v->window = calloc(v->fft_len, sizeof(*v->window));
	if (!v->window) goto fail;
	v->signal = calloc(v->fft_len, sizeof(*v->signal));
	if (!v->signal) goto fail;
	v->spectrum = calloc(v->fft_len, sizeof(*v->spectrum));
	if (!v->spectrum) goto fail;
	v->result = calloc(v->fft_len, sizeof(*v->result));
	if (!v->result) goto fail;

	// Reusable buffers to avoid per-frame allocation
	v->flush_buf = malloc(v->vis_len * sizeof(*v->flush_buf));
	if (!v->flush_buf) goto fail;

	// Scope visualization buffers (will be reallocated if terminal widens)
	v->scope_yrow = malloc(wdw_width * sizeof(*v->scope_yrow));
	if (!v->scope_yrow) goto fail;
	v->scope_colors = malloc(wdw_width * sizeof(*v->scope_colors));
	if (!v->scope_colors) goto fail;
	v->scope_w = wdw_width;

	// Blackman-Nuttall window (B=1.9761), ~100dB sidelobe attenuation
	size_t window_len = (nsamples < v->fft_len) ? nsamples : v->fft_len;
	for (size_t i = 0; i < window_len; i++) {
		double idx = (double)i;
		double n_minus_1 = (double)(window_len - 1);
		v->window[i] = 0.3635819 - 0.4891775 * cos(2 * M_PI * idx / n_minus_1) +
		               0.1365995 * cos(4 * M_PI * idx / n_minus_1) -
		               0.0106411 * cos(6 * M_PI * idx / n_minus_1);
	}

	v->plan = fftwf_plan_dft_r2c_1d(v->fft_len, v->signal, v->result, FFTW_ESTIMATE);
	if (!v->plan) {
		fprintf(stderr, "FFTW plan creation failed\n");
		goto fail;
	}

	return v;
fail:
	Vis_Destroy(v);
	return NULL;
}

static void
Vis_Destroy(Vis_State* v)
{
	if (!v) return;

	// fftwf_destroy_plan(NULL) is safe, and free(NULL) is safe
	fftwf_destroy_plan(v->plan);
	free(v->vis_buf);
	free(v->signal);
	free(v->spectrum);
	free(v->window);
	free(v->result);
	free(v->flush_buf);
	free(v->scope_yrow);
	free(v->scope_colors);
	free(v);
}

static void
Vis_Cleanup(void)
{
	fftwf_cleanup();
}

static void
Vis_Update(NcursesWindow_State* wdw)
{
	Vis_State* v = wdw->v;

	if (!wdw || !v) {
		return;
	}

	if (wdw->ps && wdw->ps->am && wdw->ps->am->playing) {
		// Flush excess buffer data to reduce latency
		// If there's more than 2 frames worth of data, skip some to catch up
		int rb_count = RingBuffer_Count(wdw->ps->am->playback_buf);
		int max_latency = v->vis_len * 2;  // Keep at most 2 frames of latency
		if (rb_count > max_latency && v->flush_buf) {
			int samples_to_flush = rb_count - max_latency;
			int to_read = (samples_to_flush < (int)v->vis_len) ?
			              samples_to_flush : (int)v->vis_len;
			RingBuffer_Read(wdw->ps->am->playback_buf, v->flush_buf, to_read);
		}

		// Don't use partial mode for more responsive visualization
		// This prevents old data from being shifted forward, reducing latency
		int samples_read = Player_GetPlaybackData(wdw->ps, v->vis_buf, v->vis_len, false);
		// Zero any unfilled portion to avoid stale data from previous frames
		size_t filled = (samples_read > 0) ? (size_t)samples_read : 0;
		if (filled < v->vis_len) {
			memset(v->vis_buf + filled, 0, (v->vis_len - filled) * sizeof(T));
		}

		size_t n = 2 * v->nsamples < v->vis_len ? v->nsamples : v->vis_len / 2;
		if (n > v->fft_len) n = v->fft_len;
		for (size_t i = 0; i < v->fft_len; i++) {
			v->signal[i] = 0.0f;
		}
		for (size_t i = 0; i < n; i++) {
			v->signal[i] = sum_stereo_samples(v->vis_buf, i) * 0.5f * v->window[i];
		}

		fftwf_execute(v->plan);

		size_t half_bins = v->fft_len / 2;
		for (size_t i = 0; i <= half_bins; i++) {
			float energy = sqrtf(v->result[i][0] * v->result[i][0] +
			                     v->result[i][1] * v->result[i][1]);
			v->spectrum[i] = log10f(energy + 1.0f);
		}
		for (size_t i = half_bins + 1; i < v->fft_len; i++) {
			v->spectrum[i] = 0.0f;
		}
	}
}

static void
clear_visualization_area(NcursesWindow_State* wdw)
{
	if (!wdw) return;

	for (int row = 0; row < wdw->vis_height; row++) {
		move(row, 0);
		clrtoeol();
	}
}

static inline int
clamp_int(int value, int min_val, int max_val)
{
	return value < min_val ? min_val :
	       value > max_val ? max_val : value;
}

static void
DrawFftVisualization(NcursesWindow_State* wdw)
{
	Vis_State* v = wdw->v;

	if (!wdw || !v || wdw->width <= 0 || v->fft_len <= 0) {
		return;
	}

	float scale = (float)wdw->vis_height / FFT_SCALE_FACTOR;
	int hminus1 = wdw->vis_height - 1;
	size_t half_bins = v->fft_len / 2;
	if (half_bins <= 0) half_bins = 1;

	for (int col = 0; col < wdw->width; col++) {
		size_t idx = (size_t)col * half_bins / wdw->width;
		if (idx >= half_bins)
			idx = half_bins;

		float energy = v->spectrum[idx];
		int bar_height = clamp_int((int)(energy * scale), 0, hminus1);
		float normalized_energy = fminf(energy / FFT_ENERGY_NORMALIZE, 1.0f);

		for (int row = wdw->vis_height - 1; row >= wdw->vis_height - bar_height; row--) {
			int pos_in_bar = hminus1 - row;
			float t = (hminus1 > 0) ? (float)pos_in_bar / hminus1 : 0.0f;

			int bar_idx = clamp_int(
				(int)((normalized_energy - t * 0.3f) * (FFT_BAR_COUNT - 1)),
				0, FFT_BAR_COUNT - 1);

			move(row, col);
			attron(A_BOLD | COLOR_PAIR(vis_energy_to_color(t)));
			addch(fft_chars[bar_idx]);
			attrset(A_NORMAL);
		}
	}
}


static void
DrawScopeVisualization(NcursesWindow_State* wdw)
{
	Vis_State* v = wdw->v;

	if (!wdw || !v) {
		return;
	}

	int mid = wdw->vis_height / 2;
	size_t total_samples = v->vis_len / 2;
	size_t step = (total_samples > (size_t)wdw->width) ?
	              (total_samples + (size_t)wdw->width - 1) / (size_t)wdw->width : 1;

	int w = wdw->width;

	// Grow persistent buffers if the terminal got wider
	if ((size_t)w > v->scope_w) {
		int* new_yrow = realloc(v->scope_yrow, w * sizeof(*new_yrow));
		int* new_colors = realloc(v->scope_colors, w * sizeof(*new_colors));
		if (!new_yrow || !new_colors) {
			free(new_yrow);
			clear_visualization_area(wdw);
			return;
		}
		v->scope_yrow = new_yrow;
		v->scope_colors = new_colors;
		v->scope_w = (size_t)w;
	}
	int *yrow = v->scope_yrow;
	int *col_colors = v->scope_colors;

	// Generate random color assignments for each column
	for (int col = 0; col < w; col++) {
		col_colors[col] = VIS_COLOR_PAIRS_RANDOM + (rand() % VIS_NUM_RANDOM_COLORS);
	}

	// Find peak amplitude for normalization
	float peak = 1.0f;
	for (int col = 0; col < wdw->width; col++) {
		size_t si = (size_t)col * step;
		if (si < v->vis_len / 2) {
			float s = fabsf(sum_stereo_samples(v->vis_buf, si));
			if (s > peak) peak = s;
		}
	}

	// Keep headroom for better visualization range
	float headroom = (float)mid * SCOPE_HEADROOM_FACTOR;
	float gain = (peak > headroom) ? (headroom / peak) : 1.0f;

	int valid = 0;

	for (int col = 0; col < w; col++) {
		size_t si = (size_t)col * step;
		if (si < total_samples) {
			float ys = (float)mid + sum_stereo_samples(v->vis_buf, si) * gain;
			yrow[col] = clamp_int((int)roundf(ys), 0, wdw->vis_height - 1);
			valid = col + 1;
		} else {
			yrow[col] = mid;
		}
	}

	// Clear the visualization area
	clear_visualization_area(wdw);

	// Draw subtle center reference line
	attron(A_DIM | COLOR_PAIR(vis_energy_to_color(0.0f)));
	move(mid, 0);
	for (int col = 0; col < w; col++) {
		addch('.');
	}
	attroff(A_DIM | COLOR_PAIR(vis_energy_to_color(0.0f)));

	// Draw fill + trace for each column
	for (int col = 0; col < valid; col++) {
		int yr = yrow[col];
		int color_pair = col_colors[col];

		// Fill from center to waveform position
		int fill_start, fill_end;
		if (yr <= mid) {
			fill_start = yr;
			fill_end = mid;
		} else {
			fill_start = mid;
			fill_end = yr;
		}

		// Subtle fill with ':' adjacent to trace
		for (int r = fill_start; r <= fill_end; r++) {
			if (r == yr || r == mid) continue;
			move(r, col);
			int fill_dist = abs(r - yr);
			attron(COLOR_PAIR(color_pair));
			addch(fill_dist == 1 ? ':' : ' ');
			attroff(COLOR_PAIR(color_pair));
		}

		// Draw trace with slope-based ASCII characters
		float dy = 0;
		if (col + 1 < valid) dy = yrow[col + 1] - yrow[col];
		else if (col > 0) dy = yrow[col] - yrow[col - 1];

		move(yr, col);
		attron(A_BOLD | COLOR_PAIR(color_pair));

		if (dy > 0.5f) addch('\\');
		else if (dy < -0.5f) addch('/');
		else addch('-');

		attroff(A_BOLD | COLOR_PAIR(color_pair));
	}

}

static void
NcursesUI_DrawVis(NcursesWindow_State* wdw)
{
	if (!wdw || !wdw->v) {
		return;
	}

	if (!is_playing(wdw)) {
		clear_visualization_area(wdw);
		return;
	}

	switch (wdw->vis) {
		case NCURSES_VIS_FFT:
			DrawFftVisualization(wdw);
			break;
		case NCURSES_VIS_SCOPE:
			DrawScopeVisualization(wdw);
			break;
		default:
			clear_visualization_area(wdw);
			break;
	}
}

static void
NcursesUI_DrawBrowser(NcursesWindow_State* wdw)
{
	if (!wdw || !wdw->ps || !wdw->ps->dir) {
		return;
	}

	int start_row = wdw->browser_y;
	size_t total = Directory_NTotal(wdw->ps->dir);


	for (int i = 0; i < wdw->max_items; i++) {
		size_t idx = i + wdw->ps->dir_ofs;
		if (idx >= total)
			break;
		bool isdir;
		const char* name = Directory_GetName(wdw->ps->dir, idx, &isdir);

		move(start_row + i, wdw->browser_x);

		// Draw indicator for current position
		if (i == 0) {
			attron(A_BOLD);
			addstr("-> ");
			attroff(A_BOLD);
		} else {
			addstr("   ");
		}

		// Draw file/directory name
		if (name && *name) {
			if (isdir) {
				attron(wdw->color_dir | A_BOLD);
				addstr(name);
				attroff(wdw->color_dir | A_BOLD);
			} else {
				attron(wdw->color_file);
				addstr(name);
				attroff(wdw->color_file);
			}
		}
		// If name is NULL or empty, show nothing (already cleared by clrtoeol)

		// Clear rest of line
		clrtoeol();
	}

	// Clear remaining browser area
	for (int i = total; i < wdw->max_items; i++) {
		move(start_row + i, wdw->browser_x);
		clrtoeol();
	}
}

static void
NcursesUI_DrawStatus(NcursesWindow_State* wdw)
{
	if (!wdw || !wdw->ps) {
		return;
	}

	int y = wdw->height - wdw->status_height;
	char tmp_str[32];  // " MM:SS/MM:SS\0" with room for large values

	// Draw status bar background
	attron(wdw->color_status);
	for (int row = y; row < wdw->height; row++) {
		move(row, 0);
		clrtoeol();
	}
	attroff(wdw->color_status);

	// Get song info - with NULL checks
	AudioManager* am = wdw->ps->am;
	AudioRenderer* ar = (am && am->active_ar) ? am->active_ar : NULL;

	const char* title = ar ? AudioRenderer_Title(ar) : NULL;
	const char* info = ar ? AudioRenderer_Info(ar) : NULL;
	float song_pos = ar ? AudioRenderer_PlayTime(ar) : 0.0f;
	float song_len = ar ? AudioRenderer_Length(ar) : 0.0f;
	int ntracks = ar ? AudioRenderer_NTracks(ar) : 0;
	int track = ar ? AudioRenderer_Track(ar) : 0;

	// Draw time on the right first (so we know how much space is left for title)
	attron(wdw->color_time);
	snprintf(tmp_str, sizeof(tmp_str), " %02d:%02d/%02d:%02d",
	         (int)(song_pos / 60.f), ((int)song_pos) % 60,
	         (int)(song_len / 60.f), ((int)song_len) % 60);
	int time_len = strlen(tmp_str);

	// Ensure time display fits
	int time_x = wdw->width - time_len - 2;
	if (time_x < 0) time_x = 0;

	move(y, time_x);
	addstr(tmp_str);
	attroff(wdw->color_time);

	// Draw title on the left, truncating to avoid overlap with time
	move(y, 2);
	attron(wdw->color_title | A_BOLD);
	if (title) {
		// Reserve space for time display + track info + margins
		int reserved_space = time_len + 10;  // time + track info + padding
		size_t maxchar = (reserved_space < wdw->width - 4) ?
		                 (wdw->width - reserved_space - 4) : 10;
		size_t title_len = strlen(title);
		if (title_len >= maxchar) {
			char truncated[MODP_STR_LENGTH];
			// Ensure we have room for "..." + null terminator
			size_t trunc_len = (maxchar > 4) ? (maxchar - 4) : 0;
			snprintf(truncated, MODP_STR_LENGTH, "%.*s...", (int)trunc_len, title);
			addstr(truncated);
		} else {
			addstr(title);
		}
	}
	attroff(wdw->color_title | A_BOLD);

	// Draw track info for multi-track files (after title, before time)
	if (ntracks > 1) {
		char track_str[32];
		snprintf(track_str, sizeof(track_str), " [%02d/%02d]", track + 1, ntracks);
		int cur_y, cur_x;
		getyx(stdscr, cur_y, cur_x);
		(void)cur_y;
		if (cur_x + (int)strlen(track_str) < wdw->width - time_len - 2) {
			addstr(track_str);
		}
	}

	// Draw settings line - clear entire row first to prevent artifacts
	move(y + 1, 0);
	attron(wdw->color_status);
	clrtoeol();
	attroff(wdw->color_status);

	move(y + 1, 2);
	attron(wdw->color_status);

	draw_status_toggle("F1:inc/", wdw->ps->auto_inc, wdw);
	addstr(" ");
	draw_status_toggle("F2:rnd/", wdw->ps->auto_rnd, wdw);

	// Draw F3/F4:mlth/000
	addstr(" F3/F4:mlth/");
	attron(wdw->color_status_value | A_BOLD);
	snprintf(tmp_str, sizeof(tmp_str), "%03d", wdw->ps->min_length);
	addstr(tmp_str);
	attroff(wdw->color_status_value | A_BOLD);
	attron(wdw->color_status);

	// Draw F5:vis/fft
	addstr(" F5:vis/");
	attron(wdw->color_status_vis | A_BOLD);
	const char* vis_name = wdw->vis == NCURSES_VIS_FFT ? "fft" :
	                     wdw->vis == NCURSES_VIS_SCOPE ? "scope" : "none";
	addstr(vis_name);
	attroff(wdw->color_status_vis | A_BOLD);

	attroff(wdw->color_status);

	// Draw info if available, but only if it doesn't overlap with settings
	if (info) {
		int cur_y, cur_x;
		getyx(stdscr, cur_y, cur_x);
		(void)cur_y;
		int info_len = strlen(info);
		if (cur_x + 4 + info_len < wdw->width) {
			move(y + 1, wdw->width - info_len - 2);
			attron(wdw->color_status);
			addstr(info);
			attroff(wdw->color_status);
		}
	}
}

void
NcursesUI_Draw(NcursesWindow_State* wdw)
{
	if (!wdw || !wdw->v) return;

	erase();

	Vis_Update(wdw);
	NcursesUI_DrawVis(wdw);
	NcursesUI_DrawBrowser(wdw);
	NcursesUI_DrawStatus(wdw);

	refresh();
}

static void
NcursesWindow_RecalcLayout(NcursesWindow_State* wdw)
{
	if (!wdw) return;

	getmaxyx(stdscr, wdw->height, wdw->width);

	wdw->status_height = 2;
	// Vis area scales with terminal height but cap it so browser always has room
	wdw->vis_height = wdw->height - wdw->status_height - 4;
	if (wdw->vis_height < 2) wdw->vis_height = 2;
	if (wdw->vis_height > 16) wdw->vis_height = 16;
	wdw->browser_height = wdw->height - wdw->vis_height - wdw->status_height;
	if (wdw->browser_height < 1) wdw->browser_height = 1;
	wdw->max_items = wdw->browser_height;
	wdw->browser_y = wdw->vis_height;
	wdw->browser_x = 0;
}

void
NcursesWindow_HandleKey(NcursesWindow_State* wdw, int ch)
{
	switch (ch) {
		case KEY_DOWN:
			Player_AlterOffset(wdw->ps, 1);
			break;
		case KEY_UP:
			Player_AlterOffset(wdw->ps, -1);
			break;
		case KEY_RIGHT:
			Player_AlterSubTrack(wdw->ps, 1);
			break;
		case KEY_LEFT:
			Player_AlterSubTrack(wdw->ps, -1);
			break;
		case KEY_NPAGE:
			Player_AlterOffset(wdw->ps, wdw->max_items);
			break;
		case KEY_PPAGE:
			Player_AlterOffset(wdw->ps, -wdw->max_items);
			break;
		case KEY_HOME:
			Player_Home(wdw->ps);
			break;
		case KEY_END:
			Player_End(wdw->ps);
			break;
		case '\n':
		case '\r':  // Carriage return - some terminals send this instead of \n
		case KEY_ENTER:
			Player_Perform(wdw->ps);
			break;
		case KEY_BACKSPACE:
		case KEY_DEL:
			Player_DirUp(wdw->ps);
			break;
		case ' ':
			Player_PlayPause(wdw->ps);
			break;
		case 'n':
		case 'N':
			Player_PlayNext(wdw->ps, true);
			break;
		case 'p':
		case 'P':
			Player_PlayPrev(wdw->ps, true);
			break;
		case 'r':
		case 'R':
			Player_PlayRandom(wdw->ps);
			break;
		case KEY_F(1):
			Player_ToggleAutoInc(wdw->ps);
			break;
		case KEY_F(2):
			Player_ToggleAutoRnd(wdw->ps);
			break;
		case KEY_F(3):
			Player_AlterMinLength(wdw->ps, -15);
			break;
		case KEY_F(4):
			Player_AlterMinLength(wdw->ps, 15);
			break;
		case KEY_F(5):
			if (++wdw->vis > NCURSES_VIS_NONE)
				wdw->vis = NCURSES_VIS_FFT;
			break;
		default:
			break;
	}
}

bool
NcursesWindow_ProcessEvents(NcursesWindow_State* wdw, bool* got_input)
{
	if (!wdw || !got_input) {
		return false;
	}

	*got_input = false;

	int ch;
	nodelay(stdscr, TRUE);

	ch = getch();
	if (ch != ERR) {
		*got_input = true;
		if (ch == KEY_RESIZE) {
			// Store old vis state
			Vis_State* old_v = wdw->v;
			int old_width = wdw->width;
			int old_height = wdw->height;

			// Recalculate layout first
			NcursesWindow_RecalcLayout(wdw);

			// Validate terminal size after resize
			if (wdw->width < MIN_TERMINAL_WIDTH || wdw->height < MIN_TERMINAL_HEIGHT) {
				// Terminal too small, restore old state
				wdw->width = old_width;
				wdw->height = old_height;
				return true;
			}

			// Create new vis state with new width
			wdw->v = Vis_Init(wdw->width, NCURSES_VIS_NSAMPLES);
			if (!wdw->v) {
				wdw->v = old_v;
				wdw->width = old_width;
				wdw->height = old_height;
				return true;
			}

			// Destroy old state after new one is created
			Vis_Destroy(old_v);
		} else if (ch == 'q' || ch == 'Q' || ch == KEY_ESC) {
			return false;
		} else {
			NcursesWindow_HandleKey(wdw, ch);
		}
	}

	return true;
}

void
NcursesWindow_Destroy(NcursesWindow_State* wdw)
{
	if (!wdw) return;

	// Clean up ncurses FIRST to prevent any further screen access
	endwin();

	// Now safe to destroy other resources
	Vis_Destroy(wdw->v);

	Vis_Cleanup();

	free(wdw);
}

NcursesWindow_State*
NcursesWindow_Init(Player_State* ps)
{
	NcursesWindow_State* wdw;

	if (!ps) {
		return NULL;
	}

	wdw = (NcursesWindow_State*) calloc(1, sizeof(NcursesWindow_State));
	if (!wdw) {
		return NULL;
	}

	wdw->ps = ps;

	// Seed random number generator for visualization colors
	srand((unsigned int)time(NULL));

	// Initialize ncurses
	initscr();
	cbreak();
	noecho();
	// nonl(); - removed as it can interfere with Enter key on some terminals
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	curs_set(0); // Hide cursor

	// Check terminal size
	getmaxyx(stdscr, wdw->height, wdw->width);
	if (wdw->width < MIN_TERMINAL_WIDTH || wdw->height < MIN_TERMINAL_HEIGHT) {
		endwin();
		fprintf(stderr, "Terminal too small. Minimum: %dx%d (current: %dx%d)\n",
		        MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT, wdw->width, wdw->height);
		free(wdw);
		return NULL;
	}

	// Initialize colors
	InitColorPairs(wdw);

	// Set up layout
	NcursesWindow_RecalcLayout(wdw);

	// Initialize visualization
	wdw->vis = NCURSES_VIS_FFT;
	wdw->v = Vis_Init(wdw->width, NCURSES_VIS_NSAMPLES);
	if (!wdw->v) {
		endwin();
		free(wdw);
		return NULL;
	}

	return wdw;
}
