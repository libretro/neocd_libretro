#ifndef INLINE_H
#define INLINE_H

#include <retro_inline.h>

#if defined(_WIN32) || defined(__INTEL_COMPILER)
#define STATIC_INLINE static __inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__>=199901L
#define STATIC_INLINE static inline
#elif defined(__GNUC__)
#define STATIC_INLINE static __inline__
#else
#define STATIC_INLINE
#endif

#if defined(__has_attribute)
#if __has_attribute(always_inline)
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE
#endif
#else
#define ALWAYS_INLINE
#endif

#endif // INLINE_H
