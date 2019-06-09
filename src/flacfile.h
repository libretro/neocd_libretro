#ifndef FLACFILE_H
#define FLACFILE_H

#include <cstdint>
#include <vector>

#include "abstractfile.h"
#include "FLAC/stream_decoder.h"

class FlacFile
{
public:
    FlacFile();
    ~FlacFile();

    // Non copyable
    FlacFile(const FlacFile&) = delete;
    
    // Non copyable
    FlacFile& operator=(const FlacFile&) = delete;

    bool initialize(AbstractFile* file);

    size_t bytesAvailable() const;

    size_t read(char *data, size_t size);

    bool seek(size_t position);

    size_t length();

    void cleanup();

    // Declare all callbacks as friends so they can modify the internals
    friend FLAC__StreamDecoderReadStatus flac_read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
    friend FLAC__StreamDecoderSeekStatus flac_seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
    friend FLAC__StreamDecoderTellStatus flac_tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
    friend FLAC__StreamDecoderLengthStatus flac_length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
    friend FLAC__bool flac_eof_cb(const FLAC__StreamDecoder *decoder, void *client_data);
    friend FLAC__StreamDecoderWriteStatus flac_write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data);
    friend void flac_error_cb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

protected:
    FLAC__StreamDecoder* m_decoder;
    AbstractFile* m_file;
    std::vector<char> m_buffer;
    size_t m_readPosition;
};

#endif // FLACFILE_H
