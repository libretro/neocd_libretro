#include <regex>

#include "cdrom.h"
#include "neogeocd.h"

class cueSourceFile
{
public:
    cueSourceFile(uint32_t ss, uint32_t sc) : sectorSize(ss), sectorCount(sc), entries()
    {}

    uint32_t sectorSize;
    uint32_t sectorCount;
    std::map<TrackIndex, uint32_t> entries;
};

#if defined(WIN32) || defined(WIN64)
#define strcasecmp _stricmp
#endif

static bool stringCompareInsensitive(const std::string& a, const std::string& b)
{
    if (a.length() != b.length())
        return false;

    return (strcasecmp(a.c_str(), b.c_str()) == 0);
}

static std::string pathReplaceFilename(const std::string& path, const std::string& newFilename)
{
    static const std::regex pathWithoutFilenameRegex("^(.*[\\\\/]).+$");

    std::smatch matchResults;
    std::regex_match(path, matchResults, pathWithoutFilenameRegex);
    if (matchResults.size() < 2)
        return newFilename;

    return matchResults[1].str() + newFilename;
}

static bool fileIsFlac(const std::string& path)
{
    static const std::regex fileIsFlacRegex("^.*\\.flac$", std::regex_constants::icase);
    return std::regex_match(path, fileIsFlacRegex);
}

static bool fileIsOgg(const std::string& path)
{
    static const std::regex fileIsOggRegex("^.*\\.ogg$", std::regex_constants::icase);
    return std::regex_match(path, fileIsOggRegex);
}

static bool fileIsIso(const std::string& path)
{
    static const std::regex fileIsIsoRegex("^.*\\.iso$", std::regex_constants::icase);
    return std::regex_match(path, fileIsIsoRegex);
}

static bool fileIsImage(const std::string& path)
{
    static const std::regex fileIsImageRegex("^.*\\.(?:bin|img)$", std::regex_constants::icase);
    return std::regex_match(path, fileIsImageRegex);
}

static bool fileIsWav(const std::string& path)
{
    static const std::regex fileIsWavRegex("^.*\\.wav$", std::regex_constants::icase);
    return std::regex_match(path, fileIsWavRegex);
}

Cdrom::Cdrom() :
    m_currentPosition(0),
    m_isPlaying(false),
    m_currentTrack(nullptr),
    m_circularBuffer(),
    m_audioWorkerThreadCreated(false),
    m_exitFlag(false),
    m_workerThread(),
    m_workerMutex(),
    m_workerConditionVariable(),
    m_file(),
    m_flacFile(),
    m_oggFile(),
    m_wavFile(),
    m_leadout(0),
    m_toc(),
    m_tocByTrack(),
    m_tocByLBA()
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
        m_workerThread = std::thread(&Cdrom::audioBufferWorker, this);
    }
}

void Cdrom::endWorkerThread()
{
    m_exitFlag = true;

    if (m_audioWorkerThreadCreated)
    {
        m_workerConditionVariable.notify_all();
        if (m_workerThread.joinable())
            m_workerThread.join();

        m_audioWorkerThreadCreated = false;
    }
}

void Cdrom::initialize()
{
    cleanup();

    m_isPlaying = false;
    m_currentPosition = 0;
    m_currentTrack = nullptr;
    m_leadout = 0;
    m_toc.clear();
    m_tocByTrack.clear();
    m_tocByLBA.clear();
}

void Cdrom::reset()
{
    m_isPlaying = false;
    m_currentPosition = 0;
    m_currentTrack = nullptr;
    seek(0);
}

bool Cdrom::findFileLength(const std::string& filename, size_t& length, uint32_t& sectorSize)
{
    std::ifstream in;
    in.open(filename, std::ios::in | std::ios::binary);
    if (!in.is_open())
    {
        LOG(LOG_ERROR, "Could not open file: %s\n", filename.c_str());
        return false;
    }

    sectorSize = 2352;

    if (fileIsIso(filename))
    {
        in.seekg(0, std::ios::end);
        length = static_cast<size_t>(in.tellg());
        sectorSize = 2048;
    }
    else if (fileIsImage(filename))
    {
        in.seekg(0, std::ios::end);
        length = static_cast<size_t>(in.tellg());
    }
    else if (fileIsWav(filename))
    {
        if (!m_wavFile.initialize(&in))
        {
            LOG(LOG_ERROR, "Could not open WAVE file: %s\nFile was not found or had incorrect format.\n", filename.c_str());
            return false;
        }

        length = m_wavFile.length();
        m_wavFile.cleanup();
    }
    else if (fileIsFlac(filename))
    {
        if (!m_flacFile.initialize(&in))
        {
            LOG(LOG_ERROR, "Could not open FLAC file: %s\nFile was not found or had incorrect format.\n", filename.c_str());
            return false;
        }

        length = m_flacFile.length();
        m_flacFile.cleanup();
    }
    else if (fileIsOgg(filename))
    {
        if (!m_oggFile.initialize(&in))
        {
            LOG(LOG_ERROR, "Could not open OGG file: %s\nFile was not found or had incorrect format.\n", filename.c_str());
            return false;
        }

        length = m_oggFile.length();
        m_oggFile.cleanup();
    }

    in.close();
    return true;
}

bool Cdrom::loadCue(const std::string& cueFile)
{
    static const std::regex fileDirective("^\\s*FILE\\s+\"(.*)\"\\s+(\\S+)\\s*$", std::regex_constants::icase);
    static const std::regex trackDirective("^\\s*TRACK\\s+([0-9]+)\\s+(\\S*)\\s*$", std::regex_constants::icase);
    static const std::regex indexDirective("^\\s*INDEX\\s+([0-9]+)\\s+([0-9]+):([0-9]+):([0-9]+)\\s*$", std::regex_constants::icase);
    static const std::regex pregapDirective("^\\s*PREGAP\\s+([0-9]+):([0-9]+):([0-9]+)\\s*$", std::regex_constants::icase);

    initialize();

    std::ifstream in;
    in.open(cueFile, std::ios::in);
    if (!in.is_open())
    {
        LOG(LOG_ERROR, "Could not open CUE file: %s\n", cueFile.c_str());
        return false;
    }

    std::map<std::string, cueSourceFile> sourceFiles;
    std::string currentFile;
    uint32_t currentTrack;
    TrackType currentType;

    // On the first pass we create the toc table, but with empty position and sizes.
    // We also create a list of source files, with all trackindexes attached to them and their position in the file
    // We also probe all source files to know total size and sector size
    while (!in.eof())
    {
        std::string line;
        std::getline(in, line);

        std::smatch matchResults;
        if (std::regex_match(line, matchResults, fileDirective))
        {
            currentFile = pathReplaceFilename(cueFile, matchResults[1].str());

            if (sourceFiles.find(currentFile) == sourceFiles.end())
            {
                size_t length;
                uint32_t sectorSize;

                if (!findFileLength(currentFile, length, sectorSize))
                    return false;

                sourceFiles.emplace(currentFile, cueSourceFile(sectorSize, static_cast<uint32_t>(length / sectorSize)));
            }
        }
        else if (std::regex_match(line, matchResults, trackDirective))
        {
            currentTrack = std::stoul(matchResults[1].str());

            std::string fileType = matchResults[2].str();

            if (stringCompareInsensitive(fileType, "MODE1/2048"))
                currentType = TrackType::Mode1_2048;
            else if (stringCompareInsensitive(fileType, "MODE1/2352"))
                currentType = TrackType::Mode1_2352;
            else if (stringCompareInsensitive(fileType, "AUDIO"))
            {
                if (fileIsImage(currentFile))
                    currentType = TrackType::AudioPCM;
                else if (fileIsFlac(currentFile))
                    currentType = TrackType::AudioFlac;
                else if (fileIsOgg(currentFile))
                    currentType = TrackType::AudioOgg;
                else if (fileIsWav(currentFile))
                    currentType = TrackType::AudioWav;
                else
                {
                    LOG(LOG_ERROR, "File %s is not recognized as a supported type.\n", currentFile.c_str());
                    return false;
                }
            }
            else
            {
                LOG(LOG_ERROR, "File type %s is unknown.\n", fileType.c_str());
                return false;
            }
        }
        else if (std::regex_match(line, matchResults, pregapDirective))
        {
            uint8_t m = static_cast<uint8_t>(std::stoul(matchResults[1].str()));
            uint8_t s = static_cast<uint8_t>(std::stoul(matchResults[2].str()));
            uint8_t f = static_cast<uint8_t>(std::stoul(matchResults[3].str()));
            uint32_t length = fromMSF(m, s, f);

            m_toc.emplace_back(TocEntry("", TrackIndex(currentTrack, 0), TrackType::Silence, 0, 0, length));
        }
        else if (std::regex_match(line, matchResults, indexDirective))
        {
            uint8_t currentIndex = static_cast<uint8_t>(std::stoul(matchResults[1].str()));
            uint8_t m = static_cast<uint8_t>(std::stoul(matchResults[2].str()));
            uint8_t s = static_cast<uint8_t>(std::stoul(matchResults[3].str()));
            uint8_t f = static_cast<uint8_t>(std::stoul(matchResults[4].str()));
            uint32_t posInFile = fromMSF(m, s, f);

            auto i = sourceFiles.find(currentFile);
            if (i == sourceFiles.end())
            {
                LOG(LOG_ERROR, "Index directive without preceding track ?\n");
                return false;
            }

            TrackIndex currentTrackIndex(currentTrack, currentIndex);

            (*i).second.entries.emplace(currentTrackIndex, posInFile);

            m_toc.emplace_back(TocEntry(currentFile, currentTrackIndex, currentType, 0, 0, 0));
        }
    }

    in.close();

    // Next we find out the position and size of all elements of the TOC

    uint32_t currentPosition = 0;

    for (TocEntry& tocEntry : m_toc)
    {
        tocEntry.lba = currentPosition;

        // Length is already set for pregap
        if (tocEntry.entryLength == 0)
        {
            std::map<std::string, cueSourceFile>::const_iterator i;

            for (i = sourceFiles.cbegin(); i != sourceFiles.cend(); ++i)
            {
                const cueSourceFile& sourceFile = (*i).second;

                auto j = sourceFile.entries.find(tocEntry.trackIndex);
                if (j != sourceFile.entries.cend())
                {
                    auto k = j;
                    ++k;

                    if (k == sourceFile.entries.cend())
                    {
                        tocEntry.entryLength = sourceFile.sectorCount - (*j).second;
                    }
                    else
                    {
                        tocEntry.entryLength = (*k).second - (*j).second;
                    }

                    tocEntry.offset = (*j).second * sourceFile.sectorSize;
                    break;
                }
            }

            if (i == sourceFiles.cend())
            {
                LOG(LOG_ERROR, "Internal error: Track not found in the source file lists.\n");
                return false;
            }
        }

        currentPosition += tocEntry.entryLength;
    }

    m_leadout = currentPosition;

    // TOC list should not be empty
    if (!m_toc.size())
    {
        LOG(LOG_ERROR, "Empty TOC! This is not supposed to happen.\n");
        initialize();
        return false;
    }

    for (const TocEntry& entry : m_toc)
    {
        m_tocByTrack[entry.trackIndex] = &entry;
        m_tocByLBA[entry.lba] = &entry;
    }

    seek(0);

    return true;
}

const std::map<TrackIndex, const Cdrom::TocEntry*>& Cdrom::toc() const
{
    return m_tocByTrack;
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

    return m_currentTrack->lba;
}

uint32_t Cdrom::currentIndexSize() const
{
    if (!m_currentTrack)
        return 0;

    auto i = m_tocByTrack.find(m_currentTrack->trackIndex);
    if (i == m_tocByTrack.end())
        return 0;

    ++i;
    if (i == m_tocByTrack.end())
        return m_leadout - m_currentTrack->lba;

    return ((*i).second->lba) - m_currentTrack->lba;
}

const Cdrom::TocEntry* Cdrom::currentTrack() const
{
    return m_currentTrack;
}

uint8_t Cdrom::firstTrack() const
{
    if (m_tocByTrack.empty())
        return 0;

    return (*m_tocByTrack.cbegin()).second->trackIndex.track();
}

uint8_t Cdrom::lastTrack() const
{
    if (m_tocByTrack.empty())
        return 0;

    return (*m_tocByTrack.crbegin()).second->trackIndex.track();
}

uint32_t Cdrom::trackPosition(uint8_t track) const
{
    TrackIndex index(track, 1);

    auto i = m_tocByTrack.find(index);
    if (i == m_tocByTrack.end())
        return 0;

    return (*i).second->lba;
}

bool Cdrom::trackIsData(uint8_t track) const
{
    TrackIndex index(track, 1);

    auto i = m_tocByTrack.find(index);
    if (i == m_tocByTrack.end())
        return 0;

    return ((*i).second->trackType == TrackType::Mode1_2048) || ((*i).second->trackType == TrackType::Mode1_2352);
}

uint32_t Cdrom::leadout() const
{
    return m_leadout;
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

    if (!m_file.is_open())
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
    if ((!m_currentTrack) || (!isData()) || !m_file.is_open() || (m_currentPosition >= m_leadout))
    {
        std::memset(buffer, 0, 2048);
        return;
    }

    uint32_t trackOffset = m_currentPosition - m_currentTrack->lba;

    if (m_currentTrack->trackType == TrackType::Mode1_2048)
        trackOffset *= 2048;
    else if (m_currentTrack->trackType == TrackType::Mode1_2352)
        trackOffset = (trackOffset * 2352) + 16;

    m_file.seekg(trackOffset + m_currentTrack->offset, std::ios::beg);
    m_file.read(buffer, 2048);

    // Fix ifstream stupidity: eof is not a failure condition
    if (m_file.fail() && m_file.eof())
        m_file.clear();

    uint32_t done = static_cast<uint32_t>(m_file.gcount());
    if (done < 2048)
        std::memset(buffer + done, 0, 2048 - done);
}

void Cdrom::audioBufferWorker()
{
    while (1)
    {
        std::unique_lock<std::mutex> lock(m_workerMutex);
        m_workerConditionVariable.wait(lock, [&]{ return (m_circularBuffer.availableToWrite() && isAudio() && isPlaying()) || (m_exitFlag); });

        if (m_exitFlag)
            break;

        char buffer[3000];
        size_t slice = std::min(size_t(3000), m_circularBuffer.availableToWrite());
        readAudioDirect(buffer, slice);
        m_circularBuffer.push_back(buffer, slice);

        lock.unlock();
        m_workerConditionVariable.notify_one();
    }
}

void Cdrom::readAudio(char* buffer, size_t size)
{
    std::unique_lock<std::mutex> lock(m_workerMutex);
    m_workerConditionVariable.wait(lock, [&]{ return (m_circularBuffer.availableToRead() >= size); });

    m_circularBuffer.pop_front(buffer, size);

    lock.unlock();
    m_workerConditionVariable.notify_one();
}

void Cdrom::readAudioDirect(char* buffer, size_t size)
{
    if ((!m_currentTrack) || (!isAudio()) || !m_file.is_open() || (m_currentPosition >= m_leadout))
    {
        std::memset(buffer, 0, size);
        return;
    }

    size_t done = 0;

    if (m_currentTrack->trackType == TrackType::AudioPCM)
    {
        m_file.read(buffer, size);

        // Fix ifstream stupidity: eof is not a failure condition
        if (m_file.fail() && m_file.eof())
            m_file.clear();

        done = static_cast<size_t>(m_file.gcount());
    }
    else if (m_currentTrack->trackType == TrackType::AudioFlac)
    {
        done = m_flacFile.read(buffer, size);
    }
    else if (m_currentTrack->trackType == TrackType::AudioOgg)
    {
        done = m_oggFile.read(buffer, size);
    }
    else if (m_currentTrack->trackType == TrackType::AudioWav)
    {
        done = m_wavFile.read(buffer, size);
    }

    if (done < size)
    {
        std::memset(buffer + done, 0, size - done);
    }
}

bool Cdrom::isData() const
{
    if (!m_currentTrack)
        return false;

    return (m_currentTrack->trackType == TrackType::Mode1_2048) || (m_currentTrack->trackType == TrackType::Mode1_2352);
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
    return (m_toc.size() == 0);
}

void Cdrom::cleanup()
{
    if (m_file.is_open())
        m_file.close();
}

bool Cdrom::hasFileChanged(const TocEntry* current) const
{
    if (!m_currentTrack)
        return true;

    if (m_currentTrack->trackIndex == current->trackIndex)
        return false;

    return m_currentTrack->file != current->file;
}

void Cdrom::handleTrackChange(bool doInitialSeek)
{
    if (!m_tocByLBA.size())
        return;

    auto i = m_tocByLBA.upper_bound(m_currentPosition);
    --i;
    const TocEntry* current = i->second;

    if (current == m_currentTrack)
        return;

    if (!hasFileChanged(current))
    {
        m_currentTrack = current;
        return;
    }

    std::unique_lock<std::mutex> lock(m_workerMutex);

    m_circularBuffer.clear();

    cleanup();

    m_currentTrack = current;

    if (m_currentTrack->trackType == TrackType::Silence)
    {
        lock.unlock();
        m_workerConditionVariable.notify_one();
        return;
    }

    m_file.open(m_currentTrack->file, std::ios::in | std::ios::binary);

    if (m_currentTrack->trackType == TrackType::AudioFlac)
        m_flacFile.initialize(&m_file);
    else if (m_currentTrack->trackType == TrackType::AudioOgg)
        m_oggFile.initialize(&m_file);
    else if (m_currentTrack->trackType == TrackType::AudioWav)
        m_wavFile.initialize(&m_file);

    if (doInitialSeek)
    {
        lock.unlock();
        seekAudio();
    }
}

void Cdrom::seekAudio()
{
    if ((!m_currentTrack) || (!m_file.is_open()) || !isAudio())
        return;

    std::unique_lock<std::mutex> lock(m_workerMutex);

    m_circularBuffer.clear();

    uint32_t trackOffset = (std::min(m_currentPosition, m_leadout - 1) - m_currentTrack->lba) * 2352;

    // Now seek according to the track type
    if (m_currentTrack->trackType == TrackType::AudioPCM)
        m_file.seekg(trackOffset + m_currentTrack->offset);
    else if (m_currentTrack->trackType == TrackType::AudioFlac)
        m_flacFile.seek(trackOffset + m_currentTrack->offset);
    else if (m_currentTrack->trackType == TrackType::AudioOgg)
        m_oggFile.seek(trackOffset + m_currentTrack->offset);
    else if (m_currentTrack->trackType == TrackType::AudioWav)
        m_wavFile.seek(trackOffset + m_currentTrack->offset);

    lock.unlock();
    m_workerConditionVariable.notify_one();
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

    neocd.cdrom.seek(cdrom.m_currentPosition);

    return in;
}
