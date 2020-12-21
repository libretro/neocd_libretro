#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "path.h"

static const char* NEOCD_SYSTEM_SUBDIR        = "neocd";
static const char* NEOCD_DEFAULT_SRM_FILENAME = "neocd";
static const char* NEOCD_SRM_EXT              = ".srm";

extern const char* systemDirectory;
extern const char* saveDirectory;

static void system_path_internal(char* buffer, size_t len)
{
   if (path_is_empty(systemDirectory))
   {
      buffer[0] = '.';
      buffer[1] = PATH_DEFAULT_SLASH_C();
      buffer[2] = '\0';
   }
   else
      strlcpy(buffer, systemDirectory, len);

   if (!path_ends_with_slash(buffer))
      strlcat(buffer, PATH_DEFAULT_SLASH(), len);

   strlcat(buffer, NEOCD_SYSTEM_SUBDIR, len);
}

static void save_path_internal(char* buffer, size_t len)
{
   /* If save directory is unset, use system directory */
   if (path_is_empty(saveDirectory))
      system_path_internal(buffer, len);
   else
   {
      strlcpy(buffer, saveDirectory, len);

      // Remove trailing slash, if required
      if (path_ends_with_slash(buffer))
      {
         size_t path_len = strlen(buffer);

         if (path_len)
            buffer[path_len - 1] = '\0';
      }
   }
}

std::string path_replace_filename(const char* path, const char* new_filename)
{
   char buffer[PATH_MAX_LENGTH];
   buffer[0] = '\0';

   if (!path_is_empty(path))
      fill_pathname_basedir(buffer, path, sizeof(buffer) - 1);

   if (!path_is_empty(new_filename))
      strlcat(buffer, new_filename, sizeof(buffer) - 1);

   return std::string(buffer);
}

std::string path_get_filename(const char* path)
{
   char buffer[PATH_MAX_LENGTH];
   buffer[0] = '\0';

   if (!path_is_empty(path))
   {
      const char *basename = path_basename(path);

      if (!path_is_empty(basename))
      {
         strlcpy(buffer, basename, sizeof(buffer) - 1);
         path_remove_extension(buffer);
      }
   }

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

   return PATH_CHAR_IS_SLASH(buffer[path_len - 1]);
}

std::string system_path()
{
   char buffer[PATH_MAX_LENGTH];
   buffer[0] = '\0';

   system_path_internal(buffer, sizeof(buffer) - 1);

   return std::string(buffer);
}

std::string make_system_path(const char* filename)
{
   char buffer[PATH_MAX_LENGTH];
   buffer[0] = '\0';

   system_path_internal(buffer, sizeof(buffer) - 1);

   strlcat(buffer, PATH_DEFAULT_SLASH(), sizeof(buffer) - 1);
   if (!path_is_empty(filename))
      strlcat(buffer, filename, sizeof(buffer) - 1);

   return std::string(buffer);
}

std::string make_save_path(const char* filename)
{
   char buffer[PATH_MAX_LENGTH];
   buffer[0] = '\0';

   save_path_internal(buffer, sizeof(buffer) - 1);

   strlcat(buffer, PATH_DEFAULT_SLASH(), sizeof(buffer) - 1);
   if (!path_is_empty(filename))
      strlcat(buffer, filename, sizeof(buffer) - 1);

   return std::string(buffer);
}

std::string make_srm_path(bool per_content_saves, const char* content_path)
{
   char srm_filename[PATH_MAX_LENGTH];
   srm_filename[0] = '\0';

   // If per-content saves are enabled, use content
   // file name to generate srm file name
   if (per_content_saves)
   {
      std::string content_filename = path_get_filename(content_path);

      if (!content_filename.empty())
      {
         strlcpy(srm_filename, content_filename.c_str(), sizeof(srm_filename) - 1);
         strlcat(srm_filename, NEOCD_SRM_EXT,            sizeof(srm_filename) - 1);
      }
   }

   // If per-content saves are disabled, or content
   // file name was empty, get default srm file name
   if (!per_content_saves || path_is_empty(srm_filename))
   {
      strlcat(srm_filename, NEOCD_DEFAULT_SRM_FILENAME, sizeof(srm_filename) - 1);
      strlcat(srm_filename, NEOCD_SRM_EXT,              sizeof(srm_filename) - 1);
   }

   // Return full srm file path
   if (per_content_saves)
      return make_save_path(srm_filename);
   else
      return make_system_path(srm_filename);
}

std::string make_path_separator(const char* path, const char* separator, const char* filename)
{
   char buffer[PATH_MAX_LENGTH];
   buffer[0] = '\0';

   if (!path_is_empty(path))
      strlcpy(buffer, path, sizeof(buffer) - 1);

   if (!path_is_empty(separator))
      strlcat(buffer, separator, sizeof(buffer) - 1);

   if (!path_is_empty(filename))
      strlcat(buffer, filename, sizeof(buffer) - 1);

   return std::string(buffer);
}

std::string make_path(const char* path, const char* filename)
{
   return make_path_separator(path, PATH_DEFAULT_SLASH(), filename);
}

bool string_compare_insensitive(const char* a, const char* b)
{
   return string_is_equal_case_insensitive(a, b);
}

