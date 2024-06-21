#include <regex>
#include <thread>

#include "cdrom.h"
#include "libretro_common.h"
#include "libretro_log.h"
#include "misc.h"
#include "neocd_endian.h"
#include "neogeocd.h"
#include "path.h"

Cdrom::Cdrom() :
    m_currentPosition(0),
    m_isPlaying(false),
    m_currentTrack(nullptr),
    m_circularBuffer(),
    m_audioWorkerThreadCreated(false),
    m_exitFlag(false),
#ifndef SYNC_CDROM
    m_workerThread(),
    m_workerMutex(),
    m_workerConditionVariable(),
#endif
    m_file(nullptr),
    m_imageFile(),
    m_chdFile(),
    m_flacFile(),
    m_oggFile(),
    m_wavFile(),
    m_toc()
{
    initialize();
    m_circularBuffer.set_capacity(1048576);
}

Cdrom::~Cdrom()
{
    cleanup();
}

void Cdrom::createWorkerThread()
{
    m_exitFlag = false;

    if (!m_audioWorkerThreadCreated)
    {
        m_audioWorkerThreadCreated = true;
#ifndef SYNC_CDROM
        m_workerThread = std::thread(&Cdrom::audioBufferWorker, this);
#endif
    }
}

void Cdrom::endWorkerThread()
{
    m_exitFlag = true;

    if (m_audioWorkerThreadCreated)
    {
#ifndef SYNC_CDROM
        m_workerConditionVariable.notify_all();
        if (m_workerThread.joinable())
            m_workerThread.join();
#endif

        m_audioWorkerThreadCreated = false;
    }
}

void Cdrom::initialize()
{
    cleanup();

    m_isPlaying = false;
    m_currentPosition = 0;
    m_currentTrack = nullptr;
    m_toc.clear();
}

void Cdrom::reset()
{
    m_isPlaying = false;
    m_currentPosition = 0;
    m_currentTrack = nullptr;

    seek(0);
}

bool Cdrom::loadCd(const std::string& imageFile)
{
    initialize();

    if (filenameIsChd(imageFile))
    {
        if (!m_toc.loadChd(imageFile))
        {
            Libretro::Log::message(RETRO_LOG_ERROR, "Could not open CHD file: %s\n", imageFile.c_str());
            return false;
        }
    }
    else
    {
        if (!m_toc.loadCueSheet(imageFile))
        {
            Libretro::Log::message(RETRO_LOG_ERROR, "Could not open CUE file: %s\n", imageFile.c_str());
            return false;
        }
    }

    // TOC list should not be empty
    if (m_toc.isEmpty())
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Empty TOC! This is not supposed to happen.\n");
        initialize();
        return false;
    }

    seek(0);

    return true;
}

TrackIndex Cdrom::currentTrackIndex() const
{
    if (!m_currentTrack)
        return TrackIndex(0);

    return m_currentTrack->trackIndex;
}

uint32_t Cdrom::currentTrackPosition() const
{
    if (!m_currentTrack)
        return 0;

    return m_currentTrack->startSector;
}

uint32_t Cdrom::currentIndexSize() const
{
    if (!m_currentTrack)
        return 0;

    return m_currentTrack->trackLength;
}

const CdromToc::Entry *Cdrom::currentTrack() const
{
    return m_currentTrack;
}

uint8_t Cdrom::firstTrack() const
{
    return m_toc.firstTrack();
}

uint8_t Cdrom::lastTrack() const
{
    return m_toc.lastTrack();
}

uint32_t Cdrom::trackPosition(uint8_t track) const
{
    TrackIndex index(track, 1);

    const CdromToc::Entry* entry = m_toc.findTocEntry(index);
    if (!entry)
        return 0;

    return entry->startSector;
}

bool Cdrom::trackIsData(uint8_t track) const
{
    TrackIndex index(track, 1);

    const CdromToc::Entry* entry = m_toc.findTocEntry(index);
    if (!entry)
        return false;

    return (entry->trackType == CdromToc::TrackType::Mode1_2048) || (entry->trackType == CdromToc::TrackType::Mode1_2352);
}

uint32_t Cdrom::leadout() const
{
    return m_toc.totalSectors();
}

uint32_t Cdrom::position() const
{
    return m_currentPosition;
}

void Cdrom::increasePosition()
{
    if (!m_isPlaying)
        return;

    m_currentPosition++;
    handleTrackChange(true);
}

void Cdrom::seek(uint32_t position)
{
    m_currentPosition = position;

    handleTrackChange(false);

    if (!m_file)
        return;

    seekAudio();
}

void Cdrom::play()
{
    m_isPlaying = true;
}

bool Cdrom::isPlaying() const
{
    return m_isPlaying;
}

void Cdrom::stop()
{
    m_isPlaying = false;
}

void Cdrom::readData(char* buffer)
{
    if ((!m_currentTrack) || (!isData()) || !m_file || (m_currentPosition >= leadout()))
    {
        std::memset(buffer, 0, 2048);
        return;
    }

    uint32_t trackOffset = m_currentPosition - m_currentTrack->startSector;

    if (m_currentTrack->trackType == CdromToc::TrackType::Mode1_2048)
    {
        if (m_file->isChd())
            trackOffset *= 2352;
        else
            trackOffset *= 2048;
    }
    else if (m_currentTrack->trackType == CdromToc::TrackType::Mode1_2352)
        trackOffset = (trackOffset * 2352) + 16;

    m_file->seek(trackOffset + m_currentTrack->fileOffset);
    uint32_t done = static_cast<uint32_t>(m_file->readData(buffer, 2048));

    if (done < 2048)
        std::memset(buffer + done, 0, 2048 - done);
}

void Cdrom::fillCircularBuffer()
{
     char buffer[3000];
     size_t slice = std::min(size_t(3000), m_circularBuffer.availableToWrite());
     readAudioDirect(buffer, slice);
     m_circularBuffer.push_back(buffer, slice);
}

#ifndef SYNC_CDROM
void Cdrom::audioBufferWorker()
{
    while (1)
    {
        std::unique_lock<std::mutex> lock(m_workerMutex);
        m_workerConditionVariable.wait(lock, [&]{ return (m_circularBuffer.availableToWrite() && isAudio() && isPlaying()) || (m_exitFlag); });

        if (m_exitFlag)
            break;

	fillCircularBuffer();

        lock.unlock();
        m_workerConditionVariable.notify_one();
    }
}
#endif

bool Cdrom::filenameIsChd(const std::string &path)
{
    return (string_compare_insensitive(path_get_extension(path.c_str()), "CHD"));
}

void Cdrom::readAudio(char* buffer, size_t size)
{
#ifndef SYNC_CDROM
    std::unique_lock<std::mutex> lock(m_workerMutex);
    m_workerConditionVariable.wait(lock, [&]{ return (m_circularBuffer.availableToRead() >= size); });
#else
    while (m_circularBuffer.availableToRead() < size)
	    fillCircularBuffer();
#endif

    m_circularBuffer.pop_front(buffer, size);

#ifndef SYNC_CDROM
    lock.unlock();
    m_workerConditionVariable.notify_one();
#endif
}

void Cdrom::readAudioDirect(char* buffer, size_t size)
{
    if ((!m_currentTrack) || (!isAudio()) || !m_file || (m_currentPosition >= leadout()))
    {
        std::memset(buffer, 0, size);
        return;
    }

    size_t done = 0;

    if (m_currentTrack->trackType == CdromToc::TrackType::AudioPCM)
    {
        done = m_file->readAudio(buffer, size);
    }
    else if (m_currentTrack->trackType == CdromToc::TrackType::AudioFlac)
    {
        done = m_flacFile.read(buffer, size);
    }
    else if (m_currentTrack->trackType == CdromToc::TrackType::AudioOgg)
    {
        done = m_oggFile.read(buffer, size);
    }
    else if (m_currentTrack->trackType == CdromToc::TrackType::AudioWav)
    {
        done = static_cast<size_t>(m_wavFile.read(buffer, static_cast<int64_t>(size)));
    }

#ifdef BIG_ENDIAN_MACHINE
    if ((m_currentTrack->trackType == CdromToc::TrackType::AudioPCM ||
	m_currentTrack->trackType == CdromToc::TrackType::AudioWav)  && (!m_file->isChd())) {
        int i;
	uint16_t *buffer16 = (uint16_t *) buffer;
	for (i = 0; i < size / 2; i++) {
	  buffer16[i] = BYTE_SWAP_16(buffer16[i]);
	}
    }
#endif

    if (done < size)
    {
        std::memset(buffer + done, 0, size - done);
    }
}

bool Cdrom::isData() const
{
    if (!m_currentTrack)
        return false;

    return (m_currentTrack->trackType == CdromToc::TrackType::Mode1_2048) || (m_currentTrack->trackType == CdromToc::TrackType::Mode1_2352);
}

bool Cdrom::isAudio() const
{
    if (!m_currentTrack)
        return false;

    return !isData();
}

bool Cdrom::isPregap() const
{
    if (!m_currentTrack)
        return false;

    return (m_currentTrack->trackIndex.index() == 0);
}

bool Cdrom::isTocEmpty() const
{
    return m_toc.isEmpty();
}

void Cdrom::cleanup()
{
    m_oggFile.cleanup();
    m_flacFile.cleanup();
    m_wavFile.cleanup();

    if (m_imageFile.isOpen())
        m_imageFile.close();

    if (m_chdFile.isOpen())
        m_chdFile.close();

    m_file = nullptr;
}

bool Cdrom::hasFileChanged(const CdromToc::Entry *current) const
{
    if (!m_currentTrack)
        return true;

    if (m_currentTrack->trackIndex == current->trackIndex)
        return false;

    return m_currentTrack->fileIndex != current->fileIndex;
}

void Cdrom::handleTrackChange(bool doInitialSeek)
{
    if (m_toc.isEmpty())
        return;

    const CdromToc::Entry* current = m_toc.findTocEntry(m_currentPosition);

    if (current == m_currentTrack)
        return;

    if (!hasFileChanged(current))
    {
        m_currentTrack = current;
        return;
    }

#ifndef SYNC_CDROM
    std::unique_lock<std::mutex> lock(m_workerMutex);
#endif

    m_circularBuffer.clear();

    cleanup();

    m_currentTrack = current;

    if (m_currentTrack->trackType == CdromToc::TrackType::Silence)
    {
#ifndef SYNC_CDROM
        lock.unlock();
        m_workerConditionVariable.notify_one();
#endif
        return;
    }

    std::string filename = m_toc.fileList().at(static_cast<size_t>(m_currentTrack->fileIndex)).fileName;

    if (filenameIsChd(filename))
    {
        m_chdFile.open(filename);
        m_file = &m_chdFile;
    }
    else
    {
        m_imageFile.open(filename);
        m_file = &m_imageFile;

        if (m_currentTrack->trackType == CdromToc::TrackType::AudioFlac)
            m_flacFile.initialize(m_file);
        else if (m_currentTrack->trackType == CdromToc::TrackType::AudioOgg)
            m_oggFile.initialize(m_file);
        else if (m_currentTrack->trackType == CdromToc::TrackType::AudioWav)
            m_wavFile.initialize(m_file);
    }

    if (doInitialSeek)
    {
#ifndef SYNC_CDROM
        lock.unlock();
#endif
        seekAudio();
    }
}

void Cdrom::seekAudio()
{
    if ((!m_currentTrack) || (!m_file) || !isAudio())
        return;

#ifndef SYNC_CDROM
    std::unique_lock<std::mutex> lock(m_workerMutex);
#endif

    m_circularBuffer.clear();

    uint32_t trackOffset = (std::min(m_currentPosition, leadout() - 1) - m_currentTrack->startSector) * 2352;

    // Now seek according to the track type
    if (m_currentTrack->trackType == CdromToc::TrackType::AudioPCM)
        m_file->seek(trackOffset + m_currentTrack->fileOffset);
    else if (m_currentTrack->trackType == CdromToc::TrackType::AudioFlac)
        m_flacFile.seek(trackOffset + m_currentTrack->fileOffset);
    else if (m_currentTrack->trackType == CdromToc::TrackType::AudioOgg)
        m_oggFile.seek(trackOffset + m_currentTrack->fileOffset);
    else if (m_currentTrack->trackType == CdromToc::TrackType::AudioWav)
        m_wavFile.seek(static_cast<int64_t>(trackOffset + m_currentTrack->fileOffset));

#ifndef SYNC_CDROM
    lock.unlock();
    m_workerConditionVariable.notify_one();
#endif
}

DataPacker& operator<<(DataPacker& out, const Cdrom& cdrom)
{
    out << cdrom.m_currentPosition;
    out << cdrom.m_isPlaying;

    return out;
}

DataPacker& operator>>(DataPacker& in, Cdrom& cdrom)
{
    in >> cdrom.m_currentPosition;
    in >> cdrom.m_isPlaying;

    neocd->cdrom.seek(cdrom.m_currentPosition);

    return in;
}
