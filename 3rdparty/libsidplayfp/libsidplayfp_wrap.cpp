#include <stdlib.h>
#include <memory>
#include <iostream>

// wrapper
#include "libsidplayfp_wrap.h"

SidConfig* newSidConfig() {
    return new SidConfig();
}

void deleteSidConfig(SidConfig *c) {
    delete c;
}

ReSIDfpBuilder* newReSIDfpBuilder() {
    return new ReSIDfpBuilder("modp_libsidplayfp_wrapper");
}

void deleteReSIDfpBuilder(ReSIDfpBuilder *c) {
    delete c;
}

sidplayfp* newSidEngine() {
    return new sidplayfp();
}

void deleteSideEngine(sidplayfp *c) {
    delete c;
}

unsigned int initSidEngine(sidplayfp *m_engine, ReSIDfpBuilder *rs, unsigned int channels, unsigned int samplerate) {
    SidConfig e_config;
    rs->create(m_engine->info().maxsids());

    if (!rs->getStatus()) {
        std::cerr << rs->error() << std::endl;
        return 0;
    }

    e_config.fastSampling = false;
    e_config.frequency = samplerate;
    e_config.playback = (channels == 1) ? SidConfig::MONO : SidConfig::STEREO;
    e_config.samplingMethod = SidConfig::INTERPOLATE;
    e_config.sidEmulation = rs;

    if (!m_engine->config(e_config)) {
        std::cerr << m_engine->error() << std::endl;
        return 0;
    }
    return 1;
}

SidTune* newSidTune(unsigned char *buf, unsigned int buflen) {
    return new SidTune(buf, buflen);
}

unsigned int loadSidTune(SidTune *m_tune, sidplayfp *m_engine) {
    if (!m_tune->getStatus()) {
        std::cerr << m_tune->statusString() << std::endl;
        return 0;
    }

    if (!m_engine->load(m_tune)) {
        std::cerr << m_engine->error() << std::endl;
        return 0;
    }

    return 1;
}

unsigned int playSidEngine(sidplayfp *m_engine, short *buf, size_t buffer_samples) {
    // unsigned int buffer_samples = ((m_engine->config().frequency * 2 * 2) / 50);
    return m_engine->play(buf, buffer_samples);
}

bool isPlayingSidEngine(sidplayfp *m_engine) {
    return m_engine->isPlaying();
}

unsigned int startSongSidTune(SidTune *m_tune) {
    const SidTuneInfo* tune_info = m_tune->getInfo();
    return tune_info->startSong();
}

unsigned int currentSongSidTune(SidTune *m_tune) {
    const SidTuneInfo* tune_info = m_tune->getInfo();
    return tune_info->currentSong();
}

unsigned int songsSidTune(SidTune *m_tune) {
    const SidTuneInfo* tune_info = m_tune->getInfo();
    return tune_info->songs();
}

unsigned int numberOfInfoStringsSidTune(SidTune *m_tune) {
    const SidTuneInfo* tune_info = m_tune->getInfo();
    return tune_info->numberOfInfoStrings();
}

const char* infoStringSidTune(SidTune *m_tune, unsigned int n) {
    const SidTuneInfo* tune_info = m_tune->getInfo();
    return tune_info->infoString(n);
}
unsigned int numberOfCommentStringsSidTune(SidTune *m_tune) {
    const SidTuneInfo* tune_info = m_tune->getInfo();
    return tune_info->numberOfCommentStrings();
}

const char* commentStringSidTune(SidTune *m_tune, unsigned int n) {
    const SidTuneInfo* tune_info = m_tune->getInfo();
    return tune_info->commentString(n);
}
unsigned int selectSongSidTune(SidTune *m_tune, unsigned int n) {
    return m_tune->selectSong(n);
}

bool getStatusSidTune(SidTune *m_tune) {
    return m_tune->getStatus();
}

void deleteSidTune(SidTune *c) {
    delete c;
}
