// Copyright intealls
// License: GPL v3

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <fftw3.h>
#include <math.h>

#include "GLWindow.h"

#include "Font.h"
#include "GL.h"
#include "GLColors.h"
#include "Player.h"
#include "Globals.h"
#include "Utils.h"

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

	// Blackman-Nuttall window (B=1.9761), ~100dB sidelobe attenuation
	// from Wikipedia

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
		v->stars[i].speed_y = rand() % 3 + 1;
		v->stars[i].xpos = rand() % wdw_width;
		v->stars[i].ypos = rand() % wdw_height;
		v->stars[i].size = rand() % 5;
		v->stars[i].in_front = rand() % 2;
		v->stars[i].phase = 0;
		v->stars[i].phase_inc = rand() % 2 + 1;
		v->stars[i].rotation = 0;
		v->stars[i].rotation_inc = rand() % 2 + 1;
		v->stars[i].visible = false;
	}

	return v;
}

static void
Vis_Update(GLWindow_State* wdw)
{
	Vis_State* v = wdw->v;

	v->mean_energy_band_div16 = 0.f;

	if (wdw->ps->am->playing) {
		Player_GetPlaybackData(wdw->ps, v->vis_buf, v->vis_len, true);

		for (size_t i = 0; i < v->nsamples * 2; i += 2)
			v->signal[i / 2] = (v->vis_buf[i] + v->vis_buf[i + 1]) / 2.f * v->window[i / 2];

		fftwf_execute(v->plan);

		for (size_t i = 0; i < v->fft_len; i++) {
			float energy = sqrt(pow(v->result[i][0], 2) + pow(v->result[i][1], 2));

			v->spectrum[i] = log10(energy);

			if (i > 16 && i < (v->fft_len / 16) + 16)
				v->mean_energy_band_div16 += energy;
		}

		v->mean_energy_band_div16 /= v->fft_len / 16;

		/* Reactive background: split spectrum into 3 bands,
		   normalize to 0–1, lerp toward target each frame. */
		{
			float bass = 0, mids = 0, treble = 0;
			size_t bass_n   = 0, mids_n = 0, treble_n = 0;
			size_t mid_start   = v->fft_len / 8;
			size_t treble_start = v->fft_len / 3;

			for (size_t i = 2; i < v->fft_len / 2; i++) {
				float e = (v->result[i][0] * v->result[i][0] +
				           v->result[i][1] * v->result[i][1]);
				if (i < mid_start)   { bass   += e; bass_n++; }
				else if (i < treble_start) { mids   += e; mids_n++; }
				else                  { treble += e; treble_n++; }
			}
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
	} else {
		/* Decay reactive color to zero when not playing. */
		for (int c = 0; c < 3; c++)
			v->reactive_color[c] *= 0.95f;
	}
}

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
	free(v);
}

static void
GLUI_DrawStars(GLWindow_State* wdw, bool in_front)
{
	Star* stars = wdw->v->stars;

	GL_OrthoOn(wdw->width, wdw->height);
	glDisable(GL_TEXTURE_2D);

	for (size_t i = 0; i < wdw->v->nstars; i++) {
		if (!stars[i].in_front && in_front)
			continue;

		stars[i].xpos += stars[i].speed_x;
		stars[i].ypos += stars[i].speed_y;

		if (stars[i].ypos >= (int) wdw->height && wdw->ps->am->playing)
			stars[i].visible = true;
		else if (stars[i].ypos >= (int) wdw->height && !wdw->ps->am->playing)
			stars[i].visible = false;

		stars[i].xpos %= wdw->width;
		stars[i].ypos %= wdw->height;
		stars[i].phase += stars[i].phase_inc;
		stars[i].rotation += stars[i].rotation_inc;
		stars[i].phase %= 360;
		stars[i].rotation %= 360;

		int xpos = stars[i].xpos + stars[i].size * sin(stars[i].phase * M_PI / 180);
		int ypos = stars[i].ypos;

		if (!stars[i].visible)
			continue;

		glPushMatrix();
		glColor4f(1, 1, 0, (rand() % 255) / 255.f);
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

static void
GLUI_DrawVis(GLWindow_State* wdw)
{
	Vis_State* v = wdw->v;
	float scale = 1;

	GLUI_DrawStars(wdw, false);

	if (wdw->vis == VIS_FFT && wdw->ps->am->playing) {
		scale = wdw->height / log10(1 << 24);
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

	scale = 192.f;
	if (wdw->vis == VIS_SCOPE && wdw->ps->am->playing) {
		glBindTexture(GL_TEXTURE_2D, 0);
		GL_OrthoOn(wdw->width, wdw->height);
		glBegin(GL_QUADS);
		{
			for (size_t i = 0; i < wdw->width * 2 - 2; i += 2) {
				float point = (v->vis_buf[i] + v->vis_buf[i + 1]) / scale;
				float next_point = (v->vis_buf[i + 2] + v->vis_buf[i + 1 + 2]) / scale;
				float y0, y1, expand;

				y0 = wdw->height / 2 + point;
				y1 = wdw->height / 2 + next_point;

				expand = 2 - fabs(next_point - point);

				if (expand > 0) {
					y0 -= expand / 2;
					y1 += expand / 2;
				}

				glColor4ub(0, 0, 0, 255);
				glVertex2i(i - 5 + 3, y0 - 3);
				glVertex2i(i + 5 + 3, y0 - 3);
				glVertex2i(i + 5 + 1 + 3, y1 - 3);
				glVertex2i(i - 5 + 1 + 3, y1 - 3);
			}

			for (size_t i = 0; i < wdw->width * 2 - 2; i += 2) {
				float point = (v->vis_buf[i] + v->vis_buf[i + 1]) / scale;
				float next_point = (v->vis_buf[i + 2] + v->vis_buf[i + 1 + 2]) / scale;
				float y0, y1, expand;

				y0 = wdw->height / 2 + point;
				y1 = wdw->height / 2 + next_point;

				expand = 2 - fabs(next_point - point);

				if (expand > 0) {
					if (y0 < y1) {
						y0 -= expand / 2;
						y1 += expand / 2;
					} else {
						y0 += expand / 2;
						y1 -= expand / 2;
					}
				}

				glColor4ub(255, (int) ((float) (i / 2) * 255.f / (float) wdw->width * 2), 0, 255);
				glVertex2i(i - 5, y0);
				glVertex2i(i + 5, y0);
				glVertex2i(i + 5 + 1, y1);
				glVertex2i(i - 5 + 1, y1);
			}
		}
		glEnd();
		GL_OrthoOff();
	}

	GLUI_DrawStars(wdw, true);
}

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

		size_t maxchar = wdw->width / (wdw->font->font_width * 3) - 14; // ...00:00/00:00

		if (strnlen(tmp, MODP_STR_LENGTH) >= maxchar) {
			tmp[maxchar] = '\0';
			strncat(tmp, "...", strlen(tmp) - 3 - 1);
		}

		Font_DrawString(wdw, tmp, 0, y - (wdw->font->font_height * title_zoom), title_zoom);
	}

	ntracks = AudioRenderer_NTracks(wdw->ps->am->active_ar);

	if (ntracks > 1) {
		assert(snprintf(tmp,
		                MODP_STR_LENGTH,
		                "\\ffffff80[%.2d/%.2d]",
		                AudioRenderer_Track(wdw->ps->am->active_ar) + 1,
		                ntracks) < MODP_STR_LENGTH - 1);

		title_zoom = 2;
		y += (wdw->font->font_height * title_zoom);

		Font_DrawString(wdw, tmp, 0, y - (wdw->font->font_height * title_zoom), title_zoom);
	}
}

void
GLUI_Draw(GLWindow_State* wdw)
{
	char tmp_str[MODP_STR_LENGTH];

	float song_pos = AudioRenderer_PlayTime(wdw->ps->am->active_ar);
	float song_len = AudioRenderer_Length(wdw->ps->am->active_ar);

	int x = (int) ((float) wdw->width * 0.6f / wdw->font->font_width) * wdw->font->font_width;
	int y = wdw->height / 2 + (wdw->max_items * (wdw->font->font_height));
	int zoom = 2;

	wdw->max_items = (wdw->height / (wdw->font->font_height * zoom)) - 6;

	// Compute layout for mouse hit testing (stored in OpenGL coords, Y=bottom)
	// The draw code does: y = y - max_items*font_height*zoom (zoom=2), then zoom=3
	// So the status bar bottom is at y - max_items*font_height*2
	wdw->layout_browser_x = x;
	wdw->layout_browser_y = y;
	wdw->layout_item_height = wdw->font->font_height * zoom;
	wdw->layout_browser_height = wdw->max_items * wdw->layout_item_height;
	wdw->layout_status_y = y - wdw->max_items * wdw->font->font_height * 2;

	// F-key positions in status bar (zoom=3, visible char positions)
	// Color codes (\XXXXXXXX) are skipped by Font_DrawString, so only visible chars count:
	//  f1:ainc/1;f2:arnd/1;f3,f4:mlth/g000;f5:vis/f
	//  0       10      20        31 34
	// Total: 43 visible chars. F3=decrement (label), F4=increment (value).
	{
		int fz3 = wdw->font->font_width * 3;
		wdw->layout_fkey_x[0] = 0 * fz3;      // "f1:ainc/1;" = 10 chars
		wdw->layout_fkey_w[0] = 10 * fz3;
		wdw->layout_fkey_x[1] = 10 * fz3;     // "f2:arnd/1;" = 10 chars
		wdw->layout_fkey_w[1] = 10 * fz3;
		wdw->layout_fkey_x[2] = 20 * fz3;     // "f3,f4:mlth/" = 11 chars (decrement)
		wdw->layout_fkey_w[2] = 11 * fz3;
		wdw->layout_fkey_x[3] = 31 * fz3;     // "000" value = 3 chars (increment)
		wdw->layout_fkey_w[3] = 3 * fz3;
		wdw->layout_fkey_x[4] = 34 * fz3;     // ";f5:vis/f" = 9 chars
		wdw->layout_fkey_w[4] = 9 * fz3;
	}

	// Determine hover state (convert SDL mouse Y to OpenGL Y)
	// Item i is drawn at Y = layout_browser_y - (i+1)*item_height
	// Its band is [layout_browser_y - (i+1)*item_height, layout_browser_y - i*item_height)
	// Item 0 is at bottom (closest to layout_browser_y), item max_items-1 at top
	wdw->hover_item = -1;
	{
		int gl_y = wdw->height - wdw->mouse_y;
		int browser_bottom = wdw->layout_browser_y;
		int browser_top = wdw->layout_browser_y - wdw->max_items * wdw->layout_item_height;
		if (gl_y > browser_top && gl_y < browser_bottom) {
			int idx = (browser_bottom - gl_y - 1) / wdw->layout_item_height;
			if (idx >= 0 && idx < (int)wdw->max_items)
				wdw->hover_item = idx;
		}
	}

	Vis_Update(wdw);
	GLUI_DrawVis(wdw);

	float boost = wdw->v->mean_energy_band_div16 / (65536.f * 8.f) * wdw->bg_flash_factor;
	boost *= boost;

	/* Blend base color with reactive spectral color, then add flash. */
	float r = wdw->clrcolor[0] + wdw->v->reactive_color[0] * 0.25f + boost;
	float g = wdw->clrcolor[1] + wdw->v->reactive_color[1] * 0.25f + boost;
	float b = wdw->clrcolor[2] + wdw->v->reactive_color[2] * 0.25f + boost;
	glClearColor(r, g, b, 0.f);

	glColor4ub(GRAY(48, 64));
	GL_DrawRec(0, y, wdw->width, wdw->font->font_height * zoom * wdw->max_items, true, wdw->width, wdw->height);
	Font_DrawString(wdw,
	                "\\ffffffff-> ",
	                x - (3 * wdw->font->font_width * zoom),
	                y - (wdw->font->font_height * zoom),
	                zoom);
	for (size_t i = 0; i < wdw->max_items; i++) {
		bool isdir;
		const char* name = Directory_GetName(wdw->ps->dir,
		                                     i + wdw->ps->dir_ofs,
		                                     &isdir);

		// Hover highlight: white for hovered item, default for others
		const char* item_color = (i == (size_t)wdw->hover_item) ? "\\ffffffff" : "";

		assert(snprintf(tmp_str,
		                MODP_STR_LENGTH,
		                "%s%s%s",
		                item_color,
		                isdir && name ? "\\" : " ", name ? name : "")
		        < MODP_STR_LENGTH - 1);

		Font_DrawString(wdw,
		                tmp_str,
		                x + (wdw->font->font_width * zoom),
		                y - ((i + 1) * wdw->font->font_height * zoom),
		                zoom);
	}

	x = 0;
	y = y - (wdw->max_items * wdw->font->font_height * zoom);
	zoom = 3;

	glColor4ub(GRAY(32, 64));
	GL_DrawRec(0, y, wdw->width, wdw->font->font_height * zoom, true, wdw->width, wdw->height);

	assert(snprintf(tmp_str,
	                MODP_STR_LENGTH,
	                "\\999999fff1:\\ccccccffainc/%s\\777777ff;"
	                "\\999999fff2:\\ccccccffarnd/%s\\777777ff;"
	                "\\999999fff3,f4:\\ccccccffmlth/%s%.3u"
	                "\\999999ff;f5:\\ccccccffvis/%s",
	                (wdw->ps->auto_inc ? "\\00dd00ff1" : "\\dd0000ff0"),
	                (wdw->ps->auto_rnd ? "\\00dd00ff1" : "\\dd0000ff0"),
	                (wdw->ps->auto_inc ? "\\00ff00ff" : "\\ff0000ff"), wdw->ps->min_length,
	                wdw->vis == VIS_FFT ? "\\dddd00fff" : "\\dddd00ffo")
	        < MODP_STR_LENGTH - 1);

	x = wdw->width - (wdw->font->font_width * zoom * 43);
	Font_DrawString(wdw, tmp_str, x, y - (wdw->font->font_height * zoom), zoom);

	zoom = 2;
	y = y + ((wdw->max_items + 1) * wdw->font->font_height * zoom) + wdw->font->font_height;
	zoom = 3;

	glColor4ub(GRAY(32, 64));
	GL_DrawRec(0, y, wdw->width, wdw->font->font_height * zoom, true, wdw->width, wdw->height);

	assert(snprintf(tmp_str,
	                MODP_STR_LENGTH,
	                "\\aaaaaa80%.2d:%.2d\\cccccc80/\\aaaaaa80%.2d:%.2d",
	                (int) (song_pos / 60.f), ((int) (song_pos)) % 60 % 100,
	                (int) (song_len / 60.f), ((int) (song_len)) % 60 % 100) < MODP_STR_LENGTH - 1);

	x = wdw->width - (wdw->font->font_width * zoom * 11);
	zoom = 3;
	Font_DrawString(wdw, tmp_str, x, y - (wdw->font->font_height * zoom), zoom);

	GLUI_DrawSongInfo(wdw, y, zoom, 2);
}

static void
GLWindow_Resize(GLWindow_State* wdw, int w, int h)
{
	wdw->width = w;
	wdw->height = h;
	glViewport(0, 0, w, h);
	SDL_SetWindowSize(wdw->sdl_wdw, w, h);
}

void
GLWindow_HandleKeyDown(GLWindow_State* wdw, SDL_Keysym* keysym)
{
	SDL_Event quit_event;
	quit_event.type = SDL_QUIT;

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
			Player_ToggleAutoInc(wdw->ps);
			break;
		case SDLK_F2:
			Player_ToggleAutoRnd(wdw->ps);
			break;
		case SDLK_F3:
			Player_AlterMinLength(wdw->ps, -15);
			break;
		case SDLK_F4:
			Player_AlterMinLength(wdw->ps, 15);
			break;
		case SDLK_F5:
			if (++wdw->vis == VIS_NONE)
				wdw->vis = VIS_FFT;
			break;
		case SDLK_F6:
			wdw->clrcolor[0] = (float)rand() / RAND_MAX;
			wdw->clrcolor[1] = (float)rand() / RAND_MAX;
			wdw->clrcolor[2] = (float)rand() / RAND_MAX;
			break;
		case SDLK_r:
			Player_PlayRandom(wdw->ps);
			break;
		case SDLK_d:
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
		case SDLK_ESCAPE:
			SDL_PushEvent(&quit_event);
			break;
		case SDLK_F10:
			GLWindow_Resize(wdw, wdw->width, wdw->height);
			break;
		case SDLK_F9:
			GLWindow_Resize(wdw, wdw->width / 2, wdw->height / 2);
			break;
		case SDLK_F8:
			GLWindow_Resize(wdw, wdw->width / 4, wdw->height / 4);
			break;
		default:
			break;
	}
}

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
					// Only scroll when mouse is over the browser area
					if (event.wheel.y != 0 &&
					    gl_y > browser_top &&
					    gl_y < wdw->layout_browser_y) {
						int scroll = (event.wheel.y > 0) ? -1 : 1;
						// Clamp multi-line scroll (pixel scrolling from trackpads)
						if (scroll > 5) scroll = 5;
						if (scroll < -5) scroll = -5;
						Player_AlterOffset(wdw->ps, scroll);
					}
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (event.button.button == SDL_BUTTON_LEFT) {
					*got_input = true;
					int mx = event.button.x;
					int gl_my = wdw->height - event.button.y; // SDL Y → OpenGL Y
					int status_h = wdw->font->font_height * 3;

					// Check status bar (F-key toggles)
					// Font_DrawString draws text at y - font_height*zoom, so the text
					// occupies [layout_status_y - status_h, layout_status_y), NOT
					// [layout_status_y, layout_status_y + status_h)
					if (gl_my >= wdw->layout_status_y - status_h && gl_my < wdw->layout_status_y) {
						int status_x = wdw->width - (wdw->font->font_width * 3 * 43);
						int rel_x = mx - status_x;
						int f;
						for (f = 0; f < 5; f++) {
							if (rel_x >= wdw->layout_fkey_x[f] &&
							    rel_x < wdw->layout_fkey_x[f] + wdw->layout_fkey_w[f])
								break;
						}
						if (f < 5) {
							SDL_Keysym ks;
							ks.sym = SDLK_F1 + f;
							GLWindow_HandleKeyDown(wdw, &ks);
							break;
						}
					}

					// Check browser items
					// Item i band: [layout_browser_y - (i+1)*item_height, layout_browser_y - i*item_height)
					// Item 0 at bottom, item max_items-1 at top
					{
						int browser_bottom = wdw->layout_browser_y;
						int browser_top = wdw->layout_browser_y - wdw->max_items * wdw->layout_item_height;
						if (gl_my > browser_top && gl_my < browser_bottom) {
							int idx = (browser_bottom - gl_my - 1) / wdw->layout_item_height;
							if (idx >= 0 && idx < (int)wdw->max_items) {
								// Navigate to clicked item: idx 0 means stay, idx 1 means +1, etc.
								Player_AlterOffset(wdw->ps, idx);
								Player_Perform(wdw->ps);
							}
						}
					}
				} else if (event.button.button == SDL_BUTTON_RIGHT) {
					*got_input = true;
					SDL_Keysym ks;
					ks.sym = SDLK_BACKSPACE;
					GLWindow_HandleKeyDown(wdw, &ks);
				}
				break;
			case SDL_QUIT:
				return false;
				break;
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

void
GLWindow_Destroy(GLWindow_State* wdw)
{
	assert(wdw);

	Vis_Destroy(wdw->v);
	Font_Destroy(wdw->font);

	free(wdw);

	SDL_Quit();
}

GLWindow_State*
GLWindow_Init(Options* opt, Player_State* ps)
{
	SDL_Window* sdl_wdw;
	GLWindow_State* gl_wdw;
	Vis_State* v;

	gl_wdw = (GLWindow_State*) calloc(1, sizeof(GLWindow_State));
	assert(gl_wdw);

	gl_wdw->ps = ps;
	gl_wdw->hover_item = -1;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_Log("Video initialization failed: %s", SDL_GetError());
		free(gl_wdw);
		return NULL;
	}

	gl_wdw->width = opt->wdw_width;
	gl_wdw->height = opt->wdw_height;
	gl_wdw->fps_limit = opt->fps_limit;
	gl_wdw->font_shake_factor = opt->font_shake_factor;
	gl_wdw->font_zoom_factor = opt->font_zoom_factor;
	gl_wdw->font_rotation_factor = opt->font_rotation_factor;
	gl_wdw->bg_flash_factor = opt->bg_flash_factor;

	gl_wdw->clrcolor[0] = opt->clr_r;
	gl_wdw->clrcolor[1] = opt->clr_g;
	gl_wdw->clrcolor[2] = opt->clr_b;

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

	GL_Init(gl_wdw->width,
	        gl_wdw->height,
	        gl_wdw->clrcolor[0], gl_wdw->clrcolor[1], gl_wdw->clrcolor[2]);

	gl_wdw->sdl_wdw = sdl_wdw;

	gl_wdw->font = Font_Init(opt->fontpath, opt->font_dbl);
	assert(gl_wdw->font);

	v = Vis_Init(gl_wdw->width, gl_wdw->height, 384, 100);
	gl_wdw->v = v;

	return gl_wdw;
}
