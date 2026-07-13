#include <stdlib.h>
#include <iostream>

// wrapper
#include "libsidplayfp_wrap.h"

SidConfig*
newSidConfig()
{
	return new SidConfig();
}

void
deleteSidConfig(SidConfig *c)
{
	delete c;
}

SIDLiteBuilder*
newSIDLiteBuilder()
{
	return new SIDLiteBuilder("modp_libsidplayfp_wrapper");
}

void
deleteSIDLiteBuilder(SIDLiteBuilder *c)
{
	delete c;
}

sidplayfp*
newSidEngine()
{
	return new sidplayfp();
}

void
deleteSideEngine(sidplayfp *c)
{
	delete c;
}

void
setRomsSidEngine(sidplayfp *m_engine,
                 const uint8_t *kernal,
                 const uint8_t *basic,
                 const uint8_t *chargen)
{
	m_engine->setRoms(kernal, basic, chargen);
}

unsigned int
initSidEngine(sidplayfp *m_engine,
              SIDLiteBuilder *rs,
              unsigned int channels,
              unsigned int samplerate)
{
	SidConfig e_config;

	e_config.frequency = samplerate;
	e_config.samplingMethod = SidConfig::INTERPOLATE;
	e_config.sidEmulation = rs;

	if (!m_engine->config(e_config)) {
		std::cerr << m_engine->error() << std::endl;
		return 0;
	}

	return 1;
}

SidTune*
newSidTune(const void *buf, unsigned int buflen)
{
	return new SidTune((const unsigned char*) buf, buflen);
}

unsigned int
loadSidTune(SidTune *m_tune, sidplayfp *m_engine)
{
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

void
initMixerSidEngine(sidplayfp *m_engine, bool stereo)
{
	m_engine->initMixer(stereo);
}

int
getBufSizeSidEngine(sidplayfp *m_engine, unsigned int cycles)
{
	return m_engine->getBufSize(cycles);
}

int
playSidEngineCycles(sidplayfp *m_engine, unsigned int cycles)
{
	return m_engine->play(cycles);
}

unsigned int
mixSidEngine(sidplayfp *m_engine, short *buf, unsigned int samples)
{
	return m_engine->mix(buf, samples);
}

unsigned int
startSongSidTune(SidTune *m_tune)
{
	const SidTuneInfo* tune_info = m_tune->getInfo();
	return tune_info->startSong();
}

unsigned int
currentSongSidTune(SidTune *m_tune)
{
	const SidTuneInfo* tune_info = m_tune->getInfo();
	return tune_info->currentSong();
}

unsigned int
songsSidTune(SidTune *m_tune)
{
	const SidTuneInfo* tune_info = m_tune->getInfo();
	return tune_info->songs();
}

unsigned int
numberOfInfoStringsSidTune(SidTune *m_tune)
{
	const SidTuneInfo* tune_info = m_tune->getInfo();
	return tune_info->numberOfInfoStrings();
}

const char*
infoStringSidTune(SidTune *m_tune, unsigned int n)
{
	const SidTuneInfo* tune_info = m_tune->getInfo();
	return tune_info->infoString(n);
}

unsigned int
numberOfCommentStringsSidTune(SidTune *m_tune)
{
	const SidTuneInfo* tune_info = m_tune->getInfo();
	return tune_info->numberOfCommentStrings();
}

const char*
commentStringSidTune(SidTune *m_tune, unsigned int n)
{
	const SidTuneInfo* tune_info = m_tune->getInfo();
	return tune_info->commentString(n);
}

unsigned int
selectSongSidTune(SidTune *m_tune, unsigned int n)
{
	return m_tune->selectSong(n);
}

bool
getStatusSidTune(SidTune *m_tune)
{
	return m_tune->getStatus();
}

void
deleteSidTune(SidTune *c)
{
	delete c;
}
