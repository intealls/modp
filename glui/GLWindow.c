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

	if (wdw->ps->am->playing) {
		Player_GetPlaybackData(wdw->ps, v->vis_buf, v->vis_len, true);

		for (size_t i = 0; i < v->nsamples * 2; i += 2)
			v->signal[i / 2] = (v->vis_buf[i] + v->vis_buf[i + 1]) / 2.f * v->window[i / 2];

		fftwf_execute(v->plan);

		v->mean_energy_band_div16 = 0;

		for (size_t i = 0; i < v->fft_len; i++) {
			float energy = sqrt(pow(v->result[i][0], 2) + pow(v->result[i][1], 2));

			v->spectrum[i] = log10(energy);

			if (i < v->fft_len / 16)
				v->mean_energy_band_div16 += energy;
		}

		v->mean_energy_band_div16 /= v->fft_len / 16;
	} else {
		v->mean_energy_band_div16 = 0.f;
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

		glBindTexture(GL_TEXTURE_2D, 0);
		GL_OrthoOn(wdw->width, wdw->height);
		glBegin(GL_QUADS);
		{
			for (size_t i = 3; i < v->fft_len / 2 + 2; i += 2) {
				glColor4ub(0, 0, 0, 255);

				glVertex2f((i - 3) * (wdw->width * 2 / v->fft_len) + 8, (int) (v->spectrum[i - 2] * scale));
				glVertex2f((i - 1) * (wdw->width * 2 / v->fft_len) + 8, (int) (v->spectrum[i] * scale));

				glVertex2f((i - 1) * (wdw->width * 2 / v->fft_len) + 8, 0);
				glVertex2f((i - 3) * (wdw->width * 2 / v->fft_len) + 8, 0);

				glColor4ub(255, (int) ((float) i * 255.f / (float) wdw->v->fft_len), 0, 255);

				glVertex2f((i - 3) * (wdw->width * 2 / v->fft_len), (int) (v->spectrum[i - 2] * scale));
				glVertex2f((i - 1) * (wdw->width * 2 / v->fft_len), (int) (v->spectrum[i] * scale));

				glVertex2f((i - 1) * (wdw->width * 2 / v->fft_len), 0);
				glVertex2f((i - 3) * (wdw->width * 2 / v->fft_len), 0);
			}
		}
		glEnd();
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
		assert(memccpy(tmp, title, '\0', MODP_STR_LENGTH) != NULL);

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

	Vis_Update(wdw);
	GLUI_DrawVis(wdw);

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

		assert(snprintf(tmp_str,
		                MODP_STR_LENGTH,
		                "%s%s",
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
			glClearColor((float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, 0);
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

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
		free(gl_wdw);
		return NULL;
	}

	gl_wdw->width = opt->wdw_width;
	gl_wdw->height = opt->wdw_height;
	gl_wdw->fps_limit = opt->fps_limit;
	gl_wdw->font_shake_factor = opt->font_shake_factor;

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
	        opt->clr_r, opt->clr_g, opt->clr_b);

	SDL_ShowCursor(1);

	gl_wdw->sdl_wdw = sdl_wdw;

	gl_wdw->font = Font_Init(opt->fontpath, opt->font_dbl);
	assert(gl_wdw->font);

	v = Vis_Init(gl_wdw->width, gl_wdw->height, 384, 100);
	gl_wdw->v = v;

	return gl_wdw;
}
