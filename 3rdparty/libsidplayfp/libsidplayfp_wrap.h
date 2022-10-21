#ifndef __LIBSIDPLAYFP_WRAP_H_
#define __LIBSIDPLAYFP_WRAP_H_

#ifdef __cplusplus

#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/builders/residfp.h>
#include "sidplayfp/siddefs.h"
#include <sidplayfp/SidTuneInfo.h>

extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ReSIDfpBuilder ReSIDfpBuilder;
typedef struct SidConfig SidConfig;
typedef struct sidplayfp sidplayfp;
typedef struct SidTune SidTune;

struct ReSIDfpBuilder* newReSIDfpBuilder();
void deleteReSIDfpBuilder(ReSIDfpBuilder *c);

struct SidConfig* newSidConfig();
void deleteSidConfig(SidConfig *c);

struct sidplayfp* newSidEngine();
void deleteSideEngine(sidplayfp *c);
unsigned int initSidEngine(sidplayfp *m_engine, ReSIDfpBuilder *rs, unsigned int channels, unsigned int samplerate);
bool isPlayingSidEngine(sidplayfp *m_engine);

struct SidTune* newSidTune(const void *buf, unsigned int buflen);
void deleteSidTune(SidTune *c);

unsigned int startSongSidTune(SidTune *m_tune);
unsigned int songsSidTune(SidTune *m_tune);
unsigned int numberOfInfoStringsSidTune(SidTune *m_tune);
const char* infoStringSidTune(SidTune *m_tune, unsigned int n);
unsigned int numberOfCommentStringsSidTune(SidTune *m_tune);
const char* commentStringSidTune(SidTune *m_tune, unsigned int n);
unsigned int selectSongSidTune(SidTune *m_tune, unsigned int n);
unsigned int currentSongSidTune(SidTune *m_tune);

bool getStatusSidTune(SidTune *m_tune);
unsigned int loadSidTune(struct SidTune *m_tune, struct sidplayfp *m_engine);
unsigned int playSidEngine(sidplayfp *m_engine, short *buf, size_t buffer_samples);

#ifdef __cplusplus
}
#endif

#endif /*  __LIBSIDPLAYFP_WRAP_H_ */
