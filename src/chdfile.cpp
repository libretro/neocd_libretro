#include <algorithm>
#include <cstring>

#include "chdfile.h"
#include "endian.h"

constexpr int CHD_SECTOR_SIZE = 2352 + 96;
constexpr int CD_SECTOR_SIZE = 2352;

ChdFile::ChdFile() : 
    AbstractFile(),
    m_file(nullptr),
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

bool ChdFile::open(const std::string& filename)
{
    close();

    chd_file* chdFile = nullptr;
    chd_error status = chd_open(filename.c_str(), CHD_OPEN_READ, nullptr, &chdFile);
    if (status != CHDERR_NONE)
        return false;

    m_file = chdFile;

    const chd_header* chdHeader = chd_get_header(m_file);

    if ((chdHeader->hunkbytes % CHD_SECTOR_SIZE) != 0)
    {
        close();
        return false;
    }

    m_hunkData = reinterpret_cast<char*>(std::malloc(chdHeader->hunkbytes));
    if (!m_hunkData)
    {
        close();
        return false;
    }

    m_hunkSize = chdHeader->hunkbytes;
    m_hunkLogicalSize = m_hunkSize / CHD_SECTOR_SIZE * CD_SECTOR_SIZE;
    m_totalSize = static_cast<size_t>(m_hunkLogicalSize) * static_cast<size_t>(chdHeader->totalhunks);
    m_readPointer = 0;
    m_isDataHunk = true;
    m_hunkNumber = -1;

    return true;
}

bool ChdFile::isOpen() const
{
    return (m_file != nullptr);
}

bool ChdFile::isChd() const
{
    return true;
}

void ChdFile::close()
{
    if (m_file)
    {
        chd_close(m_file);
        m_file = nullptr;
    }

    if (m_hunkData)
    {
        std::free(m_hunkData);
        m_hunkData = nullptr;
    }

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

    chd_error status = chd_read(m_file, number, m_hunkData);
    if (status != CHDERR_NONE)
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
    if (!m_file)
        return std::string();

    char buffer[256];
    uint32_t metadataLength;

    chd_error err = chd_get_metadata(m_file, searchTag, searchIndex, buffer, 255, &metadataLength, nullptr, nullptr);
    if (err != CHDERR_NONE)
        return std::string();

    buffer[metadataLength] = 0;

    return std::string(buffer);
}
