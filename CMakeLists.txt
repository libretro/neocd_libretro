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

# Set default locations
set(CL_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/cmake-build-${CMAKE_BUILD_TYPE}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CL_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CL_OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CL_OUTPUT_DIRECTORY})

# Check if the compiler can handle C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the current directory to includes
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

################################################################
#                       Compiler options                       #
# Uncomment one line suitable for your system or make your own #
################################################################

set(BASE_CXX_OPTIONS "-std=c++11 -Ofast -fomit-frame-pointer -fno-exceptions -fno-rtti")

# Generic x64 options
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=x86-64 -mtune=generic ${BASE_CXX_OPTIONS}")

# Generic x64 options, generate profile guided optimization files
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=x86-64 -mtune=generic ${BASE_CXX_OPTIONS} -fprofile-generate=${CL_OUTPUT_DIRECTORY}")

# Generic x64 options, use profile guided optimization files
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=x86-64 -mtune=generic ${BASE_CXX_OPTIONS} -fprofile-use=${CL_OUTPUT_DIRECTORY}")

# All architectures, optimized for the machine NeoCD is compiled on
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native ${BASE_CXX_OPTIONS}") 

# Playstation Classic, arm
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=arm ${BASE_CXX_OPTIONS}")

# Linker options
if (CMAKE_BUILD_TYPE MATCHES Release)
    set(LINK_OPTIONS -s)
endif()

# Library path
#set(CMAKE_LDFLAGS "${CMAKE_LDFLAGS} -L. ")

# Define the CXX sources
set ( CXX_SRCS
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kopdm.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/z80/z80daisy.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kopac.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kcpu.cpp
    ${CMAKE_SOURCE_DIR}/src/video.cpp
    ${CMAKE_SOURCE_DIR}/src/timergroup.cpp
    ${CMAKE_SOURCE_DIR}/src/timeprofiler.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kopnz.cpp
    ${CMAKE_SOURCE_DIR}/src/oggfile.cpp
    ${CMAKE_SOURCE_DIR}/src/neogeocd.cpp
    ${CMAKE_SOURCE_DIR}/src/z80intf.cpp
    ${CMAKE_SOURCE_DIR}/src/wavfile.cpp
    ${CMAKE_SOURCE_DIR}/src/flacfile.cpp
    ${CMAKE_SOURCE_DIR}/src/lc8951.cpp
    ${CMAKE_SOURCE_DIR}/src/timer.cpp
    ${CMAKE_SOURCE_DIR}/src/m68kintf.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_mapped.cpp
    ${CMAKE_SOURCE_DIR}/src/input.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_switches.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/musashi/m68kops.cpp
    ${CMAKE_SOURCE_DIR}/src/cdrom.cpp
    ${CMAKE_SOURCE_DIR}/src/datapacker.cpp
    ${CMAKE_SOURCE_DIR}/src/libretro.cpp
    ${CMAKE_SOURCE_DIR}/src/audio.cpp
    ${CMAKE_SOURCE_DIR}/src/memory.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/ym/ym2610.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_backupram.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_cdintf.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_z80comm.cpp
    ${CMAKE_SOURCE_DIR}/src/3rdparty/z80/z80.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_video.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_input.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_paletteram.cpp
)

# Find the required libraries
FIND_PACKAGE(FLAC REQUIRED)
FIND_PACKAGE(Ogg REQUIRED)
FIND_PACKAGE(Vorbis REQUIRED)
FIND_PACKAGE(Threads REQUIRED)

INCLUDE_DIRECTORIES(
    ${FLAC_INCLUDE_DIR}
    ${OGG_INCLUDE_DIR}
    ${VORBIS_INCLUDE_DIR}
)

# Add the libraries to the link options
set(LINK_OPTIONS ${LINK_OPTIONS} Threads::Threads ${FLAC_LIBRARY} ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})

add_library(${PROJECT_NAME} SHARED ${CXX_SRCS} ${C_SRCS})

target_link_libraries(${PROJECT_NAME} ${LINK_OPTIONS})

message("")
message("Configuration Summary")
message("---------------------")
message("CMAKE_BUILD_TYPE:        ${CMAKE_BUILD_TYPE}")
message("CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
message("LINK_OPTIONS:            ${LINK_OPTIONS}")
message("")
