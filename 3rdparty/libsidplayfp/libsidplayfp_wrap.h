#ifndef __LIBSIDPLAYFP_WRAP_H_
#define __LIBSIDPLAYFP_WRAP_H_

#ifdef __cplusplus

#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/builders/sidlite.h>
#include "sidplayfp/siddefs.h"
#include <sidplayfp/SidTuneInfo.h>

extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SIDLiteBuilder SIDLiteBuilder;
typedef struct SidConfig SidConfig;
typedef struct sidplayfp sidplayfp;
typedef struct SidTune SidTune;

struct SIDLiteBuilder* newSIDLiteBuilder();
void                   deleteSIDLiteBuilder(SIDLiteBuilder*);

struct SidConfig*      newSidConfig();
void                   deleteSidConfig(SidConfig*);

struct sidplayfp*      newSidEngine();
void                   deleteSideEngine(sidplayfp*);
void                   setRomsSidEngine(sidplayfp*, const uint8_t*, const uint8_t*, const uint8_t*);
unsigned int           initSidEngine(sidplayfp*, SIDLiteBuilder*, unsigned int, unsigned int);
void                   initMixerSidEngine(sidplayfp*, bool stereo);
int                    getBufSizeSidEngine(sidplayfp*, unsigned int cycles);
int                    playSidEngineCycles(sidplayfp*, unsigned int cycles);
unsigned int           mixSidEngine(sidplayfp*, short*, unsigned int samples);

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

#ifdef __cplusplus
}
#endif

#endif /*  __LIBSIDPLAYFP_WRAP_H_ */
