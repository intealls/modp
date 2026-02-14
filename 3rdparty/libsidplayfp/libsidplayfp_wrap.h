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
void                   deleteReSIDfpBuilder(ReSIDfpBuilder*);

struct SidConfig*      newSidConfig();
void                   deleteSidConfig(SidConfig*);

struct sidplayfp*      newSidEngine();
void                   deleteSideEngine(sidplayfp*);
unsigned int           initSidEngine(sidplayfp*, ReSIDfpBuilder*, unsigned int, unsigned int);
bool                   isPlayingSidEngine(sidplayfp*);

struct SidTune*        newSidTune(const void*, unsigned int);
void                   deleteSidTune(SidTune*);

unsigned int           startSongSidTune(SidTune*);
unsigned int           songsSidTune(SidTune*);
unsigned int           numberOfInfoStringsSidTune(SidTune*);
const char*            infoStringSidTune(SidTune*, unsigned int);
unsigned int           numberOfCommentStringsSidTune(SidTune*);
const char*            commentStringSidTune(SidTune*, unsigned int);
unsigned int           selectSongSidTune(SidTune*, unsigned int);
unsigned int           currentSongSidTune(SidTune*);

bool                   getStatusSidTune(SidTune*);
unsigned int           loadSidTune(struct SidTune*, struct sidplayfp*);
unsigned int           playSidEngine(sidplayfp*, short*, size_t);

#ifdef __cplusplus
}
#endif

#endif /*  __LIBSIDPLAYFP_WRAP_H_ */
