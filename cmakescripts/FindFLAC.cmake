
find_path(FLAC_INCLUDE_DIR FLAC/all.h
  HINTS
    ENV FLAC_DIR
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
  find_library(FLAC_LIBRARY
    NAMES libFLAC.a FLAC FLAC-8 libFLAC-8
    HINTS
      ENV FLAC_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )
else (BUILD_STATIC)
  find_library(FLAC_LIBRARY
    NAMES FLAC FLAC-8 libFLAC-8
    HINTS
      ENV FLAC_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )
endif (BUILD_STATIC)

if(FLAC_LIBRARY)
  set(FLAC_VERSION 1)
endif()

# handle the QUIETLY and REQUIRED arguments and set FLAC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(FLAC  REQUIRED_VARS  FLAC_LIBRARY  FLAC_INCLUDE_DIR
                                       VERSION_VAR FLAC_VERSION )

mark_as_advanced(FLAC_INCLUDE_DIR FLAC_LIBRARY)
