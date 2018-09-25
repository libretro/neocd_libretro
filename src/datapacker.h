#ifndef DATAPACKER_H
#define DATAPACKER_H

#include <array>
#include <cstdint>
#include <fstream>

class DataPacker
{
public:
    struct PointerMap
    {
        char* base;
        uint32_t size;
    };

    struct ConstPointerMap
    {
        const char* base;
        uint32_t size;
    };

    DataPacker();

    DataPacker(DataPacker&& other);

    DataPacker(char* data, size_t size, size_t capacity);

    ~DataPacker();

    char* begin();
    const char* cbegin() const;
    char* end();
    const char* cend() const;

    const char * data() const;

    bool empty() const;

    size_t size() const;
    bool resize(size_t newSize);
    void reserve(size_t size);
    void shrink_to_fit();

    void clear();

    void swap(DataPacker& other);

    size_t tellp() const;
    void seekp(const size_t &offset, std::ios_base::seekdir way);

    void push(const char* data, size_t size);
    void pop(char* data, size_t size);

    bool pushPointer(const char* ptr, const char* base, uint32_t maxValue);
    const char *popPointerConst(const char* base, uint32_t maxValue);
    char *popPointer(char* base, uint32_t maxValue);

    bool pushPointerMulti(const char* ptr, const ConstPointerMap* pointerMap, size_t mapSize);
    const char* popPointerMultiConst(const ConstPointerMap* pointerMap, size_t size);
    char* popPointerMulti(const PointerMap *pointerMap, size_t size);

    bool fail() const;
    void setstate(bool state);
    void clearstate();

    template<class T>
    DataPacker& operator<<(const T& value)
    {
        push(reinterpret_cast<const char*>(&value), sizeof(T));
        return *this;
    }

    template<class T>
    DataPacker& operator>>(T& value)
    {
        pop(reinterpret_cast<char*>(&value), sizeof(T));
        return *this;
    }

    DataPacker(const DataPacker& other) = delete;
    DataPacker& operator=(const DataPacker& other) = delete;

protected:
    char* m_data;
    size_t m_size;
    size_t m_capacity;
    size_t m_granularity;
    size_t m_readPosition;
    bool m_state;
    bool m_ownBuffer;
};

#endif // DATAPACKER_H
