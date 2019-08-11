// Copyright intealls
// License: GPL v3

#ifndef GL_H_
#define GL_H_

#include <stdbool.h>

void GL_DrawRec(int, int, int, int, bool, int, int);
void GL_OrthoOn(int, int);
void GL_OrthoOff(void);
void GL_Clear(void);
void GL_Init(int, int, float, float, float);

#endif
