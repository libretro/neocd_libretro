#include <lists/dir_list.h>
#include <string>
#include <vector>

#include "archive.h"
#include "bios.h"
#include "libretro_bios.h"
#include "libretro_common.h"
#include "libretro_log.h"
#include "memory.h"
#include "neogeocd.h"
#include "path.h"
#include "stringlist.h"

// Load each file from the list and test for validity
static void lookForBIOSInternal(const std::vector<std::string>& fileList)
{
    for(const std::string& path : fileList)
    {
        if (!path_is_bios_file(path.c_str()))
            continue;

        BiosListEntry newEntry;

        std::string archive;
        std::string file;
        split_compressed_path(path, archive, file);

        // For archives we want something of the form archive.zip#file.bin
        if (!archive.empty())
        {
            newEntry.filename = path;
            newEntry.description = make_path_separator(path_basename(archive.c_str()), "#", file.c_str());
        }
        else
        {
            newEntry.filename = file.c_str();
            newEntry.description = path_basename(file.c_str());
        }

	// Probably != is more correct but using < to keep compatibility with old versions that would accept larger files
	if (Archive::getFileSize(newEntry.filename) < (int64_t) Memory::ROM_SIZE)
            continue;

	uint8_t buffer[512]; // We only need that much to identify the BIOS
        size_t reallyRead;

	if (!Archive::readFile(newEntry.filename, &buffer[0], sizeof(buffer), &reallyRead))
            continue;

	if (reallyRead != sizeof(buffer))
            continue;

	Bios::autoByteSwap(&buffer[0], sizeof(buffer));

        newEntry.type = Bios::identify(&buffer[0]);

        if (newEntry.type.first == Bios::Family::Invalid)
            continue;

        newEntry.description.append(" (");
        newEntry.description.append(Bios::description(newEntry.type));
        newEntry.description.append(")");

        globals.biosList.push_back(newEntry);
    }
}

// Search for BIOS files
void Libretro::Bios::init()
{
    // Clear everything
    globals.biosList.clear();

    // Get the system path
    const auto systemPath = system_path();

    std::vector<std::string> fileList;

    // Scan the system directory
    StringList file_list(dir_list_new(systemPath.c_str(), nullptr, false, true, true, true));

    for(const string_list_elem& elem : file_list)
        {
            if (path_is_bios_file(elem.data))
                fileList.push_back(std::string(elem.data));
            else if (path_is_archive(elem.data))
            {
                auto archiveList = Archive::getFileList(std::string(elem.data));

                for(const std::string& file : archiveList)
                {
                    if (path_is_bios_file(file.c_str()))
                        fileList.push_back(file);
                }
            }
        }

    // Load all files and check for validity
    lookForBIOSInternal(fileList);

    // Sort the list
    std::sort(globals.biosList.begin(), globals.biosList.end(), [](const BiosListEntry& a, const BiosListEntry& b) {
        return strcmp(a.description.c_str(), b.description.c_str()) < 0;
    });
}

// Load, byte swap and patch BIOS
bool Libretro::Bios::load()
{
    if (!globals.biosList.size())
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "No BIOS detected!\n");
        return false;
    }

    size_t reallyRead;

    if (!Archive::readFile(globals.biosList[globals.biosIndex].filename, neocd->memory.rom, Memory::ROM_SIZE, &reallyRead))
    {
        Libretro::Log::message(RETRO_LOG_ERROR, "Could not load BIOS %s\n", globals.biosList[globals.biosIndex].filename.c_str());
        return false;
    }

    neocd->biosType = globals.biosList[globals.biosIndex].type.first;

    ::Bios::autoByteSwap(neocd->memory.rom);

    ::Bios::patch(neocd->memory.rom, globals.biosList[globals.biosIndex].type, globals.cdSpeedHack);

    return true;
}

size_t Libretro::Bios::descriptionToIndex(const char* description)
{
    for (size_t result = 0; result < globals.biosList.size(); ++result)
    {
        if (!strcmp(description, globals.biosList[result].description.c_str()))
            return result;
    }

    return 0;
}
