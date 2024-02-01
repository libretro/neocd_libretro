#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <cstdint>
#include <string>
#include <vector>

namespace Archive
{
    enum Type
    {
        TypeUnknown,
        TypeZip
    };

    std::vector<std::string> getFileList(const std::string& archiveFilename);

    bool readFile(const std::string& path, void* buffer, size_t maximumSize, size_t* reallyRead);
    int64_t getFileSize(const std::string& path);
};

#endif // ARCHIVE_H
