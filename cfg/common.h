//
// Created by Flynn on 20.03.2025.
//

#ifndef COMMON_H
#define COMMON_H

#include <cstddef>

/**
 * integral_constant<size_t, ...> wrapper which converts N to ConstStr
 * @tparam N
 */
template<std::size_t N>
class IntegralWrapper : public std::integral_constant<size_t, N>
{
public:
    using value_type = typename std::integral_constant<size_t, N>::value_type;

    template<class... TArgs>
    constexpr auto bake(auto... args) const { return int2str(*this); }
};

#endif //COMMON_H
