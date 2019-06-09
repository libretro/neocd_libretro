#ifndef FILE_H
#define FILE_H 1

#include <fstream>

#include "abstractfile.h"

class File : public AbstractFile
{
public:
    explicit File();
    virtual ~File() override;
    
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

protected:
    mutable std::ifstream m_stream;
    size_t m_fileSize;
};

#endif
