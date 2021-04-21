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

std::string make_save_path(const char* filename);

std::string make_path(const char* path, const char* filename);

std::string make_srm_path(bool per_content_saves, const char* content_path);

bool string_compare_insensitive(const std::string& a, const std::string& b);

bool string_compare_insensitive(const char* a, const char* b);

bool path_is_archive(const std::string& path);

bool path_is_bios_file(const std::string& path);

void split_compressed_path(const std::string& path, std::string& archive, std::string& file);

#endif
