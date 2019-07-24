#include <retro_miscellaneous.h>

#include "path.h"

extern const char* systemDirectory;

static void system_path_internal(char* buffer, size_t len)
{
    if (path_is_empty(systemDirectory))
        snprintf(buffer, 3, ".%s", path_default_slash());
    else
        strlcpy(buffer, systemDirectory, len);
    
    if (!path_ends_with_slash(buffer))
        strlcat(buffer, path_default_slash(), len);

    strlcat(buffer, "neocd", len);
}

std::string path_replace_filename(const char* path, const char* new_filename)
{
    char buffer[PATH_MAX_LENGTH];
    fill_pathname_basedir(buffer, path, sizeof(buffer) - 1);
    strlcat(buffer, new_filename, sizeof(buffer) - 1);
    return std::string(buffer);
}

std::string path_get_filename(const char* path)
{
    char buffer[PATH_MAX_LENGTH];
    strlcpy(buffer, path_basename(path), sizeof(buffer) - 1);
    path_remove_extension(buffer);
    return std::string(buffer);
}

bool path_is_empty(const char* buffer)
{
    if (!buffer)
        return true;

    return (buffer[0] == 0);
}

bool path_ends_with_slash(const char* buffer)
{
    size_t path_len = strlen(buffer);

    if (!path_len)
        return false;

    return path_char_is_slash(buffer[path_len - 1]);
}

std::string system_path()
{
    char buffer[PATH_MAX_LENGTH];

    system_path_internal(buffer, sizeof(buffer) - 1);

    return std::string(buffer);
}

std::string make_system_path(const char* filename)
{
    char buffer[PATH_MAX_LENGTH];

    system_path_internal(buffer, sizeof(buffer) - 1);

    strlcat(buffer, path_default_slash(), sizeof(buffer) - 1);
    strlcat(buffer, filename, sizeof(buffer) - 1);

    return std::string(buffer);
}

std::string make_path_separator(const char* path, const char* separator, const char* filename)
{
    char buffer[PATH_MAX_LENGTH];

    strlcpy(buffer, path, sizeof(buffer) - 1);
    strlcat(buffer, separator, sizeof(buffer) - 1);
    strlcat(buffer, filename, sizeof(buffer) - 1);

    return std::string(buffer);
}

std::string make_path(const char* path, const char* filename)
{
    return make_path_separator(path, path_default_slash(), filename);
}

bool string_compare_insensitive(const char* a, const char* b)
{
    return (strcasecmp(a, b) == 0);
}

