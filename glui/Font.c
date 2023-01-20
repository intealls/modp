// Copyright intealls
// License: GPL v3

#include <stdint.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "Neep_Font.h"
#include "LocalDir.h"
#include "GLWindow.h"
#include "GL.h"
#include "GLColors.h"
#include "Globals.h"

#define TO_INT(a)	((a >= '0' && a <= '9') ? a - '0' : \
						((a >= 'a' && a <= 'f') ? a - 'a' + 10 : -1))

#define IN_RANGE(a)	((a >= '0' && a <= '9') || (a >= 'a' && a <= 'f'))

static int
hc_to_rgba(const char* h, GLubyte* rgba)
{
	GLubyte f1, f2;
	int i;
	char* hp = (char*) h;

	while ((*hp) != '\0' && hp - h < 8) {
		if (!IN_RANGE(*hp))
			return 1;
		hp++;
	}

	if (strnlen(h, 8) < 8)
		return 1;

	for (i = 0; i < 4; i++) {
		f1 = TO_INT(h[i * 2]);
		f2 = TO_INT(h[(i * 2) + 1]);
		rgba[i] = f1 * 16 + f2;
	}

	return 0;
}

void
Font_DrawString(GLWindow_State* wdw, const char* str, int x, int y, int zoom)
{
	int pos = 0;
	float jit_factor = 0;
	size_t line_breaks = 0;

	Vis_State* v = wdw->v;

	jit_factor = v->mean_energy_band_div16 / 65536.f;

	GL_OrthoOn(wdw->width, wdw->height);

	glBindTexture(GL_TEXTURE_2D, wdw->font->tex_handle);

	while (*str != '\0' && line_breaks < wdw->max_items) {
		glPushMatrix();
		glTranslatef(x, y, 0.f);
		glBegin(GL_QUADS);
		{
			while (*str != '\0') {
				float rnd_x = ((((float)rand() / RAND_MAX) - 0.5) * jit_factor) * wdw->font_shake_factor;
				float rnd_y = ((((float)rand() / RAND_MAX) - 0.5) * jit_factor) * wdw->font_shake_factor;

				if (*str == '\\') {
					if (hc_to_rgba((str + 1), wdw->font->color)) {
#if DEBUG
						if (strnlen(str, 2) > 1)
							printf("unable to extract hex rgba info from '%s'", str);
#endif
					} else {
#if DEBUG
						printf("%s yielded r: %d g: %d b: %d a: %d\n", str, color[0], color[1], color[2], color[3]);
#endif
						glColor4ubv((const GLubyte*) wdw->font->color);
						str += 9;
						continue;
					}
				}

				float fw = wdw->font->font_width;
				float fh = wdw->font->font_height;
				float tw = wdw->font->tex_width;
				float th = wdw->font->tex_height;
				float zoom_x = zoom * ((((float)rand() / RAND_MAX)) * jit_factor) * wdw->font_zoom_factor;

				glColor4ub(0, 0, 0, 255);

				glTexCoord2f(fw * (*str + 0) / tw, fh / th);
				glVertex2i(      fw * pos * zoom + 2 + rnd_x - zoom_x, -2 + rnd_y - zoom_x); // 0, 0
				glTexCoord2f(fw * (*str + 1) / tw, fh / th);
				glVertex2i(fw * (pos + 1) * zoom + 2 + rnd_x + zoom_x, -2 + rnd_y - zoom_x); // 1, 0

				glTexCoord2f(fw * (*str + 1) / tw, 0);
				glVertex2i(fw * (pos + 1) * zoom + 2 + rnd_x + zoom_x, fh * zoom - 2 + rnd_y + zoom_x); // 1, 1
				glTexCoord2f(fw * (*str + 0) / tw, 0);
				glVertex2i(      fw * pos * zoom + 2 + rnd_x - zoom_x, fh * zoom - 2 + rnd_y + zoom_x); // 0, 1

				glColor4ubv((const GLubyte*) wdw->font->color);

				glTexCoord2f(fw * (*str + 0) / tw, fh / th);
				glVertex2i(      fw * pos * zoom + rnd_x - zoom_x, rnd_y - zoom_x); // 0, 0
				glTexCoord2f(fw * (*str + 1) / tw, fh / th);
				glVertex2i(fw * (pos + 1) * zoom + rnd_x + zoom_x, rnd_y - zoom_x); // 1, 0

				glTexCoord2f(fw * (*str + 1) / tw, 0);
				glVertex2i(fw * (pos + 1) * zoom + rnd_x + zoom_x, fh * zoom + rnd_y + zoom_x); // 1, 1
				glTexCoord2f(fw * (*str + 0) / tw, 0);
				glVertex2i(      fw * pos * zoom + rnd_x - zoom_x, fh * zoom + rnd_y + zoom_x); // 0, 1

				str++;
				pos++;

				if (*str == '\n') {
					str++;
					line_breaks++;
					y -= wdw->font->font_height * zoom;
					pos = 0;
					break;
				}
			}
		}
		glEnd();
		glPopMatrix();
	}
	GL_OrthoOff();
}

static unsigned int
pw2roundup(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

static size_t
readline(char* p, size_t p_len, char* dest, size_t d_len)
{
	size_t n = 0;

	while (n < d_len - 1 && p_len > 0 && p_len - n > 0) {
		dest[n] = p[n];
		n++;

		if (p_len - n == 0) {
			dest[n] = '\0';
			break;
		}

		if (p[n] == '\n') {
			dest[n] = '\0';
			n++;
			break;
		}
	}

	return n;
}

unsigned char*
TextureFromBDF(const char* file,
               size_t file_len,
               int nchar,
               int* font_w,
               int* font_h,
               bool font_d)
{
	uint32_t* tex = NULL;

	enum state { NONE,
	             FONTBOUNDINGBOX,
	             ENCODING,
	             BBX,
	             BITMAP,
	             PARSE,
	             DONE
	           } state = NONE;

	char line[MODP_STR_LENGTH];
	int bb_w = 0;
	int bb_h = 0;
	int encoding = 0;
	int w = 0;
	int h = 0;
	int nh = 0;
	int nhskip = 0;

	unsigned int ch;

	size_t img_size = 0;
	size_t n = 0;
	size_t offset = 0;
	char* p = (char*) file;

	while ((n = readline(p + offset,
	                     file_len - offset,
	                     line, MODP_STR_LENGTH))) {
		offset += n;

		switch (state) {
		case NONE:
			state = FONTBOUNDINGBOX;
			break;
		case FONTBOUNDINGBOX:
			if (sscanf((const char*) line,
			           "FONTBOUNDINGBOX %d %d",
			           &bb_w, &bb_h) == 2) {
				if (tex != NULL) {
					fprintf(stderr, "%s: FONTBOUNDINGBOX defined "
					                "more than once\n", __func__);
					goto error;
				}
				if (!(bb_w > 0 && bb_h > 0 &&
				        bb_w <= 32 && bb_h <= 32)) {
					fprintf(stderr, "%s: FONTBOUNDINGBOX size"
					                "out of bounds\n", __func__);
					goto error;
				}

				img_size = bb_w * bb_h * nchar * (font_d ? 2 : 1);

				tex = (uint32_t*) calloc(img_size, sizeof(uint32_t));
				assert(tex);
				state = ENCODING;
			}
			break;
		case ENCODING:
			nh = 0;
			if (sscanf((const char*) line,
			           "ENCODING %d",
			           &encoding) == 1) {
				state = BBX;
			}
			break;
		case BBX:
			if (sscanf((const char*) line,
			           "BBX %d %d",
			           &w, &h) == 2) {
				if (w != bb_w || h > bb_h) {
					fprintf(stderr, "%s: Unsupported BBX width/height\n",
					                __func__);
					goto error;
				}
				nhskip = h - bb_h;
				state = BITMAP;
			}
			break;
		case BITMAP:
			if (strncmp((const char*) line,
			            "BITMAP", 6) == 0)
				state = PARSE;
			break;
		case PARSE:
			if (nhskip > 0 && nhskip--)
				continue;
			if (sscanf((const char*) line,
			           "%x", &ch) == 1 && encoding < nchar) {
				ch >>= pw2roundup(w) - w;
				int xoffset = encoding * bb_w;
				int yoffset = nh * nchar * bb_w * (font_d ? 2 : 1);
				for (int i = w - 1; i >= 0; i--) {
					if ((ch >> i) & 1) {
						size_t idx = xoffset + yoffset + ((w - 1) - i);
						if (idx >= img_size) {
							fprintf(stderr, "%s: Unsupported font\n",
							                __func__);
							goto error;
						}
						tex[idx] = UINT32_MAX;
						if (font_d)
							tex[idx + (nchar * bb_w)] = UINT32_MAX;
					}
				}
			}
			if (nh++ == bb_h - 1 || strncmp((const char*) line,
			                                "ENDCHAR", 7) == 0)
				state = ENCODING;
			break;
		default:
			break;
		}
	}

	*font_w = bb_w;
	*font_h = bb_h * (font_d ? 2 : 1);

	return (unsigned char*) tex;

error:
	if (tex != NULL)
		free(tex);

	return NULL;
}

void
Font_Destroy(Font* f)
{
	assert(f);

	free(f->pixels);
	free(f);
}

Font*
Font_Init(char* filename,
          bool dblheight)
{
	Font* f = NULL;
	unsigned char* im = NULL;
	int fw, fh;
	const int nchar = 256; // number of characters supported (extended ASCII)

	size_t fontfile_len = 0;
	char* fontfile_data;

	f = (Font*) calloc(1, sizeof(Font));
	assert(f);

	fontfile_data = LocalDir_ReadFile(filename,
	                                  &fontfile_len,
	                                  MODP_MAX_FILESIZE);

	if (fontfile_data == NULL) {
		fontfile_data = (char*) neep;
		fontfile_len = sizeof(neep);
	}

	im = TextureFromBDF(fontfile_data, fontfile_len, nchar,
	                        &fw, &fh, dblheight);

	if (im == NULL)
		goto error;

	f->font_width = fw;
	f->font_height = fh;

	f->tex_width = fw * nchar;
	f->tex_height = fh;

	f->color[0] = f->color[1] = f->color[2] = f->color[3] = 255;

	glGenTextures(1, &f->tex_handle);
	glBindTexture(GL_TEXTURE_2D, f->tex_handle);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, 4, f->tex_width, f->tex_height,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, im);

	f->pixels = im;

	return f;

error:
	if (f != NULL)
		free(f);

	if (im != NULL)
		free(im);

	if (fontfile_data != (char*) neep && fontfile_data != NULL)
		free(fontfile_data);

	return NULL;
}
