#ifndef ENDIAN_H
#define ENDIAN_H

#ifdef __cplusplus
    #include <cstdint>
#else
    #include <stdint.h>
#endif

#if defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC)
    #ifndef BIG_ENDIAN
        #define BIG_ENDIAN
    #endif
#else
    #ifndef LITTLE_ENDIAN
        #define LITTLE_ENDIAN
    #endif
#endif

#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if (defined(__clang__) && __has_builtin(__builtin_bswap32) && __has_builtin(__builtin_bswap16)) \
    || (defined(__GNUC__ ) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)))
    #define BYTE_SWAP_16(x) __builtin_bswap16(x)
    #define BYTE_SWAP_32(x) __builtin_bswap32(x)
#elif defined(_MSC_VER)
    #ifdef __cplusplus
        #include <cstdlib>
    #else
        #include <stdlib.h>
    #endif
    #define BYTE_SWAP_16(x) _byteswap_ushort(x)
    #define BYTE_SWAP_32(x) _byteswap_ulong(x)
#else
    inline uint16_t BYTE_SWAP_16(uint16_t x)
    {
        return static_cast<uint16_t>((x << 8) | (x >> 8));
    }

    inline uint32_t BYTE_SWAP_32(uint32_t x)
    {
        return static_cast<uint32_t>((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24));
    }
#endif

#ifdef BIG_ENDIAN
    #define BIG_ENDIAN_WORD(x) (x)
    #define BIG_ENDIAN_DWORD(x) (x)
    #define LITTLE_ENDIAN_WORD(x) BYTE_SWAP_16(x)
    #define LITTLE_ENDIAN_DWORD(x) BYTE_SWAP_32(x)
#else // Little endian machine
    #define BIG_ENDIAN_WORD(x) BYTE_SWAP_16(x)
    #define BIG_ENDIAN_DWORD(x) BYTE_SWAP_32(x)
    #define LITTLE_ENDIAN_WORD(x) (x)
    #define LITTLE_ENDIAN_DWORD(x) (x)
#endif // LITTLE_ENDIAN

#endif // ENDIAN_H
