// Copyright intealls
// License: GPL v3

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include "GLWindow.h"

void
GL_OrthoOn(int width, int height)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, width, 0, height, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
}

void
GL_OrthoOff()
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void
GL_DrawRec(int x, int y,
           int width, int height,
           bool notexture,
           int wdw_width, int wdw_height)
{
	if (notexture)
		glDisable(GL_TEXTURE_2D);

	GL_OrthoOn(wdw_width, wdw_height);
	glBegin(GL_QUADS);
	{
		glVertex2i(x, y);
		glVertex2i(x + width, y);
		glVertex2i(x + width, y - height);
		glVertex2i(x, y - height);
	}
	glEnd();
	GL_OrthoOff();

	if (notexture)
		glEnable(GL_TEXTURE_2D);
}

void
GL_Clear()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void
GL_Init(int width, int height, float clr_r, float clr_g, float clr_b)
{
	float ratio = (float) width / height;

	glClearColor(clr_r, clr_g, clr_b, 0);

	glEnable(GL_TEXTURE_2D);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, ratio, 0.01f, 2048.0f);
}
