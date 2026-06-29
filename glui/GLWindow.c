// Copyright intealls
// License: GPL v3

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <fftw3.h>

#include "GLWindow.h"

/* ── Visualization constants ─────────────────────────────────────────── */

#define WF_COLS                32
#define WF_ROWS                16
#define WF_TEXTURE_TOP         0.025f
#define WF_TEXTURE_RANGE       0.024f
#define WF_TX_SCALE            0.1f
#define WF_TY_SCALE            0.01f
#define WF_DS_SCALE            0.01f
#define INT16_MAX_F            65536.f

#define SCOPE_SCALE            192.f
#define SCOPE_HALF_W           5
#define SCOPE_STEP             1
#define SCOPE_OFFSET           3

#define REACTIVE_DECAY         0.95f
#define REACTIVE_BLEND         0.25f
#define PEAK_DECAY             0.999f

#define MAX_ENERGY             (65536.f * 8.f)

#define SONG_INFO_TRAIL_CHARS  14

#define STATUS_BAR_CHAR_COUNT  43
#define STATUS_BAR_ZOOM        3

#define MAX_SCROLL_LINES       5

#define SCROLL_ROWS            1

#define STAR_SPEED_MIN         1
#define STAR_SPEED_MAX         3
#define STAR_SIZE_MAX          5
#define STAR_FLIP              2
#define STAR_PHASE_INC_MAX     2
#define STAR_ROT_INC_MAX       2

/* FFT / visualization parameters */
#define VIS_NSAMPLES           384
#define VIS_NSTARS             100

/* Zoom levels used across the UI */
#define ZOOM_BROWSER           2
#define ZOOM_STATUS            3
#define ZOOM_SONG              3

/* Mouse wheel scroll clamp */
#define MOUSE_SCROLL_CLAMP     5

/* ── Helpers ─────────────────────────────────────────────────────────── */

#include "Font.h"
#include "GL.h"
#include "GLColors.h"
#include "Player.h"
#include "Globals.h"
#include "Utils.h"

/* ── Forward declarations ────────────────────────────────────────────── */

static void Vis_Destroy(Vis_State* v);

/* ── Options Editor data structures ──────────────────────────────────── */

typedef struct OptionEntry {
	const char* name;         /* Display name (left-aligned) */
	const char* description;  /* Short description below name */
	float min_val, max_val;
} OptionEntry;

struct OptionsEditor {
	bool active;
	size_t selected;          /* Index into option list */
	size_t scroll;            /* Scroll offset for the list */
};

/* ── Options list (8 entries) ───────────────────────────────────────── */

static OptionEntry option_list[] = {
	/* Background color */
	{ "BG Color R",       "Background red (0.0-1.0)",  0.0f, 1.0f },
	{ "BG Color G",       "Background green (0.0-1.0)", 0.0f, 1.0f },
	{ "BG Color B",       "Background blue (0.0-1.0)",  0.0f, 1.0f },

	/* Font effects */
	{ "Shake Factor",     "Text shake synced to music", 0.0f, 100.0f },
	{ "Zoom Factor",      "Text zoom synced to music",  0.0f, 100.0f },
	{ "Rotation Factor",  "Text rotation synced to music", 0.0f, 100.0f },

	/* Visualization effects */
	{ "Perturb Factor",   "Spectrogram perturbation", 0.0f, 100.0f },
	{ "BG Flash Factor",  "Background flash on energy", 0.0f, 100.0f },

	/* Circular vis effects */
	{ "Circ Pulse",       "Circle expands with energy", 0.0f, 100.0f },
	{ "Circ Spin",        "Circle spin reactivity", 0.0f, 100.0f },
};

#define NUM_OPTIONS (sizeof(option_list) / sizeof(option_list[0]))

/* Edit targets: maps option index → (float* ptr, min, max from option_list) */
typedef struct EditTarget {
	float* ptr;
	float min_val, max_val;
} EditTarget;

static EditTarget
get_edit_target(GLWindow_State* wdw, size_t idx)
{
	assert(idx < NUM_OPTIONS);
	EditTarget et;
	et.min_val = option_list[idx].min_val;
	et.max_val = option_list[idx].max_val;

	switch (idx) {
		case 0: et.ptr = &wdw->opts->ui.clr[0]; break;
		case 1: et.ptr = &wdw->opts->ui.clr[1]; break;
		case 2: et.ptr = &wdw->opts->ui.clr[2]; break;
		case 3: et.ptr = &wdw->opts->ui.font_shake_factor; break;
		case 4: et.ptr = &wdw->opts->ui.font_zoom_factor; break;
		case 5: et.ptr = &wdw->opts->ui.font_rotation_factor; break;
		case 6: et.ptr = &wdw->opts->ui.perturb_waterfall_factor; break;
		case 7: et.ptr = &wdw->opts->ui.bg_flash_factor; break;
		case 8: et.ptr = &wdw->opts->ui.circ_pulse_factor; break;
		case 9: et.ptr = &wdw->opts->ui.circ_spin_factor; break;
	}
	return et;
}

/* ── Options Editor functions ────────────────────────────────────────── */

static void
GLUI_DrawOptionsEditor(GLWindow_State* wdw);

static void
GLWindow_OptionsEditor_HandleKey(GLWindow_State* wdw, SDL_Keysym* keysym);

static void
GLWindow_OptionsEditor_Init(OptionsEditor* ed);

static void
GLWindow_OptionsEditor_Destroy(OptionsEditor* ed);

/* ── Options Editor implementation ───────────────────────────────────── */

static void
GLWindow_OptionsEditor_Init(OptionsEditor* ed)
{
	ed->active = false;
	ed->selected = 0;
	ed->scroll = 0;
}

static void
GLWindow_OptionsEditor_Destroy(OptionsEditor* ed)
{
	(void)ed;
}

static void
GLUI_DrawOptionsEditor(GLWindow_State* wdw)
{
	OptionsEditor* ed = wdw->editor;
	if (!ed || !ed->active)
		return;

	int fw = wdw->font->font_width;
	int fh = wdw->font->font_height;
	int text_zoom = 2;

	/* Overlay dimensions — fit to actual option count */
	int overlay_w = 72 * fw * text_zoom;
	int overlay_h = (NUM_OPTIONS + 3) * fh * text_zoom;  /* title + separator + options + footer */
	int ox = (int)((float)wdw->width * 0.2f) - overlay_w / 2;
	int oy = (int)wdw->height / 2 - overlay_h / 2;

	/* Clamp if overlay is larger than window */
	if (overlay_w > (int)wdw->width) { ox = 0; overlay_w = (int)wdw->width; }
	if (overlay_h > (int)wdw->height) { oy = 0; overlay_h = (int)wdw->height; }

	/* Overlay top in OpenGL coords (y=0 at bottom) */
	int gl_oy = wdw->height - oy;

	/* Dark semi-transparent background — derived from user's BG color */
	glColor4ub(
	    (uint8_t)(wdw->opts->ui.clr[0] * 32),
	    (uint8_t)(wdw->opts->ui.clr[1] * 32),
	    (uint8_t)(wdw->opts->ui.clr[2] * 32),
	    220);
	GL_DrawRec(ox, gl_oy, overlay_w, overlay_h, true, (int)wdw->width, (int)wdw->height);

	/* Title — at top of overlay */
	Font_DrawString(wdw, "\\00ddffffOptions Editor",
	                ox + fw * text_zoom,
	                gl_oy - fh * text_zoom, 2 * text_zoom);

	/* Separator line — below title */
	glColor4ub(100, 100, 150, 180);
	GL_DrawRec(ox, gl_oy - fh * text_zoom * 2,
	           overlay_w, 1, false, (int)wdw->width, (int)wdw->height);

	/* Options list (scrollable) */
	int list_y = gl_oy - fh * text_zoom * 3;
	size_t visible_rows = (size_t)((overlay_h - fh * text_zoom * 3) / (fh * text_zoom));
	if (visible_rows < 2)
		visible_rows = 2;
	if (visible_rows > NUM_OPTIONS)
		visible_rows = NUM_OPTIONS;

	/* Clamp scroll */
	size_t max_scroll = NUM_OPTIONS > visible_rows ? NUM_OPTIONS - visible_rows : 0;
	if (ed->scroll > max_scroll)
		ed->scroll = max_scroll;

	for (size_t i = 0; i < visible_rows && (ed->scroll + i) < NUM_OPTIONS; i++) {
		size_t idx = ed->scroll + i;
		const OptionEntry* oe = &option_list[idx];

		int y = list_y - (int)(i * fh * text_zoom);
		bool is_selected = (idx == ed->selected);

		/* Option name */
		const char* name_color = is_selected ? "\\00ddffff" : "\\ccccccff";
		Font_DrawString(wdw, name_color, ox + fw * text_zoom, y, text_zoom);
		Font_DrawString(wdw, oe->name, ox + fw * text_zoom * 2, y, text_zoom);

		/* Selection indicator */
		if (is_selected)
			Font_DrawString(wdw, "\\ffffffff>", ox + fw * text_zoom, y, text_zoom);

		/* Description */
		if (is_selected) {
			int desc_y = y - fh * text_zoom;
			char desc_buf[256];
			snprintf(desc_buf, sizeof(desc_buf), "\\777777ff%s", oe->description);
			Font_DrawString(wdw, desc_buf,
			                ox + fw * text_zoom * 2,
			                desc_y, text_zoom);
		}

		/* Current value — read from GLWindow_State for immediate feedback */
		if (is_selected) {
			char val_str[64];
			EditTarget et = get_edit_target(wdw, idx);
			if (idx < 3)
				snprintf(val_str, sizeof(val_str), "%.2f", *et.ptr);
			else
				snprintf(val_str, sizeof(val_str), "%.1f", *et.ptr);

			/* Right-align value within overlay */
			int val_pixel_w = (int)strlen(val_str) * fw * text_zoom;
			int val_x = ox + overlay_w - fw * text_zoom * 2 - val_pixel_w;

			Font_DrawString(wdw, "\\dddd00ff", val_x, y, text_zoom);
			Font_DrawString(wdw, val_str, val_x, y, text_zoom);
		}
	}

	/* Footer: instructions — at bottom of overlay */
	int footer_y = gl_oy - overlay_h + fh * text_zoom;
	Font_DrawString(wdw, "\\777777ffEsc:close", ox + fw * text_zoom, footer_y, text_zoom);
}

static void
GLWindow_OptionsEditor_HandleKey(GLWindow_State* wdw, SDL_Keysym* keysym)
{
	OptionsEditor* ed = wdw->editor;
	if (!ed || !ed->active)
		return;

	if (keysym->sym == SDLK_ESCAPE) {
		ed->active = false;
		return;
	}

	/* Left/right adjust selected value by 0.05 */
	if (keysym->sym == SDLK_LEFT || keysym->sym == SDLK_RIGHT) {
		EditTarget et = get_edit_target(wdw, ed->selected);
		float delta = keysym->sym == SDLK_LEFT ? -0.05f : 0.05f;
		float new_val = *et.ptr + delta;
		if (new_val < et.min_val) new_val = et.min_val;
		if (new_val > et.max_val) new_val = et.max_val;
		*et.ptr = new_val;
	}

	/* Up/down navigate */
	switch (keysym->sym) {
		case SDLK_UP:
			if (ed->selected > 0) {
				ed->selected--;
				if (ed->selected < ed->scroll)
					ed->scroll = ed->selected;
			}
			break;
		case SDLK_DOWN:
			if (ed->selected + 1 < NUM_OPTIONS) {
				ed->selected++;
				int fh = wdw->font->font_height * 2;
				size_t visible = (size_t)((wdw->height - fh * 3) / fh);
				if (visible < 2) visible = 2;
				if (ed->selected >= ed->scroll + visible)
					ed->scroll = ed->selected - visible + 1;
				size_t max_scroll = NUM_OPTIONS > visible ? NUM_OPTIONS - visible : 0;
				if (ed->scroll > max_scroll)
					ed->scroll = max_scroll;
			}
			break;
	}
}

/* ── Initialization ──────────────────────────────────────────────────── */

static Vis_State*
Vis_Init(size_t wdw_width, size_t wdw_height, size_t nsamples, size_t nstars)
{
	Vis_State* v = (Vis_State*) calloc(1, sizeof(Vis_State));
	assert(v);

	v->fft_len = wdw_width;
	v->nsamples = nsamples;

	v->vis_len = wdw_width * 2;
	v->vis_buf = (T*) calloc(v->vis_len, sizeof(T));
	assert(v->vis_buf);

	v->window = (float*) calloc(v->fft_len, sizeof(float));
	assert(v->window);

	v->signal = (float*) calloc(v->fft_len, sizeof(float));
	assert(v->signal);

	v->spectrum = (float*) calloc(v->fft_len, sizeof(float));
	assert(v->spectrum);

	v->result = (fftwf_complex*) calloc(v->fft_len, sizeof(fftwf_complex));
	assert(v->result);

	/* Circular FFT linearized spectrum buffer */
	v->lin_cap = v->fft_len / 2;
	v->lin = (float*)calloc(v->lin_cap, sizeof(float));
	assert(v->lin);

	/* Blackman-Nuttall window (B=1.9761), ~100dB sidelobe attenuation
	 * from Wikipedia */
	for (size_t i = 0; i < nsamples; i++) {
		v->window[i] = 0.3635819 - 0.4891775 * cos(2 * M_PI * i / (nsamples - 1)) +
		               0.1365995 * cos(4 * M_PI * i / (nsamples - 1)) -
		               0.0106411 * cos(6 * M_PI * i / (nsamples - 1));
	}

	v->plan = fftwf_plan_dft_r2c_1d(v->fft_len,
	                                v->signal,
	                                v->result,
	                                FFTW_ESTIMATE);

	v->stars = calloc(nstars, sizeof(Star));
	assert(v->stars);

	v->nstars = nstars;

	for (size_t i = 0; i < nstars; i++) {
		v->stars[i].speed_x = 0;
		v->stars[i].speed_y = rand() % (STAR_SPEED_MAX - STAR_SPEED_MIN + 1) + STAR_SPEED_MIN;
		v->stars[i].xpos = rand() % wdw_width;
		v->stars[i].ypos = rand() % wdw_height;
		v->stars[i].size = rand() % STAR_SIZE_MAX;
		v->stars[i].in_front = rand() % STAR_FLIP;
		v->stars[i].phase = 0;
		v->stars[i].phase_inc = rand() % STAR_PHASE_INC_MAX + 1;
		v->stars[i].rotation = 0;
		v->stars[i].rotation_inc = rand() % STAR_ROT_INC_MAX + 1;
		v->stars[i].visible = false;
	}

	/* Waterfall (spectrogram) buffer + OpenGL texture */
	v->wf_height = wdw_height;
	v->wf_width  = wdw_width / 2;  /* only positive half of FFT matters */
	v->wf_buf = (unsigned char*) calloc(v->wf_width * v->wf_height * 4, 1);
	assert(v->wf_buf);

	glGenTextures(1, &v->wf_tex);
	glBindTexture(GL_TEXTURE_2D, v->wf_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
	            (GLsizei)v->wf_width, (GLsizei)v->wf_height,
	            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glBindTexture(GL_TEXTURE_2D, 0);

	v->circ_rotation = 0.f;
	v->circ_prev_energy = 0.f;

	return v;
}

/* ── FFT computation ─────────────────────────────────────────────────── */

static void
Vis_ComputeFFT(Vis_State* v, const T* vis_buf, size_t vis_len)
{
	(void)vis_len;

	for (size_t i = 0; i < v->nsamples * 2; i += 2)
		v->signal[i / 2] = (vis_buf[i] + vis_buf[i + 1]) / 2.f * v->window[i / 2];

	fftwf_execute(v->plan);
}

/* ── Reactive color update ───────────────────────────────────────────── */

static void
Vis_UpdateReactiveColor(Vis_State* v)
{
	v->mean_energy_band_div16 = 0.f;

	size_t mid_start   = v->fft_len / 8;
	size_t treble_start = v->fft_len / 3;

	float bass = 0, mids = 0, treble = 0;
	size_t bass_n = 0, mids_n = 0, treble_n = 0;

	for (size_t i = 0; i < v->fft_len; i++) {
		float re = v->result[i][0];
		float im = v->result[i][1];
		float energy = sqrt(re * re + im * im);

		v->spectrum[i] = log10(energy);

		if (i > 16 && i < (v->fft_len / 16) + 16)
			v->mean_energy_band_div16 += energy;

		/* Accumulate band energy for reactive background (positive half only). */
		if (i >= 2 && i < v->fft_len / 2) {
			float e = re * re + im * im;
			if (i < mid_start)          { bass  += e; bass_n++; }
			else if (i < treble_start)  { mids  += e; mids_n++; }
			else                        { treble += e; treble_n++; }
		}
	}

	v->mean_energy_band_div16 /= v->fft_len / 16;

	/* Track peak energy with slow decay for normalization */
	if (v->mean_energy_band_div16 > v->peak_energy)
		v->peak_energy = v->mean_energy_band_div16;
	else
		v->peak_energy *= PEAK_DECAY;
	if (v->peak_energy < 1.f) v->peak_energy = 1.f;

	if (bass_n)   bass /= bass_n;
	if (mids_n)   mids /= mids_n;
	if (treble_n) treble /= treble_n;

	/* Normalize each band by the max across all three. */
	float max_e = bass;
	if (mids > max_e)   max_e = mids;
	if (treble > max_e) max_e = treble;
	if (max_e > 0) {
		v->reactive_color[0] = bass / max_e;
		v->reactive_color[1] = mids / max_e;
		v->reactive_color[2] = treble / max_e;
	}
}

/* ── Waterfall scroll + upload ───────────────────────────────────────── */

static void
Vis_ScrollWaterfall(Vis_State* v)
{
	size_t row_stride = v->wf_width * 4;  /* RGBA */
	size_t scroll_rows = SCROLL_ROWS < v->wf_height ? SCROLL_ROWS : v->wf_height - 1;
	size_t scroll_bytes = row_stride * (v->wf_height - scroll_rows);

	/* Shift all existing rows down by scroll_rows (push toward bottom) */
	memmove(v->wf_buf + scroll_rows * row_stride, v->wf_buf, scroll_bytes);

	/* Find max spectrum value for normalization (use positive half) */
	float max_spec = 0;
	for (size_t i = 0; i < v->wf_width && i < v->fft_len / 2; i++)
		if (v->spectrum[i] > max_spec)
			max_spec = v->spectrum[i];

	/* Encode each new row at the top with slight fade downward. */
	for (size_t r = 0; r < scroll_rows; r++) {
		size_t row_idx = r;
		unsigned char* row = v->wf_buf + row_idx * row_stride;
		float fade = 1.f - (float) r / (float) scroll_rows;  /* brightest at top */

		for (size_t i = 0; i < v->wf_width && i < v->fft_len / 2; i++) {
			float t = max_spec > 0 ? v->spectrum[i] / max_spec : 0.f;
			t *= fade;
			if (t < 0.f) t = 0.f;
			if (t > 1.f) t = 1.f;

			/* Hot-iron colormap with alpha: low intensity → transparent */
			unsigned char a = (unsigned char)(t * 255.f);  /* alpha tracks intensity */
			if (t < 0.333f) {
				*row++ = (unsigned char)(t * 3.f * 255.f); /* R ramps up */
				*row++ = 0;                                /* G off */
				*row++ = 0;                                /* B off */
				*row++ = a;                                /* A */
			} else if (t < 0.666f) {
				*row++ = 255;                              /* R full */
				*row++ = (unsigned char)((t - 0.333f) * 3.f * 255.f); /* G ramps */
				*row++ = 0;                                /* B off */
				*row++ = a;                                /* A */
			} else {
				*row++ = 255;                              /* R full */
				*row++ = 255;                              /* G full */
				*row++ = (unsigned char)((t - 0.666f) * 3.f * 255.f); /* B ramps */
				*row++ = a;                                /* A */
			}
		}
	}

	/* Upload updated texture to GPU */
	glBindTexture(GL_TEXTURE_2D, v->wf_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
	                (GLsizei)v->wf_width, (GLsizei)v->wf_height,
	                GL_RGBA, GL_UNSIGNED_BYTE, v->wf_buf);
	glBindTexture(GL_TEXTURE_2D, 0);
}

/* ── Reactive color decay ────────────────────────────────────────────── */

static void
Vis_DecayReactiveColor(Vis_State* v)
{
	for (int c = 0; c < 3; c++)
		v->reactive_color[c] *= REACTIVE_DECAY;
}

/* ── Destruction ─────────────────────────────────────────────────────── */

static void
Vis_Destroy(Vis_State* v)
{
	assert(v);

	fftwf_destroy_plan(v->plan);

	free(v->vis_buf);
	free(v->signal);
	free(v->spectrum);
	free(v->window);
	free(v->result);
	free(v->stars);
	free(v->wf_buf);
	free(v->lin);
	glDeleteTextures(1, &v->wf_tex);
	free(v);
}

/* ── Main vis update (dispatches sub-functions) ──────────────────────── */

static void
Vis_Update(GLWindow_State* wdw)
{
	Vis_State* v = wdw->v;

	if (wdw->ps->am->playing) {
		Player_GetPlaybackData(wdw->ps, v->vis_buf, v->vis_len, true);
		Vis_ComputeFFT(v, v->vis_buf, v->vis_len);
		Vis_UpdateReactiveColor(v);
		Vis_ScrollWaterfall(v);
	} else {
		Vis_DecayReactiveColor(v);
		/* Decay energy so shake/zoom/perturb/flash stop when paused */
		v->mean_energy_band_div16 *= 0.9f;
	}
}

/* ── Star update (position, visibility, animation) ───────────────────── */

static void
Vis_UpdateStars(Vis_State* v, const GLWindow_State* wdw)
{
	const bool playing = wdw->ps->am->playing;

	for (size_t i = 0; i < v->nstars; i++) {
		Star* s = &v->stars[i];

		if (playing) {
			s->xpos += s->speed_x;
			s->ypos += s->speed_y;
			s->phase = (s->phase + s->phase_inc) % 360;
			s->rotation = (s->rotation + s->rotation_inc) % 360;
		}

		if (s->ypos >= (int)wdw->height) {
			s->visible = playing;
			s->xpos = rand() % wdw->width;
			s->ypos = 0;
		}

		s->xpos %= wdw->width;
		s->ypos %= wdw->height;
	}
}

/* ── Star rendering ──────────────────────────────────────────────────── */

static void
GLUI_DrawStars(GLWindow_State* wdw, bool in_front)
{
	Star* stars = wdw->v->stars;

	GL_OrthoOn(wdw->width, wdw->height);
	glDisable(GL_TEXTURE_2D);

	for (size_t i = 0; i < wdw->v->nstars; i++) {
		if (!stars[i].in_front && in_front)
			continue;
		if (!stars[i].visible)
			continue;

		int xpos = stars[i].xpos + stars[i].size * sin(stars[i].phase * M_PI / 180);
		int ypos = stars[i].ypos;

		glPushMatrix();
		glColor4f(1, 1, 0, (float)rand() / (float)RAND_MAX);  /* alpha flicker */
		glTranslatef(xpos, ypos, 0);
		glRotatef(stars[i].rotation, 0, 0, 1);
		glBegin(GL_QUADS);
		{
			glVertex2i(-stars[i].size / 2, -stars[i].size / 2);
			glVertex2i(stars[i].size / 2, -stars[i].size / 2);
			glVertex2i(stars[i].size / 2, stars[i].size / 2);
			glVertex2i(-stars[i].size / 2, stars[i].size / 2);
		}
		glEnd();
		glPopMatrix();
	}
	glEnable(GL_TEXTURE_2D);
	GL_OrthoOff();
}

/* ── Browser hit-testing ─────────────────────────────────────────────── */

/* Convert SDL mouse Y to browser item index, or -1 if not over browser. */
static int
browser_item_at_mouse_y(const GLWindow_State* wdw)
{
	int gl_y = wdw->height - wdw->mouse_y;
	int browser_bottom = wdw->layout_browser_y;
	int browser_top = wdw->layout_browser_y - (int)wdw->max_items * wdw->layout_item_height;

	if (gl_y <= browser_top || gl_y >= browser_bottom)
		return -1;

	int idx = (browser_bottom - gl_y - 1) / wdw->layout_item_height;
	if (idx < 0 || idx >= (int)wdw->max_items)
		return -1;
	return idx;
}

/* ── Scope rendering ─────────────────────────────────────────────────── */

/* Linearly interpolate vis_buf at a floating-point index.
 * vis_buf contains interleaved stereo samples; each call returns one sample. */
static inline float
scope_sample(const Vis_State* v, float fidx)
{
	size_t i0 = (size_t)fidx;
	size_t i1 = i0 + 1;
	if (i1 >= v->vis_len)
		i1 = v->vis_len - 1;
	float frac = fidx - (float)i0;
	return (1.f - frac) * (float)v->vis_buf[i0] + frac * (float)v->vis_buf[i1];
}

/* ── Scope render helpers ─────────────────────────────────────────────── */

typedef struct ScopeDrawParams {
	void (*expand_fn)(float *y0, float *y1, float expand);
	int  offset;
	void (*color_fn)(GLWindow_State* wdw, size_t i);
} ScopeDrawParams;

static inline void
scope_expand_symmetric(float *y0, float *y1, float expand)
{
	*y0 -= expand / 2.f;
	*y1 += expand / 2.f;
}

static inline void
scope_expand_toward_higher(float *y0, float *y1, float expand)
{
	if (*y0 < *y1) {
		*y0 -= expand / 2.f;
		*y1 += expand / 2.f;
	} else {
		*y0 += expand / 2.f;
		*y1 -= expand / 2.f;
	}
}

static void
scope_color_black(GLWindow_State* wdw, size_t i)
{
	(void)wdw; (void)i;
	glColor4ub(0, 0, 0, 255);
}

static void
scope_color_filled(GLWindow_State* wdw, size_t i)
{
	glColor4ub(255, (int)((float)(i / 2) * 255.f / (float)wdw->width * 2), 0, 255);
}

static void
draw_scope(GLWindow_State* wdw, const ScopeDrawParams* p)
{
	Vis_State* v = wdw->v;
	size_t n = (size_t)wdw->width * 2;
	if (n < 4) return;

	float scale = SCOPE_SCALE;
	float vis_scale = (float)v->vis_len / (float)(wdw->width * 2);
	int offset = p->offset;

	glBegin(GL_QUADS);
	for (size_t i = 0; i < n - 2; i += 2) {
		float src_i = (float)i * vis_scale;
		float src_i2 = (float)(i + 2) * vis_scale;

		float point = (scope_sample(v, src_i) + scope_sample(v, src_i + 1)) / scale;
		float next_point = (scope_sample(v, src_i2) + scope_sample(v, src_i2 + 1)) / scale;

		float y0 = wdw->height / 2.f + point;
		float y1 = wdw->height / 2.f + next_point;
		float expand = 2.f - fabs(next_point - point);

		if (expand > 0)
			p->expand_fn(&y0, &y1, expand);

		p->color_fn(wdw, i);
		glVertex2i((int)i - SCOPE_HALF_W + offset, (int)(y0 - offset));
		glVertex2i((int)i + SCOPE_HALF_W + offset, (int)(y0 - offset));
		glVertex2i((int)i + SCOPE_HALF_W + SCOPE_STEP + offset, (int)(y1 - offset));
		glVertex2i((int)i - SCOPE_HALF_W + SCOPE_STEP + offset, (int)(y1 - offset));
	}
	glEnd();
}

static const ScopeDrawParams scope_outline_params = {
	.expand_fn = scope_expand_symmetric,
	.offset    = SCOPE_OFFSET,
	.color_fn  = scope_color_black,
};

static const ScopeDrawParams scope_filled_params = {
	.expand_fn = scope_expand_toward_higher,
	.offset    = 0,
	.color_fn  = scope_color_filled,
};

static void
draw_scope_outline(GLWindow_State* wdw)
{
	draw_scope(wdw, &scope_outline_params);
}

static void
draw_scope_filled(GLWindow_State* wdw)
{
	draw_scope(wdw, &scope_filled_params);
}

/* ── Circular (radial) FFT rendering ─────────────────────────────────── */

static void
draw_circular_fft(GLWindow_State* wdw)
{
	Vis_State* v = wdw->v;
	size_t n_bars = v->fft_len / 2 - 2;  /* use positive half of FFT */

	/* Reallocate if window was resized */
	if (n_bars > v->lin_cap) {
		v->lin = (float*)realloc(v->lin, n_bars * sizeof(float));
		assert(v->lin);
		v->lin_cap = n_bars;
	}

	/* Center and radii */
	float cx = (float)wdw->width / 2.f;
	float cy = (float)wdw->height / 2.f;
	float inner_r = fminf(wdw->width, wdw->height) * 0.3f;
	float outer_r = fminf(wdw->width, wdw->height) * 0.9f;

	/* Linearize spectrum: convert log10 → linear energy in one pass */
	for (size_t i = 0; i < n_bars; i++)
		v->lin[i] = powf(10.f, v->spectrum[i]);

	/* Normalize scale: find max energy in positive half */
	float max_energy = 0;
	for (size_t i = 0; i < n_bars; i++)
		if (v->lin[i] > max_energy)
			max_energy = v->lin[i];
	if (max_energy <= 0) max_energy = 1;

	float bar_scale = outer_r - inner_r;
	float base_r = 0.f;

	/* Energy pulse: scale entire circle by audio energy so it throbs with the beat */
	float energy_norm = v->mean_energy_band_div16 / MAX_ENERGY;
	float pulse = 1.f + energy_norm * wdw->opts->ui.circ_pulse_factor;
	bar_scale *= pulse;

	/* Rotation driven by energy change — forward on attack, backward on decay,
	 * still when energy is steady. */
	float energy_delta = energy_norm - v->circ_prev_energy;
	v->circ_prev_energy = energy_norm;
	v->circ_rotation += energy_delta * wdw->opts->ui.circ_spin_factor;

	GL_OrthoOn(wdw->width, wdw->height);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	size_t seg = n_bars > 0 ? n_bars : 1;
	float angle_step = 2.f * M_PI / (float)seg;

	/* Draw bars as solid triangles forming a filled circle.
	 * Each bar is a pair of triangles (a quad wedge) between bin[i] and bin[i+1],
	 * from the center outward to the energy-scaled height.
	 * This eliminates the wedge-shaped gaps between adjacent line strands and
	 * the empty hole at the center. */
	glBegin(GL_TRIANGLES);
	for (size_t i = 0; i < n_bars; i++) {
		size_t next = (i + 1) % n_bars;

		float a0 = (float)i * angle_step + M_PI + v->circ_rotation;
		float a1 = (float)next * angle_step + M_PI + v->circ_rotation;

		/* Linear suppression ramp for the lowest 10 FFT bins.
		 * DC (bin 0) at 5%, bin 9 at 100%, bins 10+ unscaled. */
		float freq_scale0 = i < 10 ? 0.05f + 0.95f * (float)i / 9.f : 1.f;
		float freq_scale1 = next < 10 ? 0.05f + 0.95f * (float)next / 9.f : 1.f;

		float spec0 = sqrtf(v->lin[i] / max_energy) * freq_scale0;
		float spec1 = sqrtf(v->lin[next] / max_energy) * freq_scale1;

		/* Color by average energy of this bar edge */
		float spec = (spec0 + spec1) * 0.5f;

		unsigned char r, g, b;

		/* Color by energy: low→high maps to cool→hot (blue → cyan → green → yellow → white) */
		if (spec < 0.25f) {
			float t = spec / 0.25f;
			r = (unsigned char)(30.f * t);
			g = (unsigned char)(60.f + 140.f * t);
			b = (unsigned char)(180.f + 75.f * t);
		} else if (spec < 0.5f) {
			float t = (spec - 0.25f) / 0.25f;
			r = (unsigned char)(30.f + 30.f * t);
			g = (unsigned char)(200.f + 55.f * t);
			b = (unsigned char)(255.f * (1.f - t));
		} else if (spec < 0.75f) {
			float t = (spec - 0.5f) / 0.25f;
			r = (unsigned char)(60.f + 195.f * t);
			g = (unsigned char)(255.f);
			b = (unsigned char)(255.f * (1.f - t));
		} else {
			float t = (spec - 0.75f) / 0.25f;
			r = (unsigned char)(255.f);
			g = (unsigned char)(255.f);
			b = (unsigned char)(255.f * t);
		}

		glColor4ub(r, g, b, 220);

		/* Inner (base) and outer (energy-scaled) endpoints for this bar's edges */
		float r0 = base_r + spec0 * bar_scale;
		float r1 = base_r + spec1 * bar_scale;

		float ix0 = cx + base_r * cosf(a0);
		float iy0 = cy + base_r * sinf(a0);
		float ix1 = cx + base_r * cosf(a1);
		float iy1 = cy + base_r * sinf(a1);
		float ox0 = cx + r0 * cosf(a0);
		float oy0 = cy + r0 * sinf(a0);
		float ox1 = cx + r1 * cosf(a1);
		float oy1 = cy + r1 * sinf(a1);

		/* Triangle 1: (inner0, inner1, outer0) */
		glVertex2f(ix0, iy0);
		glVertex2f(ix1, iy1);
		glVertex2f(ox0, oy0);

		/* Triangle 2: (inner1, outer1, outer0) */
		glVertex2f(ix1, iy1);
		glVertex2f(ox1, oy1);
		glVertex2f(ox0, oy0);
	}
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	GL_OrthoOff();
}

/* ── Visualization rendering ─────────────────────────────────────────── */

static void
GLUI_DrawVis(GLWindow_State* wdw)
{
	Vis_State* v = wdw->v;
	const bool playing = wdw->ps->am->playing;

	Vis_UpdateStars(v, wdw);
	GLUI_DrawStars(wdw, false);

	switch (wdw->vis) {
	case VIS_FFT:
		if (!playing)
			break;
		{
			float scale = wdw->height / log10(1 << 24);
			float bar_w = (float) wdw->width * 2.f / v->fft_len;

			glBindTexture(GL_TEXTURE_2D, 0);
			GL_OrthoOn(wdw->width, wdw->height);
			glBegin(GL_QUADS);
			{
				for (size_t i = 3; i < v->fft_len / 2 + 2; i += 2) {
					glColor4ub(0, 0, 0, 255);

					glVertex2f((i - 3) * bar_w + 8, (int) (v->spectrum[i - 2] * scale));
					glVertex2f((i - 1) * bar_w + 8, (int) (v->spectrum[i] * scale));

					glVertex2f((i - 1) * bar_w + 8, 0);
					glVertex2f((i - 3) * bar_w + 8, 0);

					glColor4ub(255, (int) ((float) i * 255.f / (float) v->fft_len), 0, 255);

					glVertex2f((i - 3) * bar_w, (int) (v->spectrum[i - 2] * scale));
					glVertex2f((i - 1) * bar_w, (int) (v->spectrum[i] * scale));

					glVertex2f((i - 1) * bar_w, 0);
					glVertex2f((i - 3) * bar_w, 0);
				}
			}
			glEnd();
			GL_OrthoOff();
		}
		break;

	case VIS_SCOPE:
		if (!playing)
			break;
		glBindTexture(GL_TEXTURE_2D, 0);
		GL_OrthoOn(wdw->width, wdw->height);
		draw_scope_outline(wdw);
		draw_scope_filled(wdw);
		GL_OrthoOff();
		break;

	case VIS_CIRCULAR:
		if (!playing)
			break;
		glBindTexture(GL_TEXTURE_2D, 0);
		draw_circular_fft(wdw);
		break;

	case VIS_WATERFALL:
		GL_OrthoOn(wdw->width, wdw->height);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, v->wf_tex);
		glColor4ub(255, 255, 255, 255);

		float energy_factor = v->mean_energy_band_div16 / INT16_MAX_F;
		float perturb = wdw->opts->ui.perturb_waterfall_factor;

		int status_h = wdw->font->font_height * 3;
		/* Waterfall spans from status bar bottom to song info top */
		int wf_y0 = wdw->layout_browser_y - wdw->layout_browser_height - status_h;
		int wf_y1 = wdw->layout_browser_y + wdw->font->font_height * 3;
		int wf_h = wf_y1 - wf_y0;  /* waterfall height (y-dimension) */

		/* Pixel-grid rendering: split waterfall into independently perturbed blocks
		 * Float-based cell sizing prevents integer truncation gaps at edges.
		 * More blocks + higher perturb = heavier pixel chaos */
		float cell_w_f = (float)wdw->width / WF_COLS;
		float cell_h_f = (float)wf_h / WF_ROWS;

		/* Find max log-spectrum for normalization (spectrum is log10, not linear) */
		float max_spec = 0;
		for (size_t i = 0; i < v->wf_width; i++)
			if (v->spectrum[i] > max_spec)
				max_spec = v->spectrum[i];
		if (max_spec <= 0) max_spec = 1.f;

		glBegin(GL_QUADS);
		{
			for (int r = 0; r < WF_ROWS; r++) {
				for (int c = 0; c < WF_COLS; c++) {
					/* Normalized position within waterfall area */
					float nx = (float) c / WF_COLS;
					float ny = (float) r / WF_ROWS;

					/* Sample local FFT energy for per-column reactivity */
					size_t spec_idx = (size_t)(nx * (v->wf_width - 1));
					float spec_norm = v->wf_width > 0 ? v->spectrum[spec_idx] / max_spec : 0.f;

					/* Combine global energy with local spectral energy */
					float energy = (energy_factor + spec_norm) * 0.5f;

					/* Per-cell random perturbation uses the square of the applied effect. */
					float jitter = energy * perturb;
					jitter *= jitter;
					float dx = ((((float)rand() / RAND_MAX) - 0.5f) * 2.f * jitter);
					float dy = ((((float)rand() / RAND_MAX) - 0.5f) * 2.f * jitter);
					float ds = (((float)rand() / RAND_MAX) * jitter) * WF_DS_SCALE;

					/* Cell screen coordinates with jitter (float throughout — no integer truncation) */
					float cx0 = c * cell_w_f + dx;
					float cy0 = (float)wf_y0 + r * cell_h_f + dy;
					float cx1 = (c + 1) * cell_w_f + dx;
					float cy1 = (float)wf_y0 + (r + 1) * cell_h_f + dy;

					/* Texture coordinates with jitter */
					float tx0 = nx;
					float ty0 = WF_TEXTURE_TOP - ny * WF_TEXTURE_RANGE;
					float tx1 = (float)(c + 1) / WF_COLS;
					float ty1 = WF_TEXTURE_TOP - (float)(r + 1) / WF_ROWS * WF_TEXTURE_RANGE;

					/* Perturb texture coords — creates chromatic scrambling at high energies */
					float dtx = dx / (float)wdw->width * WF_TX_SCALE;
					float dty = dy / (float)wf_h * WF_TY_SCALE;

					/* Scale offset from center for zoom-pulse effect */
					float sc_x = (cx0 + cx1) * 0.5f * ds;
					float sc_y = (cy0 + cy1) * 0.5f * ds;

					glTexCoord2f(tx0 + dtx, ty0 + dty);
					glVertex2f(cx0 + sc_x, cy0 + sc_y);
					glTexCoord2f(tx1 + dtx, ty0 + dty);
					glVertex2f(cx1 + sc_x, cy0 + sc_y);
					glTexCoord2f(tx1 + dtx, ty1 + dty);
					glVertex2f(cx1 + sc_x, cy1 + sc_y);
					glTexCoord2f(tx0 + dtx, ty1 + dty);
					glVertex2f(cx0 + sc_x, cy1 + sc_y);
				}
			}
		}
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		GL_OrthoOff();
		break;

	default:
		break;
	}

	GLUI_DrawStars(wdw, true);
}

/* ── Song info rendering ─────────────────────────────────────────────── */

static void
GLUI_DrawSongInfo(GLWindow_State* wdw, int y, int title_zoom, int info_zoom)
{
	const char* title;
	const char* info;
	char tmp[MODP_STR_LENGTH];
	int ntracks;

	Font_DrawString(wdw, "\\ccccccff", 0, 0, 0);

	title = AudioRenderer_Title(wdw->ps->am->active_ar);
	info = AudioRenderer_Info(wdw->ps->am->active_ar);

	if (info != NULL)
		Font_DrawString(wdw, info, 0, y - (wdw->font->font_height * (title_zoom + info_zoom)), info_zoom);

	if (title != NULL) {
		StrCpy(tmp, MODP_STR_LENGTH, title);

		size_t maxchar = wdw->width / (wdw->font->font_width * 3);
		if (maxchar <= SONG_INFO_TRAIL_CHARS)
			maxchar = MODP_STR_LENGTH;
		else
			maxchar -= SONG_INFO_TRAIL_CHARS;

		if (strnlen(tmp, MODP_STR_LENGTH) >= maxchar) {
			tmp[maxchar] = '\0';
			strncat(tmp, "...", 4);
		}

		Font_DrawString(wdw, tmp, 0, y - (wdw->font->font_height * title_zoom), title_zoom);
	}

	ntracks = AudioRenderer_NTracks(wdw->ps->am->active_ar);

	if (ntracks > 1) {
		snprintf(tmp, MODP_STR_LENGTH,
		                "\\ffffff80[%.2d/%.2d]",
		                AudioRenderer_Track(wdw->ps->am->active_ar) + 1,
		                ntracks);

		title_zoom = 2;
		y += (wdw->font->font_height * title_zoom);

		Font_DrawString(wdw, tmp, 0, y - (wdw->font->font_height * title_zoom), title_zoom);
	}
}

/* ── Layout computation ──────────────────────────────────────────────── */

static void
GLUI_DrawLayout(GLWindow_State* wdw)
{
	int zoom = ZOOM_BROWSER;

	wdw->max_items = (wdw->height / (wdw->font->font_height * zoom)) - 6;

	/* Compute layout for mouse hit testing (stored in OpenGL coords, Y=bottom)
	 * The draw code does: y = y - max_items*font_height*zoom (zoom=2), then zoom=3
	 * So the status bar bottom is at y - max_items*font_height*2 */
	wdw->layout_browser_x = (int)((float)wdw->width * 0.6f / wdw->font->font_width) * wdw->font->font_width;
	wdw->layout_browser_y = wdw->height / 2 + (wdw->max_items * (wdw->font->font_height));
	wdw->layout_item_height = wdw->font->font_height * zoom;
	wdw->layout_browser_height = wdw->max_items * wdw->layout_item_height;
	wdw->layout_status_y = wdw->layout_browser_y - wdw->max_items * wdw->font->font_height * 2;

	/* F-key positions in status bar (zoom=3, visible char positions)
	 * Color codes (\XXXXXXXX) are skipped by Font_DrawString, so only visible chars count:
	 *  f1:ainc/1;f2:arnd/1;f3,f4:mlth/g000;f5:vis/f
	 *  0       10      20        31 34
	 * Total: STATUS_BAR_CHAR_COUNT visible chars. F3=decrement (label), F4=increment (value). */
	int fz3 = wdw->font->font_width * STATUS_BAR_ZOOM;
	wdw->layout_fkey_x[0] =  0 * fz3;  /* "f1:ainc/1;" = 10 chars */
	wdw->layout_fkey_w[0] = 10 * fz3;
	wdw->layout_fkey_x[1] = 10 * fz3;  /* "f2:arnd/1;" = 10 chars */
	wdw->layout_fkey_w[1] = 10 * fz3;
	wdw->layout_fkey_x[2] = 20 * fz3;  /* "f3,f4:mlth/" = 11 chars (decrement) */
	wdw->layout_fkey_w[2] = 11 * fz3;
	wdw->layout_fkey_x[3] = 31 * fz3;  /* "000" value = 3 chars (increment) */
	wdw->layout_fkey_w[3] = 3 * fz3;
	wdw->layout_fkey_x[4] = 34 * fz3;  /* ";f5:vis/f" = 9 chars */
	wdw->layout_fkey_w[4] = 9 * fz3;

	/* Determine hover state (convert SDL mouse Y to OpenGL Y) */
	wdw->hover_item = browser_item_at_mouse_y(wdw);
}

/* ── Browser list rendering ──────────────────────────────────────────── */

static void
GLUI_DrawBrowser(GLWindow_State* wdw, int y, int zoom)
{
	int x = wdw->layout_browser_x;
	char tmp_str[MODP_STR_LENGTH];

	glColor4ub(GRAY(48, 64));
	GL_DrawRec(0, y, wdw->width, wdw->font->font_height * zoom * wdw->max_items, true, wdw->width, wdw->height);

	Font_DrawString(wdw, "\\ffffffff-> ",
	                x - (3 * wdw->font->font_width * zoom),
	                y - (wdw->font->font_height * zoom),
	                zoom);

	for (size_t i = 0; i < wdw->max_items; i++) {
		bool isdir;
		const char* name = Directory_GetName(wdw->ps->dir,
		                                     i + wdw->ps->dir_ofs,
		                                     &isdir);

		/* Hover highlight: white for hovered item, default for others */
		const char* item_color = (i == (size_t)wdw->hover_item) ? "\\ffffffff" : "";

		snprintf(tmp_str, MODP_STR_LENGTH,
		                "%s%s%s",
		                item_color,
		                isdir && name ? "\\" : " ", name ? name : "");

		Font_DrawString(wdw, tmp_str,
		                x + (wdw->font->font_width * zoom),
		                y - ((i + 1) * wdw->font->font_height * zoom),
		                zoom);
	}
}

/* ── Status bar rendering ────────────────────────────────────────────── */

static void
GLUI_DrawStatusBar(GLWindow_State* wdw, int y, int zoom)
{
	char tmp_str[MODP_STR_LENGTH];

	glColor4ub(GRAY(32, 64));
	GL_DrawRec(0, y, wdw->width, wdw->font->font_height * zoom, true, wdw->width, wdw->height);

	snprintf(tmp_str, MODP_STR_LENGTH,
	        "\\999999fff1:\\ccccccffainc/%s\\777777ff;"
	        "\\999999fff2:\\ccccccffarnd/%s\\777777ff;"
	        "\\999999fff3,f4:\\ccccccffmlth/%s%.3u"
	        "\\999999ff;f5:\\ccccccffvis/%s",
	        (wdw->ps->auto_inc ? "\\00dd00ff1" : "\\dd0000ff0"),
	        (wdw->ps->auto_rnd ? "\\00dd00ff1" : "\\dd0000ff0"),
	        (wdw->ps->auto_inc ? "\\00ff00ff" : "\\ff0000ff"), wdw->ps->min_length,
	        wdw->vis == VIS_FFT ? "\\dddd00fff" :
	        (wdw->vis == VIS_SCOPE ? "\\dddd00ffo" :
	        (wdw->vis == VIS_CIRCULAR ? "\\dddd00ffc" :
	        (wdw->vis == VIS_WATERFALL ? "\\dddd00ffw" : "\\dddd00ffx"))));

	int x = wdw->width - (wdw->font->font_width * zoom * STATUS_BAR_CHAR_COUNT);
	Font_DrawString(wdw, tmp_str, x, y - (wdw->font->font_height * zoom), zoom);
}

/* ── Song time rendering ─────────────────────────────────────────────── */

static void
GLUI_DrawSongTime(GLWindow_State* wdw, int y, int zoom)
{
	float song_pos = AudioRenderer_PlayTime(wdw->ps->am->active_ar);
	float song_len = AudioRenderer_Length(wdw->ps->am->active_ar);
	char tmp_str[MODP_STR_LENGTH];

	glColor4ub(GRAY(32, 64));
	GL_DrawRec(0, y, wdw->width, wdw->font->font_height * zoom, true, wdw->width, wdw->height);

	snprintf(tmp_str, MODP_STR_LENGTH,
	                "\\aaaaaa80%.2d:%.2d\\cccccc80/\\aaaaaa80%.2d:%.2d",
	                (int)(song_pos / 60.f), ((int)(song_pos)) % 60,
	                (int)(song_len / 60.f), ((int)(song_len)) % 60);

	int x = wdw->width - (wdw->font->font_width * zoom * 11);
	Font_DrawString(wdw, tmp_str, x, y - (wdw->font->font_height * zoom), zoom);
}

/* ── Main draw function (dispatches sub-functions) ───────────────────── */

void
GLUI_Draw(GLWindow_State* wdw)
{
	Vis_Update(wdw);
	GLUI_DrawVis(wdw);

	/* Compute layout before drawing (needed for hit testing) */
	GLUI_DrawLayout(wdw);

	/* Options editor overlay (drawn on top of everything) */
	GLUI_DrawOptionsEditor(wdw);

	/* Background color: blend base with reactive spectral color + energy flash */
	float boost = wdw->v->mean_energy_band_div16 / MAX_ENERGY * wdw->opts->ui.bg_flash_factor;
	boost *= boost;

	float r = wdw->opts->ui.clr[0] + wdw->v->reactive_color[0] * REACTIVE_BLEND + boost;
	float g = wdw->opts->ui.clr[1] + wdw->v->reactive_color[1] * REACTIVE_BLEND + boost;
	float b = wdw->opts->ui.clr[2] + wdw->v->reactive_color[2] * REACTIVE_BLEND + boost;
	glClearColor(r, g, b, 0.f);

	/* Draw browser list (zoom=2) */
	int y = wdw->layout_browser_y;
	int zoom = ZOOM_BROWSER;
	GLUI_DrawBrowser(wdw, y, zoom);

	/* Draw status bar (zoom=3) */
	y = y - (wdw->max_items * wdw->font->font_height * zoom);
	zoom = ZOOM_STATUS;
	GLUI_DrawStatusBar(wdw, y, zoom);

	/* Draw song time + song info (zoom=3) */
	y = y + ((wdw->max_items + 1) * wdw->font->font_height * ZOOM_BROWSER) + wdw->font->font_height;
	zoom = ZOOM_SONG;
	GLUI_DrawSongTime(wdw, y, zoom);
	GLUI_DrawSongInfo(wdw, y, zoom, 2);
}

/* ── Window resize ───────────────────────────────────────────────────── */

static void
GLWindow_Resize(GLWindow_State* wdw, int w, int h)
{
	wdw->width = w;
	wdw->height = h;
	glViewport(0, 0, w, h);
}

/* ── Keyboard handling ───────────────────────────────────────────────── */

static void
GLWindow_HandleFKey(GLWindow_State* wdw, int fkey)
{
	switch (fkey) {
		case 1: Player_ToggleAutoInc(wdw->ps); break;
		case 2: Player_ToggleAutoRnd(wdw->ps); break;
		case 3: Player_AlterMinLength(wdw->ps, -15); break;
		case 4: Player_AlterMinLength(wdw->ps, 15); break;
		case 5: wdw->vis = (Vis)((wdw->vis + 1) % (VIS_NONE + 1)); break;
		default: break;
	}
}

void
GLWindow_HandleKeyDown(GLWindow_State* wdw, SDL_Keysym* keysym)
{
	/* Options editor takes priority */
	if (wdw->editor && wdw->editor->active) {
		GLWindow_OptionsEditor_HandleKey(wdw, keysym);
		return;
	}

	switch (keysym->sym) {
		case SDLK_DOWN:
			Player_AlterOffset(wdw->ps, 1);
			break;
		case SDLK_UP:
			Player_AlterOffset(wdw->ps, -1);
			break;
		case SDLK_RIGHT:
			Player_AlterSubTrack(wdw->ps, 1);
			break;
		case SDLK_LEFT:
			Player_AlterSubTrack(wdw->ps, -1);
			break;
		case SDLK_PAGEDOWN:
			Player_AlterOffset(wdw->ps, wdw->max_items);
			break;
		case SDLK_PAGEUP:
			Player_AlterOffset(wdw->ps, -wdw->max_items);
			break;
		case SDLK_HOME:
			Player_Home(wdw->ps);
			break;
		case SDLK_END:
			Player_End(wdw->ps);
			break;
		case SDLK_RETURN:
			Player_Perform(wdw->ps);
			break;
		case SDLK_F1:
		case SDLK_F2:
		case SDLK_F3:
		case SDLK_F4:
		case SDLK_F5:
			GLWindow_HandleFKey(wdw, keysym->sym - SDLK_F1 + 1);
			break;
		case SDLK_F6:
			wdw->opts->ui.clr[0] = (float)rand() / RAND_MAX;
			wdw->opts->ui.clr[1] = (float)rand() / RAND_MAX;
			wdw->opts->ui.clr[2] = (float)rand() / RAND_MAX;
			break;
		case SDLK_o:
			if (wdw->editor) {
				if (wdw->editor->active) {
					wdw->editor->active = false;
				} else {
					wdw->editor->selected = 0;
					wdw->editor->scroll = 0;
					wdw->editor->active = true;
				}
			}
			break;
		case SDLK_r:
			Player_PlayRandom(wdw->ps);
			break;
		case SDLK_n:
			Player_PlayNext(wdw->ps, true);
			break;
		case SDLK_p:
			Player_PlayPrev(wdw->ps, true);
			break;
		case SDLK_SPACE:
			Player_PlayPause(wdw->ps);
			break;
		case SDLK_BACKSPACE:
			Player_DirUp(wdw->ps);
			break;
		case SDLK_ESCAPE: {
			SDL_Event quit_event = { .type = SDL_QUIT };
			SDL_PushEvent(&quit_event);
			break;
		}
		default:
			break;
	}
}

/* ── Event processing ────────────────────────────────────────────────── */

bool
GLWindow_ProcessEvents(GLWindow_State* wdw, bool* got_input)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_KEYDOWN:
				*got_input = true;
				GLWindow_HandleKeyDown(wdw, &event.key.keysym);
				break;
			case SDL_MOUSEMOTION:
				wdw->mouse_x = event.motion.x;
				wdw->mouse_y = event.motion.y;
				break;
			case SDL_MOUSEWHEEL:
				*got_input = true;
				{
					int gl_y = wdw->height - wdw->mouse_y;
					int browser_top = wdw->layout_browser_y - wdw->max_items * wdw->layout_item_height;
					/* Only scroll when mouse is over the browser area */
					if (event.wheel.y != 0 &&
					    gl_y > browser_top &&
					    gl_y < wdw->layout_browser_y) {
						int scroll = (event.wheel.y > 0) ? -1 : 1;
						/* Clamp multi-line scroll (pixel scrolling from trackpads) */
						if (scroll > MOUSE_SCROLL_CLAMP) scroll = MOUSE_SCROLL_CLAMP;
						if (scroll < -MOUSE_SCROLL_CLAMP) scroll = -MOUSE_SCROLL_CLAMP;
						Player_AlterOffset(wdw->ps, scroll);
					}
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT) {
					*got_input = true;
					int mx = event.button.x;
					int gl_my = wdw->height - event.button.y; /* SDL Y → OpenGL Y */
					int status_h = wdw->font->font_height * 3;
					bool handled = false;

					/* Check status bar (F-key toggles)
					 * Font_DrawString draws text at y - font_height*zoom, so the text
					 * occupies [layout_status_y - status_h, layout_status_y), NOT
					 * [layout_status_y, layout_status_y + status_h) */
					if (gl_my >= wdw->layout_status_y - status_h && gl_my < wdw->layout_status_y) {
						int status_x = wdw->width - (wdw->font->font_width * 3 * STATUS_BAR_CHAR_COUNT);
						int rel_x = mx - status_x;
						int f;
						for (f = 0; f < 5; f++) {
							if (rel_x >= wdw->layout_fkey_x[f] &&
							    rel_x < wdw->layout_fkey_x[f] + wdw->layout_fkey_w[f]) {
								SDL_Keysym ks = { .sym = SDLK_F1 + f };
								GLWindow_HandleKeyDown(wdw, &ks);
								handled = true;
								break;
							}
						}
					}

					/* Check browser items (only if F-key wasn't hit) */
					if (!handled) {
						int idx = browser_item_at_mouse_y(wdw);
						if (idx >= 0) {
							/* Navigate to clicked item: idx 0 means stay, idx 1 means +1, etc. */
							Player_AlterOffset(wdw->ps, idx);
							Player_Perform(wdw->ps);
						}
					}
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
					*got_input = true;
					SDL_Keysym ks = { .sym = SDLK_BACKSPACE };
					GLWindow_HandleKeyDown(wdw, &ks);
				}
				break;
			case SDL_QUIT:
				return false;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_RESIZED)
					GLWindow_Resize(wdw, event.window.data1, event.window.data2);
				break;
			default:
				break;
		}
	}

	return true;
}

/* ── Destruction ─────────────────────────────────────────────────────── */

void
GLWindow_Destroy(GLWindow_State* wdw)
{
	assert(wdw);

	GLWindow_OptionsEditor_Destroy(wdw->editor);
	free(wdw->editor);

	Vis_Destroy(wdw->v);
	Font_Destroy(wdw->font);

	free(wdw);

	SDL_Quit();
}

/* ── Initialization ──────────────────────────────────────────────────── */

GLWindow_State*
GLWindow_Init(Options* opt, Player_State* ps)
{
	SDL_Window* sdl_wdw;
	GLWindow_State* gl_wdw;
	Vis_State* v;

	gl_wdw = (GLWindow_State*) calloc(1, sizeof(GLWindow_State));
	assert(gl_wdw);

	gl_wdw->ps = ps;
	gl_wdw->opts = opt;
	gl_wdw->hover_item = -1;

	/* Initialize options editor */
	gl_wdw->editor = (OptionsEditor*) calloc(1, sizeof(OptionsEditor));
	assert(gl_wdw->editor);
	GLWindow_OptionsEditor_Init(gl_wdw->editor);

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_Log("Video initialization failed: %s", SDL_GetError());
		free(gl_wdw->editor);
		free(gl_wdw);
		return NULL;
	}

	gl_wdw->width = opt->wdw_width;
	gl_wdw->height = opt->wdw_height;
	gl_wdw->fps_limit = opt->fps_limit;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	sdl_wdw = SDL_CreateWindow("modp",
	                           100,
	                           100,
	                           gl_wdw->width,
	                           gl_wdw->height,
	                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	SDL_GL_CreateContext(sdl_wdw);
	SDL_SetWindowMinimumSize(sdl_wdw, 640, 480);

	GL_Init(gl_wdw->width,
	        gl_wdw->height,
	        opt->ui.clr[0], opt->ui.clr[1], opt->ui.clr[2]);

	gl_wdw->sdl_wdw = sdl_wdw;

	gl_wdw->font = Font_Init(opt->fontpath, opt->font_dbl);
	assert(gl_wdw->font);

	v = Vis_Init(gl_wdw->width, gl_wdw->height, VIS_NSAMPLES, VIS_NSTARS);
	gl_wdw->v = v;

	return gl_wdw;
}
