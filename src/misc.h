#ifndef MISC_H
#define MISC_H

#ifndef UNUSED_ARG
#define UNUSED_ARG(x) (void)(x)
#endif

#define LOG_INFO  RETRO_LOG_INFO
#define LOG_ERROR RETRO_LOG_ERROR

#ifdef FALLBACK_LOG
    #define LOG(x, ...) fprintf(stdout, __VA_ARGS__)
#else
    #define LOG(x, ...) if (libretro.log) libretro.log(x, __VA_ARGS__)
#endif

#endif // MISC_H
