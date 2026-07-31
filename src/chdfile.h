#ifndef CHDFILE_H
#define CHDFILE_H 1

#include <cstdint>
#include <vector>

#include "abstractfile.h"

#include <formats/rchd.h>
#include <streams/file_stream.h>

class ChdFile : public AbstractFile
{
public:
    explicit ChdFile();
    virtual ~ChdFile() override;
    
    bool open(const std::string& filename) override;

    bool isOpen() const override;

    bool isChd() const override;

    void close() override;

    size_t size() const override;

    int64_t pos() const override;

    bool seek(size_t pos) override;

    bool skip(size_t off) override;

    bool eof() const override;

    size_t readData(void* data, size_t size) override;

    size_t readAudio(void* data, size_t size) override;
    
    std::string readLine() override;

    std::string metadata(uint32_t searchTag, uint32_t searchIndex);

protected:
    size_t read(void* data, size_t size, bool dataMode);

    bool fetchHunk(uint32_t number, bool dataMode);

    // rchd performs no file I/O of its own: it names a byte range and
    // this supplies it. Returns false if the range could not be read.
    bool service(const rchd_request_t& request);

    // Drives an rchd operation to completion, servicing every request
    // it raises. step is either rchd_open_step or rchd_read_step.
    bool pump(int (*step)(rchd_t*, rchd_request_t*));

    static void swab(void* data, size_t size);

    rchd_t* m_chd;
    RFILE* m_stream;
    std::vector<uint8_t> m_io;
    uint32_t m_hunkSize;
    uint32_t m_hunkLogicalSize;
    size_t m_totalSize;
    size_t m_readPointer;
    bool m_isDataHunk;
    int32_t m_hunkNumber;
    char* m_hunkData;
};

#endif
