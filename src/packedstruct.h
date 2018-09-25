#ifndef PACKEDSTRUCT_H
#define PACKEDSTRUCT_H

#if defined(_MSC_VER)
    #define PACKED
#elif (defined(__GNUC__) || defined(__clang__))
    #define PACKED __attribute__((packed))
#else
    #error Unknown compiler!
#endif

#endif // PACKEDSTRUCT_H
