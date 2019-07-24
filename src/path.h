#ifndef PATH_H
#define PATH_H 1

#include <string>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <file/file_path.h>

std::string path_replace_filename(const char* path, const char* new_filename);

std::string path_get_filename(const char* path);

bool path_is_empty(const char* buffer);

bool path_ends_with_slash(const char* buffer);

std::string system_path();

std::string make_path_separator(const char* path, const char* separator, const char* filename);

std::string make_system_path(const char* filename);

std::string make_path(const char* path, const char* filename);

bool string_compare_insensitive(const char* a, const char* b);

#endif
