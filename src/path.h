#ifndef PATH_H
#define PATH_H 1

#include <string>

bool string_compare_insensitive(const std::string& a, const std::string& b);

std::string path_replace_filename(const std::string& path, const std::string& newFilename);

bool split_path(const std::string& path, std::string& filename, std::string& extension);

#endif
