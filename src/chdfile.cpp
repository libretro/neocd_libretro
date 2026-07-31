#include <algorithm>
#include <cstring>

#include "chdfile.h"
#include "neocd_endian.h"

constexpr int CHD_SECTOR_SIZE = 2352 + 96;
constexpr int CD_SECTOR_SIZE = 2352;

ChdFile::ChdFile() : 
    AbstractFile(),
    m_chd(nullptr),
    m_stream(nullptr),
    m_io(),
    m_hunkSize(0),
    m_hunkLogicalSize(0),
    m_totalSize(0),
    m_readPointer(0),
    m_isDataHunk(true),
    m_hunkNumber(-1),
    m_hunkData(nullptr)
{ }

ChdFile::~ChdFile()
{
    close();
}

bool ChdFile::service(const rchd_request_t& request)
{
    // A differencing image would ask for its parent; the core has no way
    // to resolve one, so refuse rather than feed the wrong bytes.
    if (request.source != RCHD_SOURCE_SELF)
        return false;

    if (m_io.size() < request.length)
        m_io.resize(request.length);

    if (filestream_seek(m_stream, static_cast<int64_t>(request.offset), RETRO_VFS_SEEK_POSITION_START) < 0)
        return false;

    int64_t got = filestream_read(m_stream, m_io.data(), static_cast<int64_t>(request.length));
    if (got <= 0)
        return false;

    // A short supply is legal: rchd reissues the remainder.
    return rchd_feed(m_chd, m_io.data(), static_cast<size_t>(got)) == RCHD_OK;
}

bool ChdFile::pump(int (*step)(rchd_t*, rchd_request_t*))
{
    for (;;)
    {
        rchd_request_t request;
        int status = step(m_chd, &request);

        if (status == RCHD_OK)
            return true;

        if (status != RCHD_PENDING)
            return false;

        if (!service(request))
            return false;
    }
}

bool ChdFile::open(const std::string& filename)
{
    close();

    m_stream = filestream_open(filename.c_str(),
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!m_stream)
        return false;

    m_chd = rchd_new();
    if (!m_chd)
    {
        close();
        return false;
    }

    if (!pump(rchd_open_step))
    {
        close();
        return false;
    }

    const rchd_info_t* info = rchd_info(m_chd);
    if (!info || !info->hunk_bytes || (info->hunk_bytes % CHD_SECTOR_SIZE) != 0)
    {
        close();
        return false;
    }

    m_hunkData = reinterpret_cast<char*>(malloc(info->hunk_bytes));
    if (!m_hunkData)
    {
        close();
        return false;
    }

    m_hunkSize = info->hunk_bytes;
    m_hunkLogicalSize = m_hunkSize / CHD_SECTOR_SIZE * CD_SECTOR_SIZE;
    m_totalSize = static_cast<size_t>(m_hunkLogicalSize) * static_cast<size_t>(info->hunk_count);
    m_readPointer = 0;
    m_isDataHunk = true;
    m_hunkNumber = -1;

    return true;
}

bool ChdFile::isOpen() const
{
    return (m_chd != nullptr);
}

bool ChdFile::isChd() const
{
    return true;
}

void ChdFile::close()
{
    if (m_chd)
    {
        rchd_free(m_chd);
        m_chd = nullptr;
    }

    if (m_stream)
    {
        filestream_close(m_stream);
        m_stream = nullptr;
    }

    if (m_hunkData)
    {
        free(m_hunkData);
        m_hunkData = nullptr;
    }

    m_io.clear();
    m_io.shrink_to_fit();

    m_hunkSize = 0;
    m_hunkLogicalSize = 0;
    m_totalSize = 0;
    m_readPointer = 0;
    m_isDataHunk = true;
    m_hunkNumber = -1;
}

size_t ChdFile::size() const
{
    return m_totalSize;
}

int64_t ChdFile::pos() const
{
    return static_cast<int64_t>(m_readPointer);
}

bool ChdFile::seek(size_t pos)
{
    if (pos > m_totalSize)
    {
        m_readPointer = m_totalSize;
        return false;
    }

    m_readPointer = pos;
    return true;
}

bool ChdFile::skip(size_t off)
{
    return seek(static_cast<size_t>(pos()) + off);
}

bool ChdFile::eof() const
{
    return static_cast<size_t>(pos()) >= size();
}

size_t ChdFile::readData(void* data, size_t size)
{
    return read(data, size, true);
}

size_t ChdFile::readAudio(void* data, size_t size)
{
    return read(data, size, false);
}

size_t ChdFile::read(void* data, size_t size, bool dataMode)
{
    if (!isOpen())
        return 0;

    if (m_readPointer + size > m_totalSize)
        size = m_totalSize - m_readPointer;

    size_t bytesRead = 0;
    char* dst = reinterpret_cast<char*>(data);

    while(size)
    {
        uint32_t hunkNumber = static_cast<uint32_t>(m_readPointer / m_hunkLogicalSize);
        uint32_t sectorNumber = (m_readPointer % m_hunkLogicalSize) / CD_SECTOR_SIZE;
        uint32_t offset = m_readPointer % CD_SECTOR_SIZE;

        if (!fetchHunk(hunkNumber, dataMode))
            return bytesRead;

        uint32_t slice = static_cast<uint32_t>(std::min(size, static_cast<size_t>(CD_SECTOR_SIZE - offset)));
        char* src = m_hunkData + (sectorNumber * CHD_SECTOR_SIZE) + offset;
        std::memcpy(dst, src, slice);

        bytesRead += slice;
        dst += slice;
        size -= slice;
        m_readPointer += slice;
    }

    return bytesRead;
}

bool ChdFile::fetchHunk(uint32_t number, bool dataMode)
{
    if ((m_hunkNumber == static_cast<int32_t>(number)) && (m_isDataHunk == dataMode))
        return true;

    if (rchd_read_hunk_begin(m_chd, number, m_hunkData) != RCHD_OK)
    {
        m_hunkNumber = -1;
        return false;
    }

    if (!pump(rchd_read_step))
    {
        m_hunkNumber = -1;
        return false;
    }

    if (!dataMode)
        swab(m_hunkData, m_hunkSize);

    m_hunkNumber = static_cast<int32_t>(number);
    m_isDataHunk = dataMode;
    return true;
}

void ChdFile::swab(void* data, size_t size)
{
    uint16_t* start = reinterpret_cast<uint16_t*>(data);
    uint16_t* end = start + (size / 2);

    std::for_each(start, end, [](uint16_t& data) {
        data = BIG_ENDIAN_WORD(data);
    });
}

std::string ChdFile::readLine()
{
    return std::string();
}

std::string ChdFile::metadata(uint32_t searchTag, uint32_t searchIndex)
{
    if (!m_chd)
        return std::string();

    // The chain is cached during open, so this needs no I/O and the
    // payload can be read where it lies.
    const rchd_metadata_t* entry = rchd_metadata_find(m_chd, searchTag, searchIndex);
    if (!entry || !entry->data)
        return std::string();

    // The payload is NUL-terminated in every image that carries text,
    // but nothing guarantees it, so bound the length rather than trust it.
    uint32_t length = entry->length;
    const char* text = reinterpret_cast<const char*>(entry->data);
    const void* nul = std::memchr(text, 0, length);
    if (nul)
        length = static_cast<uint32_t>(reinterpret_cast<const char*>(nul) - text);

    return std::string(text, length);
}
