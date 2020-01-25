cmake_minimum_required(VERSION 3.1)

# Path to CMake scripts
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmakescripts")

# Avoid having empty buildtype
set(CMAKE_BUILD_TYPE_INIT "Release")

# Project name
project(neocd_libretro)
set(PROJECT_NAME neocd_libretro)

# Under Windows we want static libraries
if (WIN32)
    set(BUILD_STATIC 1)
endif (WIN32)

# Check if the compiler can handle C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the current directory to includes
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

################################################################
#                       Compiler options                       #
################################################################

# Set to "-march=native" on non x86 platforms
set(MACHINE_OPTIONS "-march=x86-64 -mtune=generic")

# Base options
set(BASE_OPTIONS "-Ofast -fomit-frame-pointer -ffast-math -Wall")

# C Defines
set(C_DEFINES "-DHAVE_COMPRESSION -DHAVE_ZLIB -DHAVE_7ZIP -D_7ZIP_ST -DHAVE_FLAC")

# C++ specific options
set(CXX_OPTIONS "-std=c++11 -fno-exceptions -fno-rtti")

# Set CFLAGS
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${MACHINE_OPTIONS} ${BASE_OPTIONS} ${C_DEFINES}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${MACHINE_OPTIONS} ${BASE_OPTIONS} ${CXX_OPTIONS}")

# Generate profile guided optimization files
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-generate=${CMAKE_BINARY_DIR}")

# Use profile guided optimization files
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-use=${CMAKE_BINARY_DIR}")

# Linker options
if (CMAKE_BUILD_TYPE MATCHES Release)
    set(LINK_OPTIONS -s)
endif()

# Library path
#set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -L. ")

# Define the CXX sources
set ( CXX_SRCS
    ${CMAKE_SOURCE_DIR}/src/3rdparty/ym/ym2610.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/z80/z80.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/z80/z80daisy.cpp
    ${CMAKE_SOURCE_DIR}/src/audio.cpp
    ${CMAKE_SOURCE_DIR}/src/cdrom.cpp
    ${CMAKE_SOURCE_DIR}/src/cdromtoc.cpp
    ${CMAKE_SOURCE_DIR}/src/chdfile.cpp
    ${CMAKE_SOURCE_DIR}/src/datapacker.cpp
    ${CMAKE_SOURCE_DIR}/src/file.cpp
    ${CMAKE_SOURCE_DIR}/src/flacfile.cpp
    ${CMAKE_SOURCE_DIR}/src/input.cpp
    ${CMAKE_SOURCE_DIR}/src/lc8951.cpp
    ${CMAKE_SOURCE_DIR}/src/libretro.cpp
    ${CMAKE_SOURCE_DIR}/src/m68kintf.cpp
    ${CMAKE_SOURCE_DIR}/src/memory.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_backupram.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_cdintf.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_input.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_mapped.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_paletteram.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_switches.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_video.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_z80comm.cpp
    ${CMAKE_SOURCE_DIR}/src/neogeocd.cpp
    ${CMAKE_SOURCE_DIR}/src/oggfile.cpp
    ${CMAKE_SOURCE_DIR}/src/path.cpp
    ${CMAKE_SOURCE_DIR}/src/timeprofiler.cpp
    ${CMAKE_SOURCE_DIR}/src/timer.cpp
    ${CMAKE_SOURCE_DIR}/src/timergroup.cpp
    ${CMAKE_SOURCE_DIR}/src/video.cpp
    ${CMAKE_SOURCE_DIR}/src/wavfile.cpp
    ${CMAKE_SOURCE_DIR}/src/z80intf.cpp
)

# Define the C sources
set ( C_SRCS
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/compat/compat_posix_string.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/compat/compat_strcasestr.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/compat/compat_strl.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/compat/fopen_utf8.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/encodings/encoding_crc32.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/encodings/encoding_utf.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/file/archive_file.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/file/archive_file_7z.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/file/archive_file_zlib.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/file/file_path.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/file/retro_dirent.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_bitstream.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_cdrom.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_chd.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_flac.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_flac_codec.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_huffman.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_lzma.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/formats/libchdr/libchdr_zlib.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/lists/dir_list.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/lists/file_list.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/lists/string_list.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/streams/file_stream.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/streams/trans_stream.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/streams/trans_stream_pipe.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/streams/trans_stream_zlib.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/vfs/vfs_implementation.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/7zBuf.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/7zCrc.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/7zCrcOpt.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/7zDec.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/7zFile.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/7zIn.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/7zStream.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/Bcj2.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/Bra.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/Bra86.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/LzFind.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/Lzma2Dec.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/LzmaDec.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip/LzmaEnc.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kcpu.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kopac.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kopdm.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kopnz.c
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kops.c
)

# Find the required libraries
FIND_PACKAGE(FLAC REQUIRED)
FIND_PACKAGE(Ogg REQUIRED)
FIND_PACKAGE(Vorbis REQUIRED)
FIND_PACKAGE(Threads REQUIRED)
FIND_PACKAGE(ZLIB REQUIRED)

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-common/include
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps
    ${CMAKE_SOURCE_DIR}/src/3rdparty/libretro-deps/7zip
    ${FLAC_INCLUDE_DIR}
    ${OGG_INCLUDE_DIR}
    ${VORBIS_INCLUDE_DIR}
)

# Add the libraries to the link options
set(LINK_OPTIONS ${LINK_OPTIONS} Threads::Threads ${ZLIB_LIBRARIES} ${FLAC_LIBRARY} ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})

add_library(${PROJECT_NAME} SHARED ${CXX_SRCS} ${C_SRCS})

target_link_libraries(${PROJECT_NAME} ${LINK_OPTIONS})

message("")
message("Configuration Summary")
message("---------------------")
message("CMAKE_BUILD_TYPE:        ${CMAKE_BUILD_TYPE}")
message("CMAKE_C_FLAGS_RELEASE:   ${CMAKE_C_FLAGS_RELEASE}")
message("CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
message("LINK_OPTIONS:            ${LINK_OPTIONS}")
message("")
