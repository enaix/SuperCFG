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

    template<std::size_t i, class Tuples, class Tuple>
    constexpr auto do_tuple_intersect_pairwise(const Tuples& tuples, const Tuple& prev)
    {
        auto intersect = do_tuple_intersect<0>(std::get<i>(tuples), prev, std::make_tuple<>());
        if constexpr (i + 1 < std::tuple_size_v<Tuples>())
            return do_tuple_intersect<i+1>(tuples, intersect);
        else return intersect;
    }

    template<std::size_t Ints>
    constexpr auto do_tuple_for(auto gen_func, const std::integer_sequence<std::size_t, Ints...>)
    {
        return std::make_tuple(gen_func.template operator()<Ints>()...);
    }

    template<std::size_t depth, class Tuple>
    constexpr void do_tuple_each(const Tuple& tuple, auto each_elem)
    {
        each_elem(depth, std::get<depth>(tuple));
        if constexpr (depth + 1 < std::tuple_size_v<Tuple>())
            do_tuple_each<depth+1>(tuple, each_elem);
    }

    template<std::size_t offset, class Tuple, const std::integer_sequence<std::size_t, Ints...>>
    constexpr auto do_tuple_slice(const Tuple& tuple)
    {
        return std::make_tuple(std::get<Ints + offset>(tuple)...);
    }

    template<class Array, class Src, std::size_t... Ints>
    constexpr auto do_homogeneous_morph(const Src& src, const std::integer_sequence<std::size_t, Ints...>)
    {
        return Array(std::get<Ints>(src)...);
    }

    template<class T_first, class T_second, class... T>
    constexpr bool do_is_same()
    {
        return std::is_same_v<T_first, T_second> && do_is_same<T_second, T...>();
    }

    template<class T_first, class T_second>
    constexpr bool do_is_same()
    {
        return std::is_same_v<T_first, T_second>;
    }

    template<class T_first>
    constexpr bool do_is_same() { return true; }

    template<class Src, std::size_t... Ints>
    constexpr auto do_homogeneous_array(const Src& src, std::integer_sequence<std::size_t, Ints...>)
    {
        return std::array<std::tuple_element_t<0, Src>, std::tuple_size_v<Src>>(std::get<Ints>(src)...);
    }

    template<class Src, std::size_t... Ints>
    constexpr auto do_homogeneous_tuple(const Src& src, std::integer_sequence<std::size_t, Ints...>)
    {
        return std::tuple<std::tuple_element_t<Ints, Src>...>(std::get<Ints>(src)...);
    }
} // cfg_helpers


/**
 * @brief Type morphing function that constructs a type Target<T...> using morph<int> lambda. To be used with decltype()
 * @tparam Target Target class template (std::variant, std::tuple)
 * @tparam N Deduced number of target template arguments
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
 * @tparam N Deduced number of target template arguments
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
 * @brief Type morphing function for tuples
 * @param morph Morphing lambda which generates target element at index, passed through a template
 * @param src Source tuple to cast from
 */
template<class Src>
constexpr auto tuple_morph(auto morph, const Src& src)
{
    return cfg_helpers::do_type_morph<std::tuple>(morph, src, std::make_index_sequence<std::tuple_size_v<Src>()>{});
}

template<class Src>
constexpr auto tuple_morph_t(auto morph)
{
    return cfg_helpers::do_type_morph_t<std::tuple>(morph, std::make_index_sequence<std::tuple_size_v<Src>()>{});
}

/**
 * @brief Get an intersection between 2 tuples, excluding duplicated elements
 */
template<class TupleA, class TupleB>
constexpr auto tuple_intersect(const TupleA& lhs, const TupleB& rhs)
{
    return cfg_helpers::do_tuple_intersect<0>(lhs, rhs, std::make_tuple<>());
}

/**
 * @brief Get an intersection between N tuples, excluding duplicated elements
 */
template<class Tuples>
constexpr auto tuple_intersect(const Tuples& tuples)
{
    // Do pairwise
    return cfg_helpers::do_tuple_intersect_pairwise<1>(tuples, std::get<0>(tuples));
}

/**
 * @brief Generate a tuple from an integer sequence
 * @param gen_func Lambda which takes an index and returns an elenent
 * @param length Length of the integer sequence
 */
template<std::size_t N>
constexpr auto tuple_for(auto gen_func, const IntegralWrapper<N> length)
{
    return cfg_helpers::do_tuple_for(gen_func, std::make_index_sequence<N>{});
}

/**
 * @brief Iterate over each tuple element
 * @param each_elem Lambda that takes an index and tuple element
 */
template<class Tuple>
constexpr void tuple_each(const Tuple& tuple, auto each_elem)
{
    if constexpr (std::tuple_size_v<Tuple>() != 0) cfg_helpers::do_tuple_each<0>(tuple, each_elem);
}

template<class Tuple>
constexpr auto tuple_unique(const Tuple& tuple)
{
    // TODO implement tuple unique() operator
}

template<class Tuple>
constexpr auto tuple_unique()
{
    // TODO implement tuple unique() operator
}

/**
 * @brief Construct a tuple slice from another tuple
 * @tparam Start Starting index
 * @tparam End Ending index (excluded). Any value larger than tuple size sets it to the max size (including SIZE_T_MAX)
 */
template<std::size_t Start, std::size_t End, class Tuple>
constexpr auto tuple_slice(const Tuple& tuple)
{
    constexpr std::size_t max = (End <= std::tuple_size_v<Tuple>() ? End : std::tuple_size_v<Tuple>());
    return cfg_helpers::do_tuple_slice<Start>(tuple, std::make_index_sequence<max - Start>{});
}


/**
 * @brief Tuple indexer that returns tuple element at runtime
 */
template<class Tuple>
class TupleIndexer
{
public:
    static constexpr std::size_t N = std::tuple_size_v<Tuple>();
    decltype(type_morph_t<std::variant>(
            []<std::size_t index>(){ return IntegralWrapper<index>(); },
            IntegralWrapper<N>())) indexes[N];

    constexpr TupleIndexer(const Tuple& tuple) { init_array<0>(); }

    constexpr const auto& get(const Tuple& tuple, std::size_t i) const
    {
        return std::visit([&](const auto& ind){ return std::get<std::decay_t<decltype(ind)>::value_type>(tuple); }, indexes[i]);
    }

    constexpr const auto get_index(std::size_t i) const
    {
        return std::visit([&](const auto& ind){ return ind; }, indexes[i]);
    }

protected:
    template<std::size_t depth>
    constexpr void init_array()
    {
        indexes[depth] = IntegralWrapper<depth>();
        if (depth + 1 < N) init_array<depth+1>();
    }
};


/**
 * @brief Determine if a tuple contains a type Elem
 * @tparam Elem Element type to check
 * @tparam T Deduced elements of tuple
 */
template<class Elem, class... T>
struct tuple_contains<std::tuple<T...>>
{
    static constexpr bool value = (std::is_same_v<std::decay_t<Elem>, std::decay_t<T>> || ...);
    constexpr bool operator()() const noexcept { return value; }
};

template<class Elem, class Tuple>
constexpr bool tuple_contains_v = typename tuple_contains<Elem, Tuple>::value;


/**
 * @brief std::is_same implementation for multiple parameters
 * @tparam T Values to check
 */
template<class... T>
struct are_same
{
    static constexpr bool value = cfg_helpers::do_is_same<T...>();

    constexpr bool operator()() const noexcept { return value; }
};

template<class... T> constexpr bool are_same_v = typename are_same<T...>::value;


/**
 * @brief Check if a tuple is of a homogeneous type (all types are the same)
 * @tparam T Deduced tuple members
 */
template<class... T>
struct is_homogeneous<std::tuple<T...>>
{
    static constexpr bool value = are_same_v<T...>;

    constexpr bool operator()() const noexcept { return value; }
};


/**
 * @brief Check if an array is a homogeneous type, always returns true
 * @tparam T Deduced array type
 * @tparam N Deduced array length
 */
template<class T, std::size_t N>
struct is_homogeneous<std::array<T, N>>
{
    static constexpr bool value = true;

    constexpr bool operator()() const noexcept { return value; }
};

template<class Src> static constexpr bool is_homogeneous_v = is_homogeneous<Src>::value;


/**
 * @brief Morph a tuple into an array of homogeneous variant type
 * @tparam T Deduced elements of tuple src
 * @param src Tuple to morph
 */
template<class... T>
constexpr auto homogeneous_morph(const std::tuple<T...>& src)
{
    using Array = std::array<std::variant<std::decay_t<T>...>, sizeof...(T)>;
    return cfg_helpers::do_homogeneous_morph<Array>(src, std::make_index_sequence<sizeof...(T)>{});
}


/**
 * @brief Morph a homogeneous tuple into an array
 * @tparam T Deduced members of tuple src
 * @param src Homogeneous tuple to morph
 */
template<class... T>
constexpr auto to_homogeneous_array(const std::tuple<T...>& src)
{
    static_assert(is_homogeneous_v<std::tuple<T...>>, "Tuple is not homogeneous");
    return cfg_helpers::do_homogeneous_array(src, std::make_index_sequence<sizeof...(T)>{});
}

template<class T, std::size_t N>
constexpr auto to_homogeneous_array(const std::array<T, N>& src) { return src; }


/**
 * @brief Morph an array into a homogeneous tuple
 * @tparam Type Deduced homogeneous type of the array
 * @tparam N Deduced array length
 * @param src Array to morph into a homogeneous tuple of the same length
 */
template<class Type, std::size_t N>
constexpr auto to_homogeneous_tuple(const std::array<Type, N>& src)
{
    return cfg_helpers::do_homogeneous_array(src, std::make_index_sequence<N>{});
}

/**
 * @brief Morph a tuple into a homogeneous tuple, only performs the check if it's homogeneous
 * @tparam T Deduced members of tuple src
 * @param src Tuple to morph, will be returned on success
 */
template<class... T>
constexpr auto to_homogeneous_tuple(const std::tuple<T...>& src)
{
    static_assert(is_homogeneous_v<std::tuple<T...>>, "Tuple is not homogeneous");
    return src;
}


/**
 * @brief Element-wise homogeneous morph which casts an individual element of type Elem into a homogeneous variant type
 * @tparam SrcTuple A tuple containing all possible types of Elem
 * @tparam Elem Deduced type of element elem
 * @param elem Element which needs to be cast into a homogeneous type
 */
template<class SrcTuple, class Elem>
constexpr auto homogeneous_elem_morph(const Elem& elem)
{
    static_assert(tuple_contains_v<Elem, SrcTuple>, "elem is not among the types of SrcTuple"); // Check if the type Elem is among the types in SrcTuple

    using VariantType = decltype(type_morph_t<std::variant>([&]<std::size_t i>(){ return std::tuple_element_t<i, SrcTuple>(); }));
    return VariantType(elem);
}


class variadic_bool : public std::variant<std::true_type, std::false_type>
{
public:
    explicit constexpr variadic_bool(const std::true_type t) : std::variant<std::true_type, std::false_type>(t) {}

    explicit constexpr variadic_bool(const std::false_type t) : std::variant<std::true_type, std::false_type>(t) {}

    constexpr bool value(auto visitor)
    {
        return std::visit(visitor, this);
    }
};


#endif //SUPERCFG_HELPERS_H
