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


// https://indii.org/blog/is-type-instantiation-of-template/
template<class T, template<class...> class U>
inline constexpr bool is_instance_of_v = std::false_type{};

template<template<class...> class U, class... Vs>
inline constexpr bool is_instance_of_v<U<Vs...>,U> = std::true_type{};


template<class T, template<class...> class U>
inline constexpr bool is_not_instance_of_v = std::true_type{};

template<template<class...> class U, class... Vs>
inline constexpr bool is_not_instance_of_v<U<Vs...>,U> = std::false_type{};


template<class T>
inline constexpr std::size_t tuple_size_or_none = 0;

template<template<class...> class U, class... Vs>
inline constexpr std::size_t tuple_size_or_none<U<Vs...>> = std::tuple_size_v<std::tuple<Vs...>>;


#endif //COMMON_H
