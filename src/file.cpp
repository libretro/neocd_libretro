#include <algorithm>

#include "file.h"

File::File() :
    AbstractFile(),
    m_stream(),
    m_fileSize(0)
{
}

File::~File()
{
    close();
}

bool File::open(const std::string& filename)
{
    close();

    m_stream.open(filename, std::ios::in | std::ios::binary);
    if (m_stream.is_open())
    {
        std::ifstream::pos_type currentPos = m_stream.tellg();
        m_stream.seekg(0, std::ios::end);
        m_fileSize = static_cast<size_t>(m_stream.tellg());
        m_stream.seekg(currentPos, std::ios::beg);

        return true;
    }

    return false;
}

bool File::isOpen() const
{
    return m_stream.is_open();
}

bool File::isChd() const
{
    return false;
}

void File::close()
{
    if (isOpen())
        m_stream.close();

    m_fileSize = 0;
}

size_t File::size() const
{
    return m_fileSize;
}

int64_t File::pos() const
{
    return static_cast<int64_t>(m_stream.tellg());
}

bool File::seek(size_t pos)
{
    if (!isOpen())
        return false;

    m_stream.clear();
    std::streamoff destination = static_cast<std::streamoff>(pos);
    m_stream.seekg(std::min(destination, static_cast<std::streamoff>(m_fileSize)), std::ios::beg);
    return (m_stream.tellg() == destination);
}

bool File::skip(size_t off)
{
    return seek(static_cast<size_t>(pos()) + off);
}

bool File::eof() const
{
    return m_stream.eof();
}

size_t File::readData(void* data, size_t size)
{
    if (!isOpen())
        return size_t(0);

    m_stream.clear();
    m_stream.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));

    return static_cast<size_t>(m_stream.gcount());
}

size_t File::readAudio(void* data, size_t size)
{
    return readData(data, size);
}
