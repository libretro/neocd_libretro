
find_path(OGG_INCLUDE_DIR ogg/ogg.h
  HINTS
    ENV OGG_DIR
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
  find_library(OGG_LIBRARY
    NAMES libogg.a ogg ogg-0 libogg-0
    HINTS
      ENV OGG_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )
else (BUILD_STATIC)
  find_library(OGG_LIBRARY
    NAMES ogg ogg-0 libogg-0
    HINTS
      ENV OGG_DIR
    PATH_SUFFIXES lib ${EXTRA_PATH_SUFFIX}
  )
endif (BUILD_STATIC)

if(OGG_LIBRARY)
  set(OGG_VERSION 1)
endif()

# handle the QUIETLY and REQUIRED arguments and set OGG_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OGG  REQUIRED_VARS  OGG_LIBRARY  OGG_INCLUDE_DIR
                                       VERSION_VAR OGG_VERSION )

mark_as_advanced(OGG_INCLUDE_DIR OGG_LIBRARY)
