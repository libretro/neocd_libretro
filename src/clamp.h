#ifndef CLAMP_H
#define CLAMP_H

#include <algorithm>

// If clamp is not available in the standard library, provide a reasonable implementation
#ifndef __cpp_lib_clamp
    namespace std
    {
        template <typename T>
        constexpr const T& clamp(const T& val, const T& min, const T& max)
        {
            return std::max(min, std::min(max, val));
        }   
    }
#endif

#endif // CLAMP_H
