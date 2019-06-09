#include <regex>

#include "path.h"

#if defined(WIN32) || defined(WIN64)
#define strcasecmp _stricmp
#endif

bool string_compare_insensitive(const std::string& a, const std::string& b)
{
    if (a.length() != b.length())
        return false;

    return (strcasecmp(a.c_str(), b.c_str()) == 0);
}

std::string path_replace_filename(const std::string& path, const std::string& newFilename)
{
    static const std::regex pathWithoutFilenameRegex("^(.*[\\\\/]).+$");

    std::smatch matchResults;
    std::regex_match(path, matchResults, pathWithoutFilenameRegex);
    if (matchResults.size() < 2)
        return newFilename;

    return matchResults[1].str() + newFilename;
}

bool split_path(const std::string& path, std::string& filename, std::string& extension)
{
    static const std::regex SPLIT_PATH("^(.*/)?(?:$|(.+?)(?:(\\.[^.]*$)|$))");
    std::smatch match;

    if (!std::regex_match(path, match, SPLIT_PATH))
        return false;

    filename = match[2].str();
    extension = match[3].str();

    return true;
}
