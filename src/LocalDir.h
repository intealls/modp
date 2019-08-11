// Copyright intealls
// License: GPL v3

#ifndef SRC_LOCALDIR_H_
#define SRC_LOCALDIR_H_

#include "Directory.h"

Directory* LocalDir_Create(const char*);
void*      LocalDir_ReadFile(const char*, size_t*, size_t);

#endif /* SRC_LOCALDIR_H_ */
