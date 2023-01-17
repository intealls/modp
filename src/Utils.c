// Copyright intealls
// License: GPL v3

#include <string.h>
#include <assert.h>

void
StrCpy(char* dest, size_t dest_len, const char* src)
{
	size_t src_len = strnlen(src, dest_len);
	assert(dest_len >= src_len + 1);
	memcpy(dest, src, src_len + 1);
}
