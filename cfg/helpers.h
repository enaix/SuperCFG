//
// Created by Flynn on 03.02.2025.
//

#ifndef SUPERCFG_HELPERS_H
#define SUPERCFG_HELPERS_H

#include <utility>

#include "cfg/base.h"


template<template<class...> class Target, std::size_t... Ints>
constexpr auto do_type_morph_t(auto morph, const std::integer_sequence<std::size_t, Ints...>)
{
    return Target(morph.template operator()<Ints>() ...);
}

template<template<class...> class Target, class Src, std::size_t... Ints>
constexpr auto do_type_morph(auto morph, const Src& src, const std::integer_sequence<std::size_t, Ints...>)
{
    return Target(morph.template operator()<Ints>(src) ...);
}


 /**
  * @brief Type morphing function that constructs a type Target<T...> using morph<int> lambda. To be used with decltype()
  * @tparam Target Target class template (std::variant, std::tuple)
  * @tparam N Number of target template argument
  * @param morph Morphing lambda which generates target element at index, passed through a template
  * @param length Number of template arguments
  */
template<template<class...> class Target, std::size_t N>
constexpr auto type_morph_t(auto morph, const IntegralWrapper<N> length)
{
    return do_type_morph_t<Target>(morph, std::make_index_sequence<N>{});
}

 /**
  * @brief Type morphing function that constructs an object of type Target<T...> using morph<int, src> lambda. To be used in a constructor
  * @tparam Target Class target template (std::variant, std::tuple)
  * @tparam N Number of target template argument
  * @tparam Src Deduced source class to cast from
  * @param morph Morphing lambda which generates target element at index, passed through a template
  * @param length Number of template arguments
  * @param src Const reference to source class
  */
template<template<class...> class Target, std::size_t N, class Src>
constexpr auto type_morph(auto morph, const IntegralWrapper<N> length, const Src& src)
{
    return do_type_morph<Target>(morph, src, std::make_index_sequence<N>{});
}


#endif //SUPERCFG_HELPERS_H
