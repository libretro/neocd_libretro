#ifndef CDROM_H
#define CDROM_H

#include "trackindex.h"
#include "flacfile.h"
#include "oggfile.h"
#include "wavfile.h"
#include "circularbuffer.h"
#include "datapacker.h"

#include <fstream>
#include <map>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

/**
 * @class Cdrom
 * @brief The Cdrom class holds the CD image table of contents and is also responsible for reading and decoding audio
 * data in a separate thread
 */
class Cdrom
{
public:
    /// Enum representing all track types supported by the emulator
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

    /**
     * @class TocEntry
     * @brief Structure holding information about a TOC entry
     */
    class TocEntry
    {
    public:
        TocEntry(const std::string& f, const TrackIndex& ti, TrackType tt, uint32_t p, uint32_t o, uint32_t l) :
            file(f),
            trackIndex(ti),
            trackType(tt),
            lba(p),
            offset(o),
            entryLength(l)
        {}

        /// File associated to this TOC entry
        std::string file;
        
        /// Track index
        TrackIndex trackIndex;
        
        /**
         * Track type
         * @sa Cdrom::TrackType
         */
        TrackType trackType;
        
        /// Starting sector
        uint32_t lba;
        
        /// Start offset of the track in the file (in bytes)
        uint32_t offset;
        
        /// Track length (in sectors)
        uint32_t entryLength;
    };

    Cdrom();
    ~Cdrom();
    
    // Non copyable
    Cdrom(const Cdrom&) = delete;
    
    // Non copyable
    Cdrom& operator=(const Cdrom&) = delete;

    /**
     * @brief Create the worker thread to decode audio
     */
    void createWorkerThread();
    
    /**
     * @brief Ask the audio decoder thread to exit and wait for it to finish
     */
    void endWorkerThread();

    /**
     * @brief Initialize all members to a known state
     */
    void initialize();
    
    /**
     * @brief Reset the emulated CD-ROM
     */
    void reset();

    /**
     * @brief Determine the length of a file, in bytes and in sectors. 
     * @note If the file is compressed audio, the values are calculated from the uncompressed size.
     * @param[in] filename Name of the file for which we need the length
     * @param[out] length Will contain the length in bytes
     * @param[out] sectorSize Will contain the length in sectors
     */
    bool findFileLength(const std::string& filename, size_t& length, uint32_t& sectorSize);

    /**
     * @brief Load a cue sheet file
     * @param cueFile File to load the CD-ROM TOC from
     * @return true if loading succeeded
     */
    bool loadCue(const std::string& cueFile);

    /**
     * @brief Get the CD-ROM table of contents
     * @return Map of TocEntry sorted by TrackIndex
     */
    const std::map<TrackIndex, const TocEntry*>& toc() const;

    /**
     * @brief Get a pointer to the TocEntry of the current track
     */
    const TocEntry* currentTrack() const;
    
    /**
     * @brief Get the TrackIndex of the current track
     */
    TrackIndex currentTrackIndex() const;
    
    /**
     * @brief Get the start sector of the current track
     */
    uint32_t currentTrackPosition() const;
    
    /**
     * @brief Get the size in sectors of the current index
     */
    uint32_t currentIndexSize() const;

    /**
     * @brief Get the track number of the first track
     */
    uint8_t firstTrack() const;
    
    /**
     * @brief Get the track number of the last track
     */
    uint8_t lastTrack() const;

    /**
     * @brief Get the start sector of track 'track'.
     * @param track Track number.
     * @return The start sector for the track, or 0.
     */
    uint32_t trackPosition(uint8_t track) const;
    
    /**
     * @brief Check if the track 'track' is a data track.
     * @param track Track number.
     * @return True if the track is of data type.
     */
    bool trackIsData(uint8_t track) const;

    /**
     * @brief Return the position of the CD leadout.
     */
    uint32_t leadout() const;

    /**
     * @brief Return the current play position.
     */
    uint32_t position() const;
    
    /**
     * @brief Move the play position to the next sector (if playing).
     */
    void increasePosition();
    
    /**
     * @brief Change the current play position. This will handle track change and discard buffered audio.
     * @param position New play position.
     */
    void seek(uint32_t position);

    /**
     * @brief Start playing at the current position
     */
    void play();
    
    /**
     * @brief Returns true if the CD-ROM is playing
     */
    bool isPlaying() const;
    
    /**
     * @brief Stop playing
     */
    void stop();

    /**
     * @brief Read a data sector from CD-ROM. (2048 bytes)
     * @note Returns an empty section if the current position does not belong to a data track.
     * @param buffer The buffer to write to.
     */
    void readData(char *buffer);
    
    /**
     * @brief Read audio data decoded by the audio decoder thread, blocking if needed.
     * @note When the CD-ROM is not playing the worker thread keeps filling the buffer with silence.
     * @param buffer The buffer to write to.
     * @param size Size of the data to get.
     */
    void readAudio(char *buffer, size_t size);
    
    /**
     * @brief Read audio data from the image file and decode it. Should only be called from the decoder thread!
     * @param buffer The buffer to write to.
     * @param size The size to srite.
     */
    void readAudioDirect(char *buffer, size_t size);

    /**
     * @brief Return true if the current position belongs to a data track.
     */
    bool isData() const;
    
    /**
     * @brief Return true if the current position belongs to an audio track.
     */
    bool isAudio() const;
    
    /**
     * @brief Return true if the current position is pregap (silence)
     */
    bool isPregap() const;

    /**
     * @brief Returns true if the TOC is empty (invalid).
     */
    bool isTocEmpty() const;

    /**
     * @brief Decode a BCD value.
     * @param value Value to decode.
     */
    static inline constexpr uint8_t fromBCD(uint8_t value)
    {
        return (((value >> 4) * 10) + (value & 0x0F));
    }

    /**
     * @brief Encode a value to BCD.
     * @param value Value to encode.
     */
    static inline constexpr uint8_t toBCD(uint8_t value)
    {
        return (((value / 10) << 4) | (value % 10));
    }

    /**
     * @brief Convert a logical block address to sector number.
     * @param lba Logical Block Address.
     */
    static inline constexpr uint32_t fromLBA(uint32_t lba)
    {
        return (lba + 150);
    }

    /**
     * @brief Convert a sector number to logical block address.
     * @param position Sector number.
     */
    static inline constexpr uint32_t toLBA(uint32_t position)
    {
        return (position - 150);
    }

    /**
     * @brief Convert a sector number to minutes, seconds, frames.
     * @param[in] position The position to convert.
     * @param[out] m Minutes.
     * @param[out] s Seconds.
     * @param[out] f Frames.
     */
    static inline void toMSF(uint32_t position, uint8_t& m, uint8_t& s, uint8_t& f)
    {
        m = position / 4500;
        s = (position / 75) % 60;
        f = position % 75;
    }

    /**
     * @brief Convert a position in minutes, seconds, frames to a sector number
     * @param m Minutes.
     * @param s Seconds.
     * @param f Frames.
     * @return Sector Number.
     */
    static inline constexpr uint32_t fromMSF(uint8_t m, uint8_t s, uint8_t f)
    {
        return (m * 4500) + (s * 75) + f;
    }

protected:

    /**
     * @brief Close the image / audio file if needed.
     */
    void cleanup();
    
    /**
     * @brief Returns true if the file corresponding to the TocEntry is different from the one currently open.
     * @param current The TocEntry 
     */
    bool hasFileChanged(const TocEntry* current) const;
    
    
    /**
     * @brief Open the image / audio file corresponding to the current track, and immediately seek audio if 'doInitialSeek' is true.
     * @param doInitialSeek If true, see audio immediately.
     */
    void handleTrackChange(bool doInitialSeek);
    
    /**
     * @brief Seek in the audio file to the position corresponding to m_currentPosition.
     */
    void seekAudio();
    
    /**
     * @brief Audio decoding function. Runs in a separate thread.
     */
    void audioBufferWorker();

    // **** START Variables to save in savestate
    
    uint32_t    m_currentPosition;  /// The sector being played / decoded
    bool        m_isPlaying;        /// True if the CD-ROM is playing
    
    // **** END Variables to save in savestate

    const TocEntry*         m_currentTrack;             /// TocEntry pointer to the current track

    CircularBuffer<char>    m_circularBuffer;           /// Circular buffer to store decoded audio
    bool                    m_audioWorkerThreadCreated; /// True is the audio decoder thread has been created
    bool                    m_exitFlag;                 /// Set to true to have the audio thread stop
    std::thread             m_workerThread;             /// Audio decoder worker thread
    std::mutex              m_workerMutex;              /// Mutex to access the circular buffer
    std::condition_variable m_workerConditionVariable;  /// Condition variable to notify when more data is available

    std::ifstream           m_file;                     /// The currently opened image cd image file
    FlacFile                m_flacFile;                 /// FLAC file decoder
    OggFile                 m_oggFile;                  /// OGG file decoder
    WavFile                 m_wavFile;                  /// WAV file decoder

    uint32_t                                m_leadout;      /// Sector number of the lead out area (end of the CD)
    std::vector<TocEntry>                   m_toc;          /// CD-ROM table of contents
    std::map<TrackIndex, const TocEntry*>   m_tocByTrack;   /// Pointers to the TOC entries, sorted by track index
    std::map<uint32_t, const TocEntry*>     m_tocByLBA;     /// Pointers to the TOC entries, sorted by logical block address

    friend DataPacker& operator<<(DataPacker& out, const Cdrom& cdrom);
    friend DataPacker& operator>>(DataPacker& in, Cdrom& cdrom);
};

/**
 * @brief Save CD-ROM state to a DataPacker
 * @param out DataPacker to save to
 * @param cdrom The cdrom state
 */
DataPacker& operator<<(DataPacker& out, const Cdrom& cdrom);

/**
 * @brief Restore CD-ROM state from a DataPacker
 * @param in DataPacker to read state from
 * @param cdrom The cdrom state
 */
DataPacker& operator>>(DataPacker& in, Cdrom& cdrom);

#endif // CDROM_H
