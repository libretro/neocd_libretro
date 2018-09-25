
find_path(VORBIS_INCLUDE_DIR vorbis/codec.h
  HINTS
    ENV VORBIS_DIR
  PATH_SUFFIXES include
)

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(EXTRA_PATH_SUFFIX x64)
  else()
    set(EXTRA_PATH_SUFFIX x86)
  endif()
endif()

if (BUILD_STATIC)
  find_library(VORBIS_LIBRARY
    NAMES libvorbis.a vorbis vorbis-0 libvorbis-0
    HINTS
      ENV VORBIS_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )

  find_library(VORBISFILE_LIBRARY
    NAMES libvorbisfile.a vorbisfile vorbisfile-3 libvorbisfile-3
    HINTS
      ENV VORBIS_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )
else (BUILD_STATIC)
  find_library(VORBIS_LIBRARY
    NAMES vorbis vorbis-0 libvorbis-0
    HINTS
      ENV VORBIS_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )

  find_library(VORBISFILE_LIBRARY
    NAMES vorbisfile vorbisfile-3 libvorbisfile-3
    HINTS
      ENV VORBIS_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )
endif (BUILD_STATIC)

if(VORBIS_LIBRARY)
  set(VORBIS_VERSION 1)
endif()

# handle the QUIETLY and REQUIRED arguments and set VORBIS_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VORBIS  REQUIRED_VARS  VORBIS_LIBRARY VORBISFILE_LIBRARY VORBIS_INCLUDE_DIR
                                       VERSION_VAR VORBIS_VERSION )

mark_as_advanced(VORBIS_INCLUDE_DIR VORBIS_LIBRARY VORBISFILE_LIBRARY)
