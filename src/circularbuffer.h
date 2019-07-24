#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

#include <cstdint>
#include <cstring>
#include <cassert>

template<class T>
class CircularBuffer
{
public:
    explicit CircularBuffer() : 
        m_capacity(0), 
        m_dataSize(0), 
        m_frontIndex(0), 
        m_backIndex(0), 
        m_buffer(nullptr)
    {}

    virtual ~CircularBuffer()
    {
        releaseBuffer();
    }
    // Non copyable
    CircularBuffer(const CircularBuffer&) = delete;
    
    // Non copyable
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    size_t capacity() const
    {
        return m_capacity;
    }

    bool empty() const
    {
        return (availableToRead() == 0);
    }

    void clear()
    {
        m_dataSize = 0;
        m_frontIndex = 0;
        m_backIndex = 0;
    }

    void set_capacity(size_t newCapacity)
    {
        assert(newCapacity > 0);

        if (newCapacity == m_capacity)
            return;

        releaseBuffer();
        m_capacity = newCapacity;
        m_buffer = reinterpret_cast<T*>(std::malloc(m_capacity * sizeof(T)));
        m_dataSize = 0;
        m_frontIndex = 0;
        m_backIndex = 0;
    }

    size_t availableToRead() const
    {
        return m_dataSize;
    }

    size_t availableToWrite() const
    {
        return m_capacity - availableToRead();
    }

    void push_back(const T& val)
    {
        assert(availableToWrite() != 0);
        m_buffer[m_backIndex] = val;
        m_dataSize++;
        incrementIndex(m_backIndex, 1);
    }

    void push_back(const T* ptr, size_t num)
    {
        assert(availableToWrite() >= num);
        size_t toWrapAround = m_capacity - m_backIndex;

        if (num > toWrapAround)
        {
            std::memcpy(&m_buffer[m_backIndex], ptr, toWrapAround * sizeof(T));
            m_backIndex = 0;
            m_dataSize += toWrapAround;
            ptr += toWrapAround;
            num -= toWrapAround;
        }

        std::memcpy(&m_buffer[m_backIndex], ptr, num * sizeof(T));
        incrementIndex(m_backIndex, num);
        m_dataSize += num;
    }

    void push_front(const T& val)
    {
        assert(availableToWrite() != 0);
        decrementIndex(m_frontIndex, 1);
        m_dataSize++;
        m_buffer[m_frontIndex] = val;
    }

    void push_front(const T* ptr, size_t num)
    {
        assert(availableToWrite() >= num);

        decrementIndex(m_frontIndex, num);

        size_t idx = m_frontIndex;
        size_t toWrapAround = m_capacity - idx;

        if (num > toWrapAround)
        {
            std::memcpy(&m_buffer[idx], ptr, toWrapAround * sizeof(T));
            idx = 0;
            m_dataSize += toWrapAround;
            ptr += toWrapAround;
            num -= toWrapAround;
        }

        std::memcpy(&m_buffer[idx], ptr, num * sizeof(T));
        m_dataSize += num;
    }

    const T& front() const
    {
        assert(availableToRead() != 0);
        return m_buffer[m_frontIndex];
    }

    const T& back() const
    {
        assert(availableToRead() != 0);
        size_t n = m_backIndex;
        decrementIndex(n, 1);
        return m_buffer[n];
    }

    const T& at(size_t n) const
    {
        assert(n < availableToRead());
        incrementIndex(n, m_frontIndex);
        return m_buffer[n];
    }

    void pop_back()
    {
        assert(availableToRead() != 0);
        m_dataSize--;
        decrementIndex(m_backIndex, 1);
    }

    void pop_back(T* dst, size_t num)
    {
        assert(availableToRead() >= num);

        decrementIndex(m_backIndex, num);

        size_t idx = m_backIndex;
        size_t toWrapAround = m_capacity - idx;

        if (num > toWrapAround)
        {
            std::memcpy(dst, &m_buffer[idx], toWrapAround * sizeof(T));
            idx = 0;
            m_dataSize -= toWrapAround;
            dst += toWrapAround;
            num -= toWrapAround;
        }

        std::memcpy(dst, &m_buffer[idx], num * sizeof(T));
        m_dataSize -= num;
    }

    void pop_front()
    {
        assert(availableToRead() != 0);
        m_dataSize--;
        incrementIndex(m_frontIndex, 1);
    }

    void pop_front(T* dst, size_t num)
    {
        assert(availableToRead() >= num);

        size_t toWrapAround = m_capacity - m_frontIndex;

        if (num > toWrapAround)
        {
            std::memcpy(dst, &m_buffer[m_frontIndex], toWrapAround * sizeof(T));
            m_frontIndex = 0;
            m_dataSize -= toWrapAround;
            dst += toWrapAround;
            num -= toWrapAround;
        }

        std::memcpy(dst, &m_buffer[m_frontIndex], num * sizeof(T));
        incrementIndex(m_frontIndex, num);
        m_dataSize -= num;
    }

protected:
    void releaseBuffer()
    {
        if (m_buffer)
        {
            std::free(m_buffer);
            m_buffer = nullptr;
        }
    }

    inline void incrementIndex(size_t& index, size_t displacement) const
    {
        assert(m_capacity != 0);
        index = (index + displacement) % m_capacity;
    }

    inline void decrementIndex(size_t& index, size_t displacement) const
    {
        assert(m_capacity != 0);
        index = (index - displacement - 1) % m_capacity;
    }

    inline size_t indexDistance(const size_t& index1, const size_t& index2) const
    {
        if (index2 >= index1)
            return index2 - index1;

        return (m_capacity - index1) + index2;
    }

    size_t  m_capacity;
    size_t  m_dataSize;
    size_t  m_frontIndex;
    size_t  m_backIndex;
    T*      m_buffer;
};

#endif // CIRCULARBUFFER_H
