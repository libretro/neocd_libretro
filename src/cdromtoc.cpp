#include <algorithm>
#include <regex>
#include <string.h>

#include "cdromtoc.h"
#include "chdfile.h"
#include "flacfile.h"
#include "libretro_log.h"
#include "misc.h"
#include "oggfile.h"
#include "path.h"
#include "wavfile.h"

CdromToc::CdromToc() :
    m_toc(),
    m_fileList(),
    m_firstTrack(0),
    m_lastTrack(0),
    m_totalSectors(0)
{ }

void CdromToc::clear()
{
    m_toc.clear();
    m_fileList.clear();
    m_firstTrack = 0;
    m_lastTrack = 0;
    m_totalSectors = 0;
}

#ifdef __PSL1GHT__
namespace std
{
    long stol (const std::string& str) {
	return strtol(str.c_str(), NULL, 10);
    }
    unsigned long stoul (const std::string& str) {
        return strtoul(str.c_str(), NULL, 10);
    }
}
#endif

bool CdromToc::loadCueSheet(const std::string &filename)
{
/*
    static const std::regex FILE_REGEX("^\\s*FILE\\s+\"(.*)\"\\s+(\\S+)\\s*$", std::regex_constants::icase);
    static const std::regex TRACK_REGEX("^\\s*TRACK\\s+([0-9]+)\\s+(\\S*)\\s*$", std::regex_constants::icase);
    static const std::regex PREGAP_REGEX("^\\s*PREGAP\\s+([0-9]+):([0-9]+):([0-9]+)\\s*$", std::regex_constants::icase);
    static const std::regex INDEX_REGEX("^\\s*INDEX\\s+([0-9]+)\\s+([0-9]+):([0-9]+):([0-9]+)\\s*$", std::regex_constants::icase);
    static const std::regex POSTGAP_REGEX("^\\s*POSTGAP\\s+([0-9]+):([0-9]+):([0-9]+)\\s*$", std::regex_constants::icase);
*/

    // Regexes modified to work around a bug where \s mysteriously fails to match under Linux, and only under Retroarch
    // (Works when compiled as a standalone program)
    // \s replaced with [ \\t] and \S with [^ \\t]
    static const std::regex FILE_REGEX("^[ \\t]*FILE[ \\t]+\"(.*)\"[ \\t]+([^ \\t]+)[ \\t]*$", std::regex_constants::icase);
    static const std::regex TRACK_REGEX("^[ \\t]*TRACK[ \\t]+([0-9]+)[ \\t]+([^ \\t]*)[ \\t]*$", std::regex_constants::icase);
    static const std::regex PREGAP_REGEX("^[ \\t]*PREGAP[ \\t]+([0-9]+):([0-9]+):([0-9]+)[ \\t]*$", std::regex_constants::icase);
    static const std::regex INDEX_REGEX("^[ \\t]*INDEX[ \\t]+([0-9]+)[ \\t]+([0-9]+):([0-9]+):([0-9]+)[ \\t]*$", std::regex_constants::icase);
    static const std::regex POSTGAP_REGEX("^[ \\t]*POSTGAP[ \\t]+([0-9]+):([0-9]+):([0-9]+)[ \\t]*$", std::regex_constants::icase);

    clear();

    File in;
    if (!in.open(filename))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Could not open CUE file: %s\n", filename.c_str());
        return false;
    }

    //***********************************
    // CUE parsing is done in three steps.
    //
    // For the first step:
    // - Check the CUE sheet syntax as thoroughly as possible
    // - Create the TOC structure, leaving some fields blanks for now
    // - Make a list of all source files, with their size. (Size of uncompressed audio for audio tracks)
    //***********************************

    std::string currentFile;
    int currentFileIndex = -1;
    TrackType currentFileAudioType = TrackType::AudioPCM;
    int currentTrack = -1;
    int currentIndex = -1;
    TrackType currentType = TrackType::Silence;
    bool trackHasPregap = false;
    bool trackHasPostgap = false;
    bool trackHasIndexOne = false;

    while(!in.eof())
    {
        std::string line = in.readLine();

        std::smatch match;

        if (std::regex_match(line, match, FILE_REGEX))
        {
            const std::string filespec = match[1].str();
            if (path_is_absolute(filespec))
                currentFile = filespec;
            else
                currentFile = path_replace_filename(filename.c_str(), filespec.c_str());
            
            currentTrack = -1;
            currentIndex = -1;
            currentType = TrackType::Silence;
            trackHasPregap = false;
            trackHasPostgap = false;
            trackHasIndexOne = false;

            std::string type = match[2].str();
            bool isBinary = string_compare_insensitive(type.c_str(), "BINARY");
            bool isWave = string_compare_insensitive(type.c_str(), "WAVE");

            if (!isBinary && !isWave)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "File type %s is not supported.\n", type.c_str());
                return false;
            }

            auto i = std::find_if(m_fileList.cbegin(), m_fileList.cend(), [&](const FileEntry& entry) { return entry.fileName == currentFile; });
            if (i == m_fileList.cend())
            {
                File file;
                if (!file.open(currentFile))
                {
                    Libretro::Log::message(RETRO_LOG_ERROR, "File %s could not be opened.\n", match[1].str().c_str());
                    return false;
                }

                int64_t fileSize;

                if (isBinary)
                {
                    fileSize = static_cast<int64_t>(file.size());
                    currentFileAudioType = TrackType::AudioPCM;
                }
                else
                {
                    if (!findAudioFileSize(currentFile, file, fileSize, currentFileAudioType))
                        return false;
                }

                m_fileList.push_back({currentFile, fileSize});

                currentFileIndex = static_cast<int>(m_fileList.size() - 1);
            }
            else
                currentFileIndex = static_cast<int>(std::distance(m_fileList.cbegin(), i));

            continue;
        }

        if (std::regex_match(line, match, TRACK_REGEX))
        {
            if (currentFileIndex < 0)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Track directive without file!\n");
                return false;
            }

            int newTrack = std::stol(match[1].str());

            if ((newTrack < 1) || (newTrack > 99))
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Track numbers should be between 1 and 99.\n");
                return false;
            }

            if ((currentTrack != -1) && (newTrack - currentTrack != 1))
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Track numbers should be contiguous and increasing.\n");
                return false;
            }

            if ((currentTrack != -1) && (!trackHasIndexOne))
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Track %02d has no index 01.\n", currentTrack);
                return false;
            }

            currentTrack = newTrack;
            currentIndex = -1;
            trackHasPregap = false;
            trackHasPostgap = false;
            trackHasIndexOne = false;

            std::string mode = match[2].str();

            if (string_compare_insensitive(mode.c_str(), "MODE1/2048"))
                currentType = TrackType::Mode1_2048;
            else if (string_compare_insensitive(mode.c_str(), "MODE1/2352"))
                currentType = TrackType::Mode1_2352;
            else if (string_compare_insensitive(mode.c_str(), "AUDIO"))
                currentType = currentFileAudioType;
            else
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Track mode %s is not supported.\n", match[2].str().c_str());
                return false;
            }

            if (((currentType == TrackType::Mode1_2048) || (currentType == TrackType::Mode1_2352)) && (currentFileAudioType != TrackType::AudioPCM))
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Data track defined when current source file is audio type.\n");
                return false;
            }

            continue;
        }

        if (std::regex_match(line, match, PREGAP_REGEX))
        {
            if (currentTrack < 0)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Pregap directive with no track defined.\n");
                return false;
            }

            if (trackHasPregap)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: A track can have only one pregap.\n");
                return false;
            }

            if (currentIndex >= 0)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Pregap directive must come before any indexes.\n");
                return false;
            }

            uint32_t m = std::stoul(match[1].str());
            uint32_t s = std::stoul(match[2].str());
            uint32_t f = std::stoul(match[3].str());
            uint32_t length = fromMSF(m, s, f);

            m_toc.push_back({ -1, { static_cast<uint8_t>(currentTrack), 0 }, TrackType::Silence, 0, 0, 0, length });

            trackHasPregap = true;

            continue;
        }

        if (std::regex_match(line, match, INDEX_REGEX))
        {
            if (currentTrack < 0)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Index directive with no track defined.\n");
                return false;
            }

            if (trackHasPostgap)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Index directive must come before postgap.\n");
                return false;
            }

            int newIndex = std::stol(match[1].str());

            if ((newIndex < 0) || (newIndex > 99))
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Index numbers should be between 0 and 99.\n");
                return false;
            }

            if (trackHasPregap && (newIndex == 0))
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Index 0 is not allowed with pregap.\n");
                return false;
            }

            if ((currentIndex != -1) && (newIndex - currentIndex != 1))
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Index numbers should be contiguous and increasing.\n");
                return false;
            }

            uint32_t m = std::stoul(match[2].str());
            uint32_t s = std::stoul(match[3].str());
            uint32_t f = std::stoul(match[4].str());
            uint32_t indexPosition = fromMSF(m, s, f);

            currentIndex = newIndex;

            if (currentIndex == 1)
                trackHasIndexOne = true;

            m_toc.push_back({ currentFileIndex, { static_cast<uint8_t>(currentTrack), static_cast<uint8_t>(currentIndex) }, currentType, indexPosition, 0, 0, 0 });

            continue;
        }

        if (std::regex_match(line, match, POSTGAP_REGEX))
        {
            if (currentTrack < 0)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Postgap directive with no track defined.\n");
                return false;
            }

            if (currentIndex < 0)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Postgap directive must come after all indexes.\n");
                return false;
            }

            if (trackHasPostgap)
            {
                Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: A track can have only one postgap.\n");
                return false;
            }

            ++currentIndex;
            uint32_t m = std::stoul(match[1].str());
            uint32_t s = std::stoul(match[2].str());
            uint32_t f = std::stoul(match[3].str());
            uint32_t length = fromMSF(m, s, f);

            m_toc.push_back({ -1, { static_cast<uint8_t>(currentTrack), static_cast<uint8_t>(currentIndex) }, TrackType::Silence, 0, 0, 0, length });

            trackHasPostgap = true;

            continue;
        }
    }

    // Final syntax checks

    if (currentTrack == -1)
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Should define at least one track.\n");
        return false;
    }

    if ((currentTrack != -1) && (!trackHasIndexOne))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CUE sheet: Track %02d  has no index 01.\n", currentTrack);
        return false;
    }

    //****************************
    // For step 2:
    //
    // - Calculate the size of every TOC entry
    // - Calculate the start offset of every TOC entry in the data files
    // ***************************

    // Build a list of pointers to every (track;index) couple
    std::vector<CdromToc::Entry*> sortedList;
    for(CdromToc::Entry& entry : m_toc)
        sortedList.push_back(&entry);

    // Sort the list by file index and "start sector" (offset in the file)
    std::sort(sortedList.begin(), sortedList.end(), [](const CdromToc::Entry* left, const CdromToc::Entry* right) -> bool
    {
        if (left->fileIndex == right->fileIndex)
            return left->indexPosition < right->indexPosition;

        return left->fileIndex < right->fileIndex;
    });

    auto i = sortedList.begin();
    while(i != sortedList.end())
    {
        int fileIndex = (*i)->fileIndex;

        // Count the number of TOC entries belonging to the same file
        ptrdiff_t count = std::count_if(i, sortedList.end(), [&](CdromToc::Entry* entry) -> bool
        {
            return entry->fileIndex == fileIndex;
        });

        // Index -1 is for silence entries with no data, skip it
        if (fileIndex == -1)
        {
            std::advance(i, count);
            continue;
        }

        size_t currentFileOffset = 0;

        auto end = std::next(i, count);
        while(i != end)
        {
            auto next = std::next(i, 1);

            uint32_t trackLength = 0;
            size_t sectorSize = 0;

            // Find the sector size from enty type
            if ((*i)->trackType == TrackType::Mode1_2048)
                sectorSize = 2048;
            else
                sectorSize = 2352;

            if (next == end)
            {
                // Last TOC entry of the file, calculate from the file size instead
                trackLength = static_cast<uint32_t>((static_cast<size_t>(m_fileList.at(static_cast<size_t>(fileIndex)).fileSize) - currentFileOffset) / sectorSize);
            }
            else
            {
                // Length is start of next TOC entry - start of this entry
                trackLength = (*next)->indexPosition - (*i)->indexPosition;
            }

            // Update the entry with the file offset and length
            (*i)->fileOffset = currentFileOffset;
            (*i)->trackLength = trackLength;

            // Update the file offset
            currentFileOffset += (trackLength * sectorSize);
            ++i;
        }
    }

    //***********************
    // For step three:
    //
    // - Calculate the position of everything on our virtual disc
    //***********************

    uint32_t currentSector = 0;

    for(CdromToc::Entry& entry : m_toc)
    {
        entry.startSector = currentSector;
        currentSector += entry.trackLength;
    }

    m_totalSectors = currentSector;
    m_firstTrack = m_toc.front().trackIndex.track();
    m_lastTrack = m_toc.back().trackIndex.track();

    return true;
}

bool  CdromToc::loadChd(const std::string& filename)
{
    static const std::regex CHTR_REGEX(".*TRACK:([0-9]+) TYPE:(.*) SUBTYPE:(.*) FRAMES:([0-9]+).*", std::regex_constants::icase);
    static const std::regex CHT2_REGEX(".*TRACK:([0-9]+) TYPE:(.*) SUBTYPE:(.*) FRAMES:([0-9]+) PREGAP:([0-9]+) PGTYPE:(.*) PGSUB:(.*) POSTGAP:([0-9]+).*", std::regex_constants::icase);
    //                                             1           2            3             4               5             6           7             8

    // Clear everything
    clear();

    // Open the CHD file
    ChdFile chd;
    if (!chd.open(filename))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Could not open CHD file: %s\n", filename.c_str());
        return false;
    }

    // Add the CHD to the file list
    m_fileList.push_back({ filename, static_cast<int64_t>(chd.size()) });

    // Position inside the CHD (sectors).
    // Each track begins on a sector number that is a multiple of 4
    uint32_t chdPosition = 0;

    // Current position on the CD
    uint32_t cdPosition = 0;

    // Used to fix some weird CHD behavior
    bool previousWasData = true;

    // Scan metadata
    for(uint32_t idx = 0; idx < 99; ++idx)
    {
        // Current track type
        TrackType trackType = TrackType::Silence;

        // Current track number
        uint32_t trackNumber;

        // Length of the pregap (index 0)
        uint32_t pregapLength;

        // Length of the track (index 1)
        uint32_t trackLength;

        // Length of the postgap (index 2)
        uint32_t postgapLength;

        // True if the metadata is in V2 format
        bool v2Metadata = true;

        // First, try to read V2 metadata
        std::string metadata = chd.metadata(CDROM_TRACK_METADATA2_TAG, idx);
        if (metadata.empty())
        {
            // If that failed, try the old format
            metadata = chd.metadata(CDROM_TRACK_METADATA_TAG, idx);
            v2Metadata = false;
        }

        // If no metadata was found for this index, continue
        if (metadata.empty())
            continue;

        // Match the obtained metadata using the appropriate regex
        std::smatch match;
        bool regexMatched = false;

        if (v2Metadata)
            regexMatched = std::regex_match(metadata, match, CHT2_REGEX);
        else
            regexMatched = std::regex_match(metadata, match, CHTR_REGEX);

        if (!regexMatched)
        {
            Libretro::Log::message(RETRO_LOG_ERROR, "Regex did not match line: %s\n", metadata.c_str());
            return false;
        }

        if (v2Metadata)
        {
            // Get pre / post gap length from metadata
            pregapLength = std::stoul(match[5].str());
            postgapLength = std::stoul(match[8].str());
        }
        else
        {
            // Old metadata don't have pre / post gap info so set it to zero
            pregapLength = 0;
            postgapLength = 0;
        }

        //Get the track number and length from metadata
        trackNumber = std::stoul(match[1].str());
        trackLength = std::stoul(match[4].str());

        // Find the track type
        const std::string trackTypeStr = match[2];

        if (string_compare_insensitive(trackTypeStr.c_str(), "MODE1"))
            trackType = TrackType::Mode1_2048;
        else if (string_compare_insensitive(trackTypeStr.c_str(), "MODE1/2048"))
            trackType = TrackType::Mode1_2048;
        else if (string_compare_insensitive(trackTypeStr.c_str(), "MODE1_RAW"))
            trackType = TrackType::Mode1_2352;
        else if (string_compare_insensitive(trackTypeStr.c_str(), "MODE1/2352"))
            trackType = TrackType::Mode1_2352;
        else if (string_compare_insensitive(trackTypeStr.c_str(), "AUDIO"))
            trackType = TrackType::AudioPCM;
        else
        {
            Libretro::Log::message(RETRO_LOG_ERROR, "Track type %s is not supported.\n", trackTypeStr.c_str());
            return false;
        }

        const std::string pgTypeStr = match[6];
        const bool isVAudio = string_compare_insensitive(pgTypeStr.c_str(), "VAUDIO");

        // Make the CHD position a multiple of 4
        if (chdPosition % 4)
            chdPosition += 4 - (chdPosition % 4);

        // Create the pregap entry (index 0)
        if (pregapLength)
        {
            m_toc.push_back({ -1, { static_cast<uint8_t>(trackNumber), 0 }, TrackType::Silence, 0, cdPosition, 0, pregapLength });

            // CHD WEIRDNESS: If the previous track was data don't move the chd position
            // But do it if PGTYPE indicates 'VAUDIO' (newer CHDs)
            if (!previousWasData || isVAudio)
            {
                chdPosition += pregapLength;

                // Shorten the track accordingly
                trackLength -= pregapLength;
            }

            cdPosition += pregapLength;
        }

        // Create the track entry (index 1)
        m_toc.push_back({ 0, { static_cast<uint8_t>(trackNumber), 1 }, trackType, 0, cdPosition, static_cast<size_t>(chdPosition) * 2352, trackLength });
        chdPosition += trackLength;
        cdPosition += trackLength;

        // Create the post gap entry (index 2)
        if (postgapLength)
        {
            m_toc.push_back({ -1, { static_cast<uint8_t>(trackNumber), 2 }, TrackType::Silence, 0, cdPosition, 0, postgapLength });
            cdPosition += postgapLength;
        }

        previousWasData = !(trackType == TrackType::AudioPCM);
    }

    if (m_toc.empty())
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Invalid CHD: TOC is empty!\n");
        return false;
    }

    m_totalSectors = cdPosition;
    m_firstTrack = m_toc.front().trackIndex.track();
    m_lastTrack = m_toc.back().trackIndex.track();

    return true;
}

const CdromToc::Entry *CdromToc::findTocEntry(const TrackIndex &trackIndex) const
{
    auto i = std::lower_bound(m_toc.cbegin(), m_toc.cend(), trackIndex, [](const CdromToc::Entry& entry, const TrackIndex& trackIndex) -> bool
    {
        return entry.trackIndex < trackIndex;
    });

    if ((i == m_toc.cend()) || (i->trackIndex != trackIndex))
        return nullptr;

    return &(*i);
}

const CdromToc::Entry* CdromToc::findTocEntry(uint32_t sector) const
{
    auto i = std::upper_bound(m_toc.cbegin(), m_toc.cend(), sector, [](uint32_t sector, const CdromToc::Entry& entry) -> bool
    {
        return sector < entry.startSector;
    });

    if (i != m_toc.cbegin())
        --i;

    return &(*i);
}

bool CdromToc::findAudioFileSize(const std::string& path, File &file, int64_t &fileSize, TrackType &trackType)
{
    std::string filename = path_get_filename(path.c_str());
    std::string extension = path_get_extension(path.c_str());

    if (string_compare_insensitive(extension.c_str(), "WAV"))
    {
        WavFile wavFile;

        if (!wavFile.initialize(&file))
        {
            Libretro::Log::message(RETRO_LOG_ERROR, "File %s%s is not a valid WAV file.", filename.c_str(), extension.c_str());
            return false;
        }

        fileSize = wavFile.length();
        trackType = TrackType::AudioWav;

        wavFile.cleanup();

        return true;
    }
    else if (string_compare_insensitive(extension.c_str(), "FLAC"))
    {
        FlacFile flacFile;

        if (!flacFile.initialize(&file))
        {
            Libretro::Log::message(RETRO_LOG_ERROR, "File %s%s is not a valid FLAC file.", filename.c_str(), extension.c_str());
            return false;
        }

        fileSize = static_cast<int64_t>(flacFile.length());
        trackType = TrackType::AudioFlac;

        flacFile.cleanup();

        return true;
    }
    else if (string_compare_insensitive(extension.c_str(), "OGG"))
    {
        OggFile oggFile;

        if (!oggFile.initialize(&file))
        {
            Libretro::Log::message(RETRO_LOG_ERROR, "File %s%s is not a valid OGG file.", filename.c_str(), extension.c_str());
            return false;
        }

        fileSize = static_cast<int64_t>(oggFile.length());
        trackType = TrackType::AudioOgg;

        oggFile.cleanup();

        return true;
    }

    Libretro::Log::message(RETRO_LOG_ERROR, "File type %s is not supported.", extension.c_str());
    return false;
}
