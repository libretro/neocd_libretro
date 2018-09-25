#ifndef ROUND_H
#define ROUND_H 1

#include <type_traits>

template<class I, class F>
constexpr I round(const F& f)
{
    static_assert(std::is_integral<I>::value, "I must be integral type");
    static_assert(std::is_floating_point<F>::value, "F must be floating point type");

    return f >= F(0) ? I(f + F(0.5)) : I(f - F(I(f - F(1))) + F(0.5)) + I(f - F(1));
}

#endif  // ROUND_H
