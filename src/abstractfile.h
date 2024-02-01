#ifndef ABSTRACTFILE_H
#define ABSTRACTFILE_H 1

#include <cstdint>
#include <string>

class AbstractFile
{
public:
    virtual ~AbstractFile() { };

    virtual bool open(const std::string& filename) = 0;

    virtual bool isOpen() const = 0;

    virtual bool isChd() const = 0;

    virtual void close() = 0;

    virtual size_t size() const = 0;

    virtual int64_t pos() const = 0;

    virtual bool seek(size_t pos) = 0;

    virtual bool skip(size_t off) = 0;

    virtual bool eof() const = 0;

    virtual size_t readData(void* data, size_t size) = 0;

    virtual size_t readAudio(void* data, size_t size) = 0;

    virtual std::string readLine() = 0;
};

#endif
