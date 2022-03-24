#ifndef __CORETYPES_H__
#define __CORETYPES_H__

#include <stdint.h>
#include <stdio.h>

#ifdef USE_LIBRETRO_VFS
#define SKIP_STDIO_REDEFINES 1
#include <streams/file_stream_transforms.h>
#endif

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(x[0]))
#endif

#if defined(__PS3__) || defined(__PSL1GHT__)
#undef UINT32
#undef UINT16
#undef UINT8
#undef INT32
#undef INT16
#undef INT8
#endif

typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t UINT8;

typedef int64_t INT64;
typedef int32_t INT32;
typedef int16_t INT16;
typedef int8_t INT8;

#ifndef USE_LIBRETRO_VFS
#define core_file FILE
#define core_fopen(file) fopen(file, "rb")
#endif

#if defined USE_LIBRETRO_VFS
	#define core_file RFILE
	#define core_fopen(file) filestream_open(file, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE)
	#define core_fseek rfseek
	#define core_ftell filestream_tell
	#define core_fread filestream_read
	#define core_fclose filestream_close
	#define core_fsize filestream_get_size
#elif defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WIN64__)
	#define core_fseek _fseeki64
	#define core_ftell _ftelli64
#elif defined(_LARGEFILE_SOURCE) && defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
	#define core_fseek fseeko64
	#define core_ftell ftello64
#elif defined(__PS3__) && !defined(__PSL1GHT__) || defined(__SWITCH__) || defined(VITA)
    #define core_fseek(x,y,z) fseek(x,(off_t)y,z)
    #define core_ftell(x) (off_t)ftell(x)
#else
	#define core_fseek fseeko
	#define core_ftell ftello
#endif

#ifndef USE_LIBRETRO_VFS
#define core_fread(fc, buff, len) fread(buff, 1, len, fc)
#define core_fclose fclose

static inline UINT64 core_fsize(core_file *f)
{
    UINT64 rv;
    UINT64 p = core_ftell(f);
    core_fseek(f, 0, SEEK_END);
    rv = core_ftell(f);
    core_fseek(f, p, SEEK_SET);
    return rv;
}
#endif

#endif
