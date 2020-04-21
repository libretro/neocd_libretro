#ifndef CDROM_H
#define CDROM_H

#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "cdromtoc.h"
#include "chdfile.h"
#include "circularbuffer.h"
#include "datapacker.h"
#include "file.h"
#include "flacfile.h"
#include "oggfile.h"
#include "trackindex.h"
#include "wavfile.h"

/**
 * @class Cdrom
 * @brief The Cdrom class holds the CD image table of contents and is also responsible for reading and decoding audio
 * data in a separate thread
 */
class Cdrom
{
public:
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
     * @brief Audio worker thread step: fill some data in circular buffer
     */
    void fillCircularBuffer();

    /**
     * @brief Initialize all members to a known state
     */
    void initialize();
    
    /**
     * @brief Reset the emulated CD-ROM
     */
    void reset();

    /**
     * @brief Load a CD image
     * @param imageFile CD-ROM image file
     * @return true if loading succeeded
     */
    bool loadCd(const std::string& imageFile);

    /**
     * @brief Get a pointer to the TocEntry of the current track
     */
    const CdromToc::Entry* currentTrack() const;
    
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
        return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
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
    bool hasFileChanged(const CdromToc::Entry* current) const;
    
    
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

    /**
     * @brief Returns true if the filename is a CHD file
     * @param filename
     * @return True if the filename is a CHD file
     */
    static bool filenameIsChd(const std::string& path);

    // **** START Variables to save in savestate
    
    /// The sector being played / decoded
    uint32_t m_currentPosition;

    /// True if the CD-ROM is playing
    bool m_isPlaying;
    
    // **** END Variables to save in savestate

    /// TocEntry pointer to the current track
    const CdromToc::Entry* m_currentTrack;

    /// Circular buffer to store decoded audio
    CircularBuffer<char> m_circularBuffer;

    /// True is the audio decoder thread has been created
    bool m_audioWorkerThreadCreated;

    /// Set to true to have the audio thread stop
    bool m_exitFlag;

#ifndef SYNC_CDROM
    /// Audio decoder worker thread
    std::thread m_workerThread;

    /// Mutex to access the circular buffer
    std::mutex m_workerMutex;

    /// Condition variable to notify when more data is available
    std::condition_variable m_workerConditionVariable;
#endif

    /// The currently opened image cd image file
    AbstractFile* m_file;

    File m_imageFile;

    ChdFile m_chdFile;

    /// FLAC file decoder
    FlacFile m_flacFile;

    /// OGG file decoder
    OggFile m_oggFile;

    /// WAV file decoder
    WavFile m_wavFile;

    /// CD-ROM table of contents
    CdromToc m_toc;

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
