#include <algorithm>
#include <cstring>

#include "flacfile.h"

#ifndef UNUSED_ARG
#define UNUSED_ARG(x) (void)x
#endif

// FLAC stream read callback
FLAC__StreamDecoderReadStatus flac_read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    UNUSED_ARG(decoder);

    FlacFile* flacFile = reinterpret_cast<FlacFile*>(client_data);
    AbstractFile *file = flacFile->m_file;

    if (!file || !file->isOpen())
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    *bytes = file->readData(&buffer[0], *bytes);

    if (*bytes == 0)
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

// FLAC stream seek callback
FLAC__StreamDecoderSeekStatus flac_seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
    UNUSED_ARG(decoder);

    FlacFile* flacFile = reinterpret_cast<FlacFile*>(client_data);
    AbstractFile *file = flacFile->m_file;

    if (!file || !file->isOpen())
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

    if (!file->seek(absolute_byte_offset))
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

// FLAC stream tell callback
FLAC__StreamDecoderTellStatus flac_tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
    UNUSED_ARG(decoder);

    FlacFile* flacFile = reinterpret_cast<FlacFile*>(client_data);
    AbstractFile *file = flacFile->m_file;

    if (!file || !file->isOpen())
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

    *absolute_byte_offset = static_cast<FLAC__uint64>(file->pos());

    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

// FLAC stream length callback
FLAC__StreamDecoderLengthStatus flac_length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
    UNUSED_ARG(decoder);

    FlacFile* flacFile = reinterpret_cast<FlacFile*>(client_data);
    AbstractFile *file = flacFile->m_file;

    if (!file || !file->isOpen())
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;

    *stream_length = static_cast<FLAC__uint64>(file->size());

    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

// FLAC stream end of file callback
FLAC__bool flac_eof_cb(const FLAC__StreamDecoder *decoder, void *client_data)
{
    UNUSED_ARG(decoder);

    FlacFile* flacFile = reinterpret_cast<FlacFile*>(client_data);
    AbstractFile *file = flacFile->m_file;

    if (!file || !file->isOpen())
        return false;

    return file->eof();
}

// FLAC stream write callback
FLAC__StreamDecoderWriteStatus flac_write_cb(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
    UNUSED_ARG(decoder);

    FlacFile* flacFile = reinterpret_cast<FlacFile*>(client_data);

    // This should not be called if there bytes still left in the buffer!
    if (flacFile->bytesAvailable())
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    // Get the audio characteristics from the frame header
    uint32_t samples = frame->header.blocksize;
    uint32_t channels = frame->header.channels;
    uint32_t bitsPerSample = frame->header.bits_per_sample;

    // Make sure we have enough room for the samples and reset the read pointer
    // FLAC seems to pass a constant sample size so it should not trigger too many memory allocations
    flacFile->m_buffer.resize(samples * 4);
    flacFile->m_readPosition = 0;

    // Set up the pointers for the conversion, in case of a monaural FLAC we duplicate the channel
    int16_t* out = reinterpret_cast<int16_t*>(&flacFile->m_buffer[0]);
    const FLAC__int32* left = buffer[0];
    const FLAC__int32* right = buffer[channels == 1 ? 0 : 1];

    // Copy the samples in the buffer
    if (bitsPerSample == 16)
    {
        while(samples >= 8)
        {
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            samples -= 8;
        }

        while(samples)
        {
            *out++ = static_cast<int16_t>(*left++);
            *out++ = static_cast<int16_t>(*right++);
            --samples;
        }
    }
    else if (bitsPerSample == 8)
    {
        while(samples)
        {
            *out++ = static_cast<int16_t>((*left++) << 8);
            *out++ = static_cast<int16_t>((*right++) << 8);
            --samples;
        }
    }
    else if (bitsPerSample == 24)
    {
        while(samples)
        {
            *out++ = static_cast<int16_t>((*left++) >> 8);
            *out++ = static_cast<int16_t>((*right++) >> 8);
            --samples;
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

// FLAC stream error callback
void flac_error_cb(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    UNUSED_ARG(decoder);
    UNUSED_ARG(status);
    UNUSED_ARG(client_data);
}

FlacFile::FlacFile() :
    m_decoder(nullptr),
    m_file(nullptr),
    m_buffer(),
    m_readPosition(0)
{
}

FlacFile::~FlacFile()
{
    if (m_decoder)
        FLAC__stream_decoder_delete(m_decoder);
}

bool FlacFile::initialize(AbstractFile* file)
{
    // Set the new pointer to the file stream
    m_file = file;

    // Empty the read buffer
    m_readPosition = m_buffer.size();

    // Create and initialize the decoder, if needed
    if (!m_decoder)
    {
        m_decoder = FLAC__stream_decoder_new();
        if (!m_decoder)
            return false;

        FLAC__StreamDecoderInitStatus result = FLAC__stream_decoder_init_stream(
                    m_decoder,
                    flac_read_cb,
                    flac_seek_cb,
                    flac_tell_cb,
                    flac_length_cb,
                    flac_eof_cb,
                    flac_write_cb,
                    nullptr,
                    flac_error_cb,
                    this);
        if (result != FLAC__STREAM_DECODER_INIT_STATUS_OK)
            return false;

        return true;
    }

    // If the decoder already exists, just reset it
    return (FLAC__stream_decoder_reset(m_decoder) != 0);
}

size_t FlacFile::bytesAvailable() const
{
    return m_buffer.size() - m_readPosition;
}

size_t FlacFile::read(char *data, size_t size)
{
    size_t done = 0;

    if (!m_decoder)
        return done;

    // Loop until the requested size has been read
    while(size)
    {
        // Loop processing single frames until some data is available
        while(!bytesAvailable())
        {
            if (!FLAC__stream_decoder_process_single(m_decoder))
                return done;

            FLAC__StreamDecoderState status = FLAC__stream_decoder_get_state(m_decoder);
            if ((status == FLAC__STREAM_DECODER_END_OF_STREAM) || (status == FLAC__STREAM_DECODER_ABORTED))
                return done;
        }

        // Determine the slice of data to copy
        size_t slice = std::min(size, bytesAvailable());

        // Copy the data
        std::memcpy(data, &m_buffer[m_readPosition], slice);

        // Advance the pointers
        m_readPosition += slice;
        done += slice;
        data += slice;
        size -= slice;
    }

    return done;
}

bool FlacFile::seek(size_t position)
{
    if (!m_decoder)
        return false;

    // Empty the read buffer
    m_readPosition = m_buffer.size();

    // Position is in bytes and FLAC wants samples so we divide by 4 (2 channels, 16 bits)
    return (FLAC__stream_decoder_seek_absolute(m_decoder, static_cast<FLAC__uint64>(position / 4)) != 0);
}

size_t FlacFile::length()
{
    if (!m_decoder)
        return false;

    size_t size = static_cast<size_t>(FLAC__stream_decoder_get_total_samples(m_decoder));
    int i = 1000;

    // If the info is not available, decode stuff until it is
    while((size == 0) && (i > 0))
    {
        FLAC__stream_decoder_process_single(m_decoder);
        size = static_cast<size_t>(FLAC__stream_decoder_get_total_samples(m_decoder));
        --i;
    }

    return size * 4;
}

void FlacFile::cleanup()
{
}
