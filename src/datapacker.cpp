#include <algorithm>
#include <cstring>

#include "clamp.h"
#include "datapacker.h"

DataPacker::DataPacker() : 
    m_data(nullptr), 
    m_size(0), 
    m_capacity(0), 
    m_granularity(1024), 
    m_readPosition(0), 
    m_state(true), 
    m_ownBuffer(true)
{ }

DataPacker::DataPacker(DataPacker&& other) : 
    DataPacker()
{
    swap(other);
}

DataPacker::DataPacker(
        char *data, 
        size_t size, 
        size_t capacity) : 
    m_data(data), 
    m_size(size), 
    m_capacity(capacity), 
    m_granularity(1024), 
    m_readPosition(0), 
    m_state(true), 
    m_ownBuffer(false)
{ }

DataPacker::~DataPacker()
{
    if (m_data && m_ownBuffer)
        std::free(m_data);
}

char *DataPacker::begin()
{
    return m_data;
}

const char *DataPacker::cbegin() const
{
    return m_data;
}

char *DataPacker::end()
{
    return m_data + m_size;
}

const char *DataPacker::cend() const
{
    return m_data + m_size;
}

const char *DataPacker::data() const
{
    return m_data;
}

bool DataPacker::empty() const
{
    return (m_size == 0);
}

size_t DataPacker::size() const
{
    return m_size;
}

bool DataPacker::resize(size_t newSize)
{
    if (!m_ownBuffer)
    {
        m_size = std::min(newSize, m_capacity);
        return (newSize <= m_capacity);
    }

    if (newSize == m_size)
        return true;

    reserve(newSize);
    m_size = newSize;

    return true;
}

void DataPacker::reserve(size_t size)
{
    if (!m_ownBuffer)
        return;

    size_t newCapacity;

    if (size % m_granularity)
        newCapacity = ((size / m_granularity) + 1) * m_granularity;
    else
        newCapacity = size;

    if (newCapacity <= m_capacity)
        return;

    m_capacity = newCapacity;

    if (m_data)
        m_data = reinterpret_cast<char *>(std::realloc(m_data, m_capacity));
    else
        m_data = reinterpret_cast<char *>(std::malloc(m_capacity));
}

void DataPacker::shrink_to_fit()
{
    if (!m_ownBuffer)
        return;

    if (m_size == m_capacity)
        return;

    if (m_size)
    {
        m_data = reinterpret_cast<char *>(std::realloc(m_data, m_size));
        m_capacity = m_size;
    }
    else
    {
        std::free(m_data);
        m_data = nullptr;
        m_capacity = 0;
    }
}

void DataPacker::clear()
{
    if (m_ownBuffer)
    {
        if (m_data)
            std::free(m_data);

        m_data = nullptr;
        m_capacity = 0;
    }

    m_size = 0;
    m_granularity = 1024;
    m_readPosition = 0;
    m_state = true;
}

void DataPacker::swap(DataPacker &other)
{
    std::swap(m_data, other.m_data);
    std::swap(m_size, other.m_size);
    std::swap(m_capacity, other.m_capacity);
    std::swap(m_granularity, other.m_granularity);
    std::swap(m_readPosition, other.m_readPosition);
    std::swap(m_state, other.m_state);
    std::swap(m_ownBuffer, other.m_ownBuffer);
}

size_t DataPacker::tellp() const
{
    return m_readPosition;
}

void DataPacker::seekp(const size_t &offset, std::ios_base::seekdir way)
{
    int64_t newOffset;

    switch (way)
    {
    case std::ios_base::cur:
        newOffset = m_readPosition;
        newOffset += offset;
        break;
    case std::ios_base::end:
        newOffset = m_size;
        newOffset -= offset;
        break;
    default:
        newOffset = offset;
        break;
    }

    newOffset = std::clamp(newOffset, static_cast<int64_t>(0), static_cast<int64_t>(m_size));

    m_readPosition = static_cast<size_t>(newOffset);
}

void DataPacker::push(const char *data, size_t size)
{
    size_t oldSize = m_size;
    if (!resize(m_size + size))
    {
        m_state = false;
        return;
    }

    std::memcpy(m_data + oldSize, data, size);
}

void DataPacker::pop(char *data, size_t size)
{
    if (m_readPosition + size > m_size)
    {
        m_state = false;
        return;
    }

    std::memcpy(data, m_data + m_readPosition, size);
    m_readPosition += size;
}

bool DataPacker::pushPointer(const char *ptr, const char *base, uint32_t maxValue)
{
    uint32_t offset;

    if (!ptr)
        offset = 0xFFFFFFFF;
    else
    {
        if (ptr < base)
        {
            m_state = false;
            return false;
        }

        offset = static_cast<uint32_t>(ptr - base);

        if (offset > maxValue)
        {
            m_state = false;
            return false;
        }
    }

    push(reinterpret_cast<char *>(&offset), sizeof(uint32_t));
    return true;
}

const char *DataPacker::popPointerConst(const char *base, uint32_t maxValue)
{
    uint32_t offset;
    *this >> offset;

    if (fail())
        return nullptr;

    if (offset == 0xFFFFFFFF)
        return nullptr;

    if (offset > maxValue)
    {
        m_state = false;
        return nullptr;
    }

    return base + offset;
}

char *DataPacker::popPointer(char *base, uint32_t maxValue)
{
    return const_cast<char *>(popPointerConst(base, maxValue));
}

bool DataPacker::pushPointerMulti(const char *ptr, const DataPacker::ConstPointerMap *pointerMap, size_t mapSize)
{
    uint32_t offset;

    if (!ptr)
    {
        uint32_t i = 0;
        offset = 0xFFFFFFFF;
        *this << i;
        *this << offset;
        return true;
    }

    for (uint32_t i = 0; i < mapSize; ++i)
    {
        if (ptr < pointerMap[i].base)
            continue;

        offset = static_cast<uint32_t>(ptr - pointerMap[i].base);

        if (offset >= pointerMap[i].size)
            continue;

        *this << i;
        *this << offset;
        return true;
    }

    m_state = false;
    return false;
}

const char *DataPacker::popPointerMultiConst(const ConstPointerMap *pointerMap, size_t size)
{
    uint32_t i;
    uint32_t offset;

    *this >> i;
    *this >> offset;

    if (fail())
        return nullptr;

    if (i >= size)
    {
        m_state = false;
        return nullptr;
    }

    if (offset == 0xFFFFFFFF)
        return nullptr;

    if (offset >= pointerMap[i].size)
    {
        m_state = false;
        return nullptr;
    }

    return pointerMap[i].base + offset;
}

char *DataPacker::popPointerMulti(const DataPacker::PointerMap *pointerMap, size_t size)
{
    return const_cast<char *>(popPointerMultiConst(reinterpret_cast<const DataPacker::ConstPointerMap *>(pointerMap), size));
}

bool DataPacker::fail() const
{
    return (!m_state);
}

void DataPacker::setstate(bool state)
{
    m_state = state;
}

void DataPacker::clearstate()
{
    m_state = true;
}
