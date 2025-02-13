//
// Created by Flynn on 03.02.2025.
//

#ifndef SUPERCFG_HELPERS_H
#define SUPERCFG_HELPERS_H

#include <utility>

#include "cfg/base.h"


namespace cfg_helpers
{


    template<template<class...> class Target, std::size_t... Ints>
    constexpr auto do_type_morph_t(auto morph, const std::integer_sequence<std::size_t, Ints...>)
    {
        return Target(morph.template operator()<Ints>() ...);
    }

    template<template<class...> class Target, class Src, std::size_t... Ints>
    constexpr auto do_type_morph(auto morph, const Src &src, const std::integer_sequence<std::size_t, Ints...>)
    {
        return Target(morph.template operator()<Ints>(src) ...);
    }


    template<class TElem, class... TupleElems>
    constexpr bool compare_elements(const TElem& elem, const TupleElems&... args)
    {
        return (std::is_same_v<std::remove_cvref_t<TElem>, std::remove_cvref_t<TupleElems>> || ...);
    }

    template<class TElem, class TupleRes>
    constexpr bool is_element_present(const TElem& elem, const TupleRes& res)
    {
        return std::apply([&](const auto&... args){ return compare_elements(elem, res...); }, res);
    }

    template<std::size_t i, class SrcTuple, class TupleRes>
    constexpr auto add_if_not_present(const SrcTuple& src, const TupleRes& res)
    {
        if constexpr (i < std::tuple_size_v<SrcTuple>())
        {
            const auto& elem = std::get<i>(src);
            if (is_element_present(elem, res)) return std::tuple_cat(std::make_tuple(elem), res);
            else return res;
        } else return res;
    }

    template<std::size_t i, class TupleA, class TupleB, class TupleRes>
    constexpr auto do_tuple_intersect(const TupleA& lhs, const TupleB& rhs, const TupleRes res)
    {
        add_if_not_present<i>(lhs, res);
        add_if_not_present<i>(rhs, res);

        if constexpr (i + 1 < std::tuple_size_v<TupleA>() || i + 1 < std::tuple_size_v<TupleB>())
            return do_tuple_intersect<i+1>(lhs, rhs, res);
        else return res;
    }
} // cfg_helpers


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
    return cfg_helpers::do_type_morph_t<Target>(morph, std::make_index_sequence<N>{});
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
    return cfg_helpers::do_type_morph<Target>(morph, src, std::make_index_sequence<N>{});
}

/**
 * @brief Get an intersection between 2 tuples excluding duplicated elements
 */
template<class TupleA, class TupleB>
constexpr auto tuple_intersect(const TupleA &lhs, const TupleB &rhs)
{
    return cfg_helpers::do_tuple_intersect<0>(lhs, rhs, std::make_tuple<>());
}


#endif //SUPERCFG_HELPERS_H
