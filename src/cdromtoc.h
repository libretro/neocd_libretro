#ifndef CDROMTOC_H
#define CDROMTOC_H

#include <cstdint>
#include <string>
#include <vector>

#include "file.h"
#include "trackindex.h"

class CdromToc
{
public:
    /// Enum representing all track types supported
    enum class TrackType
    {
        Mode1_2352,  /// Raw track (2352 bytes per sector)
        Mode1_2048,  /// ISO track (2048 bytes per sector)
        Silence,     /// Audio silence (no associated data)
        AudioPCM,    /// PCM audio (raw track)
        AudioFlac,   /// FLAC audio
        AudioOgg,    /// Ogg audio
        AudioWav     /// WAV audio
    };

    struct Entry
    {
        /// Index of file holding the track data in the file list
        int fileIndex;

        /// Track and index numbers for the entry
        TrackIndex trackIndex;

        /// Track type (MODE1/2048, MODE1/2352, AUDIO, etc)
        CdromToc::TrackType trackType;

        /// Index position in data file (sector)
        uint32_t indexPosition;

        /// Starting sector
        uint32_t startSector;

        /// Offset to the data of the track in the file (in bytes)
        size_t fileOffset;

        /// Track length (in sectors)
        uint32_t trackLength;
    };

    struct FileEntry
    {
        /// Path to the data file
        std::string fileName;

        /// Size of the file, in bytes. For audio files this the size of uncompressed audio.
        int64_t fileSize;
    };

    explicit CdromToc();

    /*!
        Erase all TOC information
     */
    void clear();

    /*!
        Loads a cue sheet file. (.CUE)
     */
    bool loadCueSheet(const std::string& filename);

        /*!
        Loads a MAME chd file. (.CHD)
     */
    bool loadChd(const std::string& filename);

    /*!
        Returns a non modifiable reference to the TOC entries.
     */
    inline const std::vector<CdromToc::Entry>& toc() const
    {
        return m_toc;
    }

    /*!
        Returns a non modifiable reference to the file list.
     */
    inline const std::vector<CdromToc::FileEntry>& fileList() const
    {
        return m_fileList;
    }

    /*!
        Returns the number of the first track
     */
    inline uint8_t firstTrack() const
    {
        return m_firstTrack;
    }

    /*!
        Return the number of the last track
     */
    inline uint8_t lastTrack() const
    {
        return m_lastTrack;
    }

    /*!
        Returns the total number of sectors
     */
    inline uint32_t totalSectors() const
    {
        return m_totalSectors;
    }

    /*!
        Finds the TOC entry corresponding to a given (track;index)
     */
    const CdromToc::Entry* findTocEntry(const TrackIndex& trackIndex) const;

    /*!
        Finds the TOC entry corresponding to a given sector
     */
    const CdromToc::Entry* findTocEntry(uint32_t sector) const;

    /*!
        Converts a logical block address to sector number.
     */
    static inline constexpr uint32_t fromLBA(uint32_t lba)
    {
        return (lba + 150);
    }

    /*!
        Converts a sector number to logical block address.
     */
    static inline constexpr uint32_t toLBA(uint32_t position)
    {
        return (position - 150);
    }

    /*!
        Convert a sector number to minutes, seconds, frames.
     */
    static inline void toMSF(uint32_t position, uint32_t& m, uint32_t& s, uint32_t& f)
    {
        m = position / 4500;
        s = (position / 75) % 60;
        f = position % 75;
    }

    /*!
        Converts a position in minutes, seconds, frames to a sector number
     */
    static inline constexpr uint32_t fromMSF(uint32_t m, uint32_t s, uint32_t f)
    {
        return (m * 4500) + (s * 75) + f;
    }

    inline bool isEmpty() const
    {
        return (m_toc.size() == 0);
    }

protected:
    /*!
        Finds the size of uncompressed audio data.
     */
    bool findAudioFileSize(const std::string& path, File& file, int64_t& fileSize, CdromToc::TrackType& trackType);

    /// TOC entries
    std::vector<CdromToc::Entry> m_toc;

    /// List of files referenced in the TOC
    std::vector<CdromToc::FileEntry> m_fileList;

    /// Number of the first track
    uint8_t m_firstTrack;

    /// Number of the last track
    uint8_t m_lastTrack;

    /// Total number of sectors
    uint32_t m_totalSectors;
};

#endif // CDROMTOC_H
