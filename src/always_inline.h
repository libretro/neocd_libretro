#ifndef ALWAYS_INLINE_H
#define ALWAYS_INLINE_H

#if __has_attribute(always_inline)
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE
#endif

#endif // ALWAYS_INLINE_H
