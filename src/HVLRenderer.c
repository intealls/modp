// two smokes, let's go!
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <stddef.h>

#include <iconv.h>

#include "../3rdparty/hvl/hvl_replay.h"
#include "HVLRenderer.h"
#include "Globals.h"

#define hvl_load   hvl_ParseTune

static bool
HVLRenderer_CanLoad(const AudioRenderer* obj,
                        const void* data,
                        const size_t len)
{
	int r = 1;

	DataObject(rndr_data, obj);

	size_t headerlen = ;

	r = openmpt_probe(OPENMPT_PROBE_FILE_HEADER_FLAGS_DEFAULT,
	                  data, headerlen, len, OpenMPTRenderer_LogFunc,
	                  NULL, NULL, NULL, NULL, NULL);

	return r == OPENMPT_PROBE_FILE_HEADER_RESULT_SUCCESS;
}
