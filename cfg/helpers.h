//
// Created by Flynn on 03.02.2025.
//

#ifndef SUPERCFG_HELPERS_H
#define SUPERCFG_HELPERS_H

#include <utility>
#include <variant>
#include <tuple>

#include "cfg/common.h"


/**
 * @brief Pairwise lambda return type enum
 */
enum class PairwiseLambdaRT
{
    Singleton, /** Returns a single type, it will be wrapped into a tuple */
    ExpandResult, /** Expand the result of func into N individual elements (do not wrap into tuple) */
    CustomReturnType, /** The functions returns a pair of elements instead: the first element is treated as a result (it must be a tuple, like in ExpandResult), while the second element is a tuple of elements to concat to the result */
};

namespace cfg_helpers
{
    template<template<class...> class Target, std::size_t... Ints>
    constexpr auto do_type_morph_t(auto morph, const std::integer_sequence<std::size_t, Ints...>)
    {
        return Target<std::decay_t<decltype(morph.template operator()<Ints>())>...>(morph.template operator()<Ints>() ...);
    }

    template<template<class...> class Target, class Src, std::size_t... Ints>
    constexpr auto do_type_morph(auto morph, const Src &src, const std::integer_sequence<std::size_t, Ints...>)
    {
        return Target<std::decay_t<decltype(morph.template operator()<Ints>(src))>...>(morph.template operator()<Ints>(src) ...);
    }

    template<bool init_agg, class Target, class Src, std::size_t... Ints>
    constexpr auto do_h_type_morph(auto morph, const Src &src, const std::integer_sequence<std::size_t, Ints...>)
    {
        if constexpr (init_agg)
            return Target({morph.template operator()<Ints>(src) ...});
        else
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
        return std::apply([&](const auto&... args){ return compare_elements(elem, args...); }, res);
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
        // Calculate intersection between tuple i and the previous one (i-1)
        auto intersect = do_tuple_intersect<0>(std::get<i>(tuples), prev, std::make_tuple<>());
        if constexpr (i + 1 < std::tuple_size_v<Tuples>())
            return do_tuple_intersect_pairwise<i+1>(tuples, intersect);
        else return intersect;
    }

    template<std::size_t... Ints>
    constexpr auto do_tuple_for(auto gen_func, const std::integer_sequence<std::size_t, Ints...>)
    {
        return std::make_tuple(gen_func.template operator()<Ints>()...);
    }

    template<class T, T Step, T Start, T I, T End, T... integers>
    struct range_helper
    {

        using type = typename range_helper<T, Step, Start, I + 1, End, integers..., Start + I * Step>::type;
    };

    template<class T, T Step, T Start, T End, T... integers>
    struct range_helper<T, Step, Start, End, End, integers...>
    {
        using type = std::integer_sequence<T, integers...>;
    };

    template<std::size_t depth, class Tuple>
    constexpr void do_tuple_each(const Tuple& tuple, auto each_elem)
    {
        each_elem(depth, std::get<depth>(tuple));
        if constexpr (depth + 1 < std::tuple_size_v<Tuple>)
            do_tuple_each<depth+1>(tuple, each_elem);
    }

    template<std::size_t depth, class Tuple>
    constexpr void do_tuple_each_idx(const Tuple& tuple, auto each_elem)
    {
        each_elem.template operator()<depth>(std::get<depth>(tuple));
        if constexpr (depth + 1 < std::tuple_size_v<Tuple>)
            do_tuple_each_idx<depth+1>(tuple, each_elem);
    }

    template<std::size_t depth, class Tuple>
    constexpr bool do_tuple_each_or_return(const Tuple& tuple, auto each_elem)
    {
        if (each_elem(depth, std::get<depth>(tuple))) return true;
        if constexpr (depth + 1 < std::tuple_size_v<Tuple>)
            return do_tuple_each_or_return<depth+1>(tuple, each_elem);
        else return false;
    }

    template<std::size_t depth, class Tuple>
    constexpr bool do_tuple_each_type_or_return(auto each_elem)
    {
        if constexpr (each_elem.template operator()<depth, std::tuple_element_t<depth, std::decay_t<Tuple>>>()) return true;
        if constexpr (depth + 1 < std::tuple_size_v<Tuple>)
            return do_tuple_each_type_or_return<depth+1, Tuple>(each_elem);
        else return false;
    }

    template<std::size_t offset, class Tuple, std::size_t... Ints>
    constexpr auto do_tuple_slice(const Tuple& tuple, const std::integer_sequence<std::size_t, Ints...>)
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
        if constexpr (sizeof...(T) == 0)
            return std::is_same_v<T_first, T_second>;
        else
            return std::is_same_v<T_first, T_second> && do_is_same<T_second, T...>();
    }

    template<class T_first>
    constexpr bool do_is_same() { return true; }

    template<class... Args>
    constexpr bool do_are_same()
    {
        if constexpr (sizeof...(Args) == 0) return true;
        else return do_is_same<Args...>();
    }

    template<class Src, std::size_t... Ints>
    constexpr auto do_homogeneous_array(const Src& src, const std::integer_sequence<std::size_t, Ints...>)
    {
        return std::array<std::tuple_element_t<0, Src>, std::tuple_size_v<Src>>(std::get<Ints>(src)...);
    }

    template<class Type, std::size_t N, std::size_t... Ints>
    constexpr auto do_homogeneous_tuple(const std::array<Type, N>& src, const std::integer_sequence<std::size_t, Ints...>)
    {
        return std::tuple_cat(std::tuple<Type>(src[Ints])...);
    }

    template<class SrcTuple, std::size_t... Ints>
    constexpr auto do_type_expansion(const SrcTuple& src, auto func, const std::integer_sequence<std::size_t, Ints...>)
    {
        std::visit([&](const auto... elems){ return func(std::make_tuple(elems...)); }, std::get<Ints>(src)...);
    }

    template<class Tuple>
    struct do_variadic_morph_t;

    template<class... T>
    struct do_variadic_morph_t<std::tuple<T...>>
    {
        using value = std::variant<std::decay_t<T>...>;
    };

    template<class TupleA, class TupleB, std::size_t... IntsLHS, std::size_t... IntsRHS>
    constexpr auto do_tuple_merge_ex(const TupleA& lhs, const TupleB& rhs, const std::integer_sequence<std::size_t, IntsLHS...>, const std::integer_sequence<std::size_t, IntsRHS...>)
    {
        return std::make_tuple(std::get<IntsLHS>(lhs)..., std::get<IntsRHS>(rhs)...);
    }

    template<class TupleA, class TupleB>
    constexpr auto do_tuple_merge(const TupleA& lhs, const TupleB& rhs)
    {
        return do_tuple_merge_ex(lhs, rhs, std::make_index_sequence<std::tuple_size_v<TupleA>>{}, std::make_index_sequence<std::tuple_size_v<TupleB>>{});
    }

    template<class TupleA, class TupleB, class... Tuples>
    constexpr auto do_tuple_merge(const TupleA& lhs, const TupleB& rhs, const Tuples&... tuples)
    {
        auto ab = do_tuple_merge_ex(lhs, rhs, std::make_index_sequence<std::tuple_size_v<TupleA>>{}, std::make_index_sequence<std::tuple_size_v<TupleB>>{});
        return do_tuple_merge(ab, tuples...);
    }

    template<class Tuple>
    constexpr auto do_tuple_merge(const Tuple& lhs)
    {
        return lhs;
    }

    template<class SrcTuple, std::size_t... Ints>
    constexpr auto do_tuple_merge_sub(const SrcTuple& src, const std::integer_sequence<std::size_t, Ints...>)
    {
        return do_tuple_merge(std::get<Ints>(src)...);
    }

    //template<typename T>
    //requires (is_not_instance_of_v<std::decay_t<T>, std::tuple>)
    template<std::size_t depth, std::size_t index, class Elem, std::size_t... Ints>
    constexpr void tuple_each_tree(const Elem& elem, auto each_elem, auto each_tuple, const std::index_sequence<Ints...>)
    {
        each_elem(depth, index, elem);
    }

    template<std::size_t depth, std::size_t index, class Tuple, std::size_t... Ints>
    requires (is_instance_of_v<std::decay_t<Tuple>, std::tuple>)
    constexpr void tuple_each_tree(const Tuple& tuple, auto each_elem, auto each_tuple, const std::index_sequence<Ints...>)
    {
        each_tuple(depth, index, true);
        (tuple_each_tree<depth + 1, Ints>(std::get<Ints>(tuple), each_elem, each_tuple, std::make_index_sequence<tuple_size_or_none<std::tuple_element_t<Ints, std::decay_t<Tuple>>>>{}),...);
        each_tuple(depth, index, false);
    }

    template<std::size_t i, class Tuple, class Elem>
    constexpr bool is_type_present_in()
    {
        if constexpr (i >= std::tuple_size_v<Tuple>)
            return false;
        else
        {
            if constexpr (std::is_same_v<std::decay_t<Elem>, std::decay_t<std::tuple_element_t<i, Tuple>>>)
            {
                return true;
            } else {
                if constexpr (i + 1 < std::tuple_size_v<Tuple>)
                    return is_type_present_in<i+1, Tuple, Elem>();
            }
            return false;
        }
    }

    template<std::size_t i, class Tuple>
    constexpr auto remove_empty_tuples(const Tuple& tuple)
    {
        if constexpr (std::is_same_v<std::tuple_element_t<i, Tuple>, std::tuple<>>)
        {
            if constexpr (i + 1 < std::tuple_size_v<Tuple>)
                return remove_empty_tuples<i+1>(tuple);
            else
                return std::tuple<>();
        } else {
            if constexpr (i + 1 < std::tuple_size_v<Tuple>)
                return std::tuple_cat(std::make_tuple(std::get<i>(tuple)), remove_empty_tuples<i+1>(tuple));
            else
                return std::make_tuple(std::get<i>(tuple));
        }
        /*return std::tuple_cat([&](){
            if constexpr (std::is_same_v<std::tuple_element_t<Ints, Tuple>, std::tuple<>>)
                return std::get<Ints>(tuple);
            else
                std::make_tuple(std::get<Ints>(tuple));
        }()...);*/
    }

    template<std::size_t depth, bool expand_result, class Tuple>
    constexpr auto do_tuple_morph_each(const Tuple& tuple, auto each_elem)
    {
        if constexpr (expand_result)
        {
            if constexpr (depth + 1 < std::tuple_size_v<Tuple>)
                return std::tuple_cat(each_elem(depth, std::get<depth>(tuple)), do_tuple_morph_each<depth+1, expand_result>(tuple, each_elem));
            else
                return each_elem(depth, std::get<depth>(tuple));
        } else {
            if constexpr (depth + 1 < std::tuple_size_v<Tuple>)
                return std::tuple_cat(std::make_tuple(each_elem(depth, std::get<depth>(tuple))), do_tuple_morph_each<depth+1, expand_result>(tuple, each_elem));
            else
                return std::make_tuple(each_elem(depth, std::get<depth>(tuple))); // TODO fix this behavior
        }
    }

    template<std::size_t depth, std::size_t N, bool expand_result>
    constexpr auto do_concat_each(auto each_elem)
    {
        if constexpr (expand_result)
        {
            if constexpr (depth + 1 < N)
                return std::tuple_cat(each_elem.template operator()<depth>(), do_concat_each<depth+1, N, expand_result>(each_elem));
            else
                return each_elem.template operator()<depth>();
        } else {
            if constexpr (depth + 1 < N)
                return std::tuple_cat(std::make_tuple(each_elem.template operator()<depth>()), std::make_tuple(do_concat_each<depth+1, N, expand_result>(each_elem)));
            else
                return each_elem.template operator()<depth>();
        }
    }

    template<std::size_t i, std::size_t j, bool expand_result, class SrcTuple, class TElem>
    constexpr auto do_tuple_apply_pairwise_loop(const SrcTuple& src, const TElem& elem, auto func)
    {
        auto func_res = func.template operator()<i, j>(elem, std::get<j>(src));
        auto res = (expand_result ? func_res : (std::is_same_v<std::decay_t<decltype(func_res)>, std::tuple<>> ? func_res : std::make_tuple(func_res)));
        if constexpr (j + 1 < std::tuple_size_v<std::decay_t<SrcTuple>>)
            return std::tuple_cat(res, do_tuple_apply_pairwise_loop<i, j+1, expand_result>(src, elem, func));
        else
            return res;
    }

    template<std::size_t i, bool expand_result, class SrcTuple>
    constexpr auto do_tuple_apply_pairwise(const SrcTuple& src, auto func)
    {
        if constexpr (i + 1 >= std::tuple_size_v<std::decay_t<SrcTuple>>)
            return std::tuple<>();
        else
        {
            auto loop = do_tuple_apply_pairwise_loop<i, i+1, expand_result>(src, std::get<i>(src), func);
            if constexpr (i + 1 < std::tuple_size_v<std::decay_t<SrcTuple>>)
                return std::tuple_cat(loop, do_tuple_apply_pairwise<i+1, expand_result>(src, func));
            else
                return loop;
        }
    }


    template<std::size_t i, std::size_t j, PairwiseLambdaRT rt, std::size_t... Ints>
    constexpr auto do_tuple_apply_pairwise_loop_if(const auto& src, const auto& elem, auto func)
    {
        using SrcTuple = decltype(src);
        using TElem = decltype(elem);

        auto func_res = func.template operator()<i, j>(std::get<0>(elem), std::get<j>(src));
        auto res = [&](){
            if constexpr (rt == PairwiseLambdaRT::ExpandResult)
                return func_res;
            else if constexpr (rt == PairwiseLambdaRT::Singleton)
                return (std::is_same_v<std::decay_t<decltype(func_res)>, std::tuple<>> ? func_res : std::make_tuple(func_res));
            else
                return func_res.first;
        }();

        // Check if original return type is not empty - we need to add the index to exclude list
        if constexpr (!std::is_same_v<std::decay_t<decltype(res)>, std::tuple<>>)
        {
            // We need to set res as the new elem - the original elem is discarded
            if constexpr (j + 1 < std::tuple_size_v<std::decay_t<SrcTuple>>)
            {
                if constexpr (rt == PairwiseLambdaRT::CustomReturnType)
                {
                    auto [next, inds] = do_tuple_apply_pairwise_loop_if<i, j+1, rt, Ints..., j>(src, res, func);
                    return std::make_pair(std::tuple_cat(func_res.second, next), inds);
                }
                else
                    return do_tuple_apply_pairwise_loop_if<i, j+1, rt, Ints..., j>(src, res, func);
            }
            else // elem is replaced with res
            {
                if constexpr (rt == PairwiseLambdaRT::CustomReturnType)
                    return std::make_pair(std::tuple_cat(func_res.second, res), std::make_tuple(std::integral_constant<std::size_t, Ints>()..., std::integral_constant<std::size_t, j>()));
                else
                    return std::make_pair(res, std::make_tuple(std::integral_constant<std::size_t, Ints>()..., std::integral_constant<std::size_t, j>()));
            }

        }
        else
        {
            // Do not modify the exclude list
            if constexpr (j + 1 < std::tuple_size_v<std::decay_t<SrcTuple>>)
            {
                // Do not concat with empty tuple
                return do_tuple_apply_pairwise_loop_if<i, j+1, rt, Ints...>(src, elem, func);
            }
            else // Include the original elem. If no func() was called, it would be the original element. Otherwise it is the latest call to func
                return std::make_pair(elem, std::make_tuple(std::integral_constant<std::size_t, Ints>()...));
        }

    }

    template<std::size_t i, PairwiseLambdaRT rt, class SrcTuple, class IndsTuple>
    constexpr auto do_tuple_apply_pairwise_if(const SrcTuple& src, auto func, const IndsTuple& inds, auto each_or_return, auto tuple_expand)
    {
        if constexpr (i + 1 >= std::tuple_size_v<std::decay_t<SrcTuple>>)
            return std::make_tuple(std::get<i>(src)); // Include the original element
        else
        {
            // We need to check the exclusion list
            if constexpr (each_or_return.template operator()<IndsTuple>([&]<std::size_t k, class TElem>(){ return std::is_same_v<std::integral_constant<std::size_t, i>, std::decay_t<TElem>>; }))
            {
                // Skip this element
                if constexpr (i + 1 < std::tuple_size_v<std::decay_t<SrcTuple>>)
                    return do_tuple_apply_pairwise_if<i+1, rt>(src, func, inds, each_or_return, tuple_expand);
                else
                    return std::tuple<>();
            } else {
                auto [loop, inds_next] = tuple_expand(inds, [&]<class... T>(){ return do_tuple_apply_pairwise_loop_if<i, i+1, rt, T::value...>(src, std::make_tuple(std::get<i>(src)), func); });
                // If the loop is empty, then no apply() was performed -> leave the original element
                if constexpr (i + 1 < std::tuple_size_v<std::decay_t<SrcTuple>>)
                    return std::tuple_cat(loop, do_tuple_apply_pairwise_if<i+1, rt>(src, func, inds_next, each_or_return, tuple_expand));
                else
                    return loop;
            }
        }
    }

    template<std::size_t i, class Collapsed, class TElem>
    constexpr auto perform_collapse(const Collapsed& lhs, const TElem& elem, auto collapse)
    {
        if constexpr (i >= std::tuple_size_v<Collapsed>)
            return std::make_pair(std::tuple<>(), std::tuple<>());
        else
        {
            const auto& lhs_i = std::get<i>(lhs);
            const auto res = collapse(elem, lhs_i);

            const auto [new_elems, collapsed_next] = perform_collapse<i+1>(lhs, elem, collapse);
            // No new elements, we should include lhs[i] in collapsed
            if constexpr (std::tuple_size_v<std::decay_t<decltype(res)>> == 0)
                return std::make_pair(new_elems, std::tuple_cat(std::make_tuple(lhs_i), collapsed_next));
            else
                return std::make_pair(std::tuple_cat(res, new_elems), collapsed_next);
        }

        /*if constexpr (i + 1 < std::tuple_size_v<Collapsed>)
        {

        } else {
            if constexpr (std::tuple_size_v<std::decay_t<decltype(res)>> == 0)
                return std::make_pair(res, std::make_tuple(lhs_i));
            else
                return std::make_pair(res, std::tuple<>());
        }*/
    }

    template<class Collapsed, class ToCollapse>
    constexpr auto do_tuple_pairwise_collapse(const Collapsed& lhs, const ToCollapse& rhs, auto collapse, auto tup_slice)
    {
        if constexpr (std::tuple_size_v<std::decay_t<ToCollapse>> == 0)
            return lhs; // Finished!
        else
        {
            // We need to take one element from ToCollapse and apply the pairwise collapse() operation
            const auto& next = std::get<0>(rhs);
            const auto [new_elems, collapsed] = perform_collapse<0>(lhs, next, collapse);

            if constexpr (std::tuple_size_v<std::decay_t<decltype(new_elems)>> == 0) // No collapse was preformed -> add next to collapsed
                return do_tuple_pairwise_collapse(std::tuple_cat(collapsed, std::make_tuple(next)), tup_slice.template operator()<1, std::tuple_size_v<ToCollapse>>(rhs), collapse, tup_slice);
            else
                return do_tuple_pairwise_collapse(collapsed, std::tuple_cat(new_elems, tup_slice.template operator()<1, std::tuple_size_v<ToCollapse>>(rhs)), collapse, tup_slice);
        }
    }

    template<std::size_t i, class SrcTuple, class Value>
    constexpr auto do_tuple_pairwise(const SrcTuple& src, const Value& value, auto collapse)
    {
        if constexpr (i >= std::tuple_size_v<std::decay_t<SrcTuple>>)
            return value;
        else {
            return do_tuple_pairwise<i+1>(src, collapse.template operator()<i>(value, std::get<i>(src)), collapse);
        }
    }

    template<std::size_t depth, class SrcTuple, class Elem>
    constexpr std::size_t do_tuple_index_of()
    {
        if constexpr (std::is_same_v<std::decay_t<std::tuple_element_t<depth, SrcTuple>>, Elem>)
            return depth;
        else {
            if constexpr (depth + 1 < std::tuple_size_v<SrcTuple>)
                return do_tuple_index_of<depth+1, SrcTuple, Elem>();
            else
                return std::numeric_limits<std::size_t>::max();
        }
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
    // TODO pass tuple element in morph
    return cfg_helpers::do_type_morph<std::tuple>(morph, src, std::make_index_sequence<std::tuple_size_v<Src>>{});
}

template<class Src>
constexpr auto tuple_morph_t(auto morph)
{
    return cfg_helpers::do_type_morph_t<std::tuple>(morph, std::make_index_sequence<std::tuple_size_v<Src>>{});
}

/**
 * @brief Type morph for homogeneous target type. Accepts a non-template target type
 * @tparam init_agg Initialize target with aggregate types (in {...})
 */
template<class Target, bool init_agg, std::size_t N, class Src>
constexpr auto h_type_morph(auto morph, const IntegralWrapper<N> length, const Src& src)
{
    return cfg_helpers::do_h_type_morph<init_agg, Target>(morph, src, std::make_index_sequence<N>{});
}


/**
 * @brief Morph a tuple of type std::tuple<T...> into a variant of type std::variant<T...>
 */

template<class SrcTuple>
using variadic_morph_t = typename cfg_helpers::do_variadic_morph_t<SrcTuple>::value;

/**
 * @brief Get an intersection between 2 tuples, excluding duplicated elements
 */
/*template<class TupleA, class TupleB>
constexpr auto tuple_intersect(const TupleA& lhs, const TupleB& rhs)
{
    // Not implemented
    return cfg_helpers::do_tuple_intersect<0>(lhs, rhs, std::make_tuple<>());
}*/

/**
 * @brief Get an intersection between N tuples, excluding duplicated elements
 */
template<class Tuples>
constexpr auto tuple_intersect(const Tuples& tuples)
{
    // Do pairwise
    if constexpr (std::tuple_size_v<Tuples> < 2) return tuples;
    else return cfg_helpers::do_tuple_intersect_pairwise<1>(tuples, std::get<0>(tuples));
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
    if constexpr (std::tuple_size_v<Tuple> != 0) cfg_helpers::do_tuple_each<0>(tuple, each_elem);
}


/**
 * @brief Iterate over each tuple element and pass index as a template argument
 * @param each_elem Lambda that takes an index and tuple element
 */
template<class Tuple>
constexpr void tuple_each_idx(const Tuple& tuple, auto each_elem)
{
    if constexpr (std::tuple_size_v<Tuple> != 0) cfg_helpers::do_tuple_each_idx<0>(tuple, each_elem);
}


/**
 * @brief Iterate over each tuple element and concatenate the result into a new tuple
 * @tparam expand_result Expand the result of func into N individual elements
 * @param each_elem Lambda that takes an index and the tuple element and returns a new element
 */
template<bool expand_result = false, class Tuple>
constexpr auto tuple_morph_each(const Tuple& tuple, auto each_elem)
{
    if constexpr (std::tuple_size_v<Tuple> != 0)
        return cfg_helpers::do_tuple_morph_each<0, expand_result>(tuple, each_elem);
    else return std::tuple<>();
}


/**
 * @brief Similar to tuple_morph_each, but only concats the results of f<i>()
 * @tparam N Number of times to call f
 * @tparam expand_result Expand the result of func into N individual elements
 * @param each_elem Lambda that takes an index as a template parameter and returns a new element
 */
template<std::size_t N, bool expand_result = false>
constexpr auto concat_each(auto each_elem)
{
    if constexpr (N != 0)
        return cfg_helpers::do_concat_each<0, N, expand_result>(each_elem);
    else return std::tuple<>();
}


/**
 * @brief Iterate over each tuple element and return if the tuple returns true
 * @param each_elem Lambda that takes an index and tuple element and returns true to return
 */
template<class Tuple>
constexpr bool tuple_each_or_return(const Tuple& tuple, auto each_elem)
{
    if constexpr (std::tuple_size_v<Tuple> != 0) return cfg_helpers::do_tuple_each_or_return<0>(tuple, each_elem);
    else return false;
}


/**
 * @brief Constexpr version of tuple_each_or_return which passes only a type
 * @param each_elem Lambda that takes an index and tuple element and returns true to return
 */
template<class Tuple>
constexpr bool tuple_each_type_or_return(auto each_elem)
{
    if constexpr (std::tuple_size_v<Tuple> != 0) return cfg_helpers::do_tuple_each_type_or_return<0, Tuple>(each_elem);
    else return false;
}


/**
 * @brief Descend over all elements in a tuple
 */
template<class Tuple>
constexpr void tuple_each_tree(const Tuple& tuple, auto each_elem, auto each_tuple)
{
    cfg_helpers::tuple_each_tree<0, 0>(tuple, each_elem, each_tuple, std::make_index_sequence<tuple_size_or_none<std::decay_t<Tuple>>>{});
}


template<class Tuple>
constexpr auto tuple_unique(const Tuple& tuple)
{
    if constexpr (std::tuple_size_v<Tuple> == 0)
        return tuple;
    else
    {
        return cfg_helpers::remove_empty_tuples<0>(tuple_morph([&]<std::size_t i>(const auto& src){
            if constexpr (!cfg_helpers::is_type_present_in<i+1, Tuple, std::tuple_element_t<i, Tuple>>())
                return std::get<i>(src);
            else
                return std::tuple<>();
        }, tuple));
    }
}

/**
 * @brief Concatenate N tuples
 * @tparam Tuples Deduced tuples type
 * @param tuples Tuples to merge
 */
template<class... Tuples>
constexpr auto tuple_concat(const Tuples&... tuples)
{
    return cfg_helpers::do_tuple_merge(tuples...);
}

/**
 * @brief Flatten a tuple one layer deep
 * @tparam SrcTuple Deduced tuple type
 * @param src Tuple to flatten
 */
template<class SrcTuple>
constexpr auto tuple_flatten_layer(const SrcTuple& src)
{
    return cfg_helpers::do_tuple_merge_sub(src, std::make_index_sequence<std::tuple_size_v<SrcTuple>>{});
}

/**
 * @brief Construct a tuple slice from another tuple
 * @tparam Start Starting index
 * @tparam End Ending index (excluded). Any value larger than tuple size sets it to the max size (including SIZE_T_MAX)
 */
template<std::size_t Start, std::size_t End, class Tuple>
constexpr auto tuple_slice(const Tuple& tuple)
{
    constexpr std::size_t max = (End <= std::tuple_size_v<Tuple> ? End : std::tuple_size_v<Tuple>);
    if constexpr (Start == max)
        return std::tuple<>();
    else
        return cfg_helpers::do_tuple_slice<Start>(tuple, std::make_index_sequence<max - Start>{});
}


template<class Elem, class Tuple>
struct tuple_contains;

/**
 * @brief Determine if a tuple contains a type Elem
 * @tparam Elem Element type to check
 * @tparam T Deduced elements of tuple
 */
template<class Elem, class... T>
struct tuple_contains<Elem, std::tuple<T...>>
{
    static constexpr bool value = (std::is_same_v<std::decay_t<Elem>, std::decay_t<T>> || ...);
    constexpr bool operator()() const noexcept { return value; }
};

// Overload for null tuple
template<class Elem>
struct tuple_contains<Elem, std::tuple<>>
{
    static constexpr bool value = false;
    constexpr bool operator()() const noexcept { return value; }
};

template<class Elem, class Tuple>
constexpr bool tuple_contains_v = tuple_contains<Elem, Tuple>::value;


/**
 * @brief std::is_same implementation for multiple parameters
 * @tparam T Values to check
 */
template<class... T>
struct are_same
{
    static constexpr bool value = cfg_helpers::do_are_same<T...>();

    constexpr bool operator()() const noexcept { return value; }
};

template<class... T> constexpr bool are_same_v = are_same<T...>::value;


template<class Tuple>
struct is_homogeneous;

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
    return cfg_helpers::do_homogeneous_tuple(src, std::make_index_sequence<N>{});
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

    // using VariantType = decltype(type_morph_t<std::variant>([&]<std::size_t i>(){ return std::tuple_element_t<i, SrcTuple>(); }, IntegralWrapper<std::tuple_size_v<SrcTuple>>()));
    return variadic_morph_t<SrcTuple>(elem);
}


/**
 * @brief Expand a homogeneous tuple into a nonhomogeneous tuple
 * @tparam SrcTuple Deduced type of tuple src
 * @param src Homogeneous tuple of std::variant elements
 * @param func Visitor lambda which accepts nonhomogeneous tuple. May return a homogeneous type
 */
template<class SrcTuple>
constexpr auto type_expansion(const SrcTuple& src, auto func)
{
    static_assert(is_homogeneous_v<SrcTuple>, "Tuple is not homogeneous");
    return cfg_helpers::do_type_expansion(src, func, std::make_index_sequence<std::tuple_size_v<SrcTuple>>{});
}


/**
 * @brief Expand a homogeneous array into a nonhomogeneous tuple
 * @tparam Type Deduced homogeneous type
 * @tparam N Deduced array length
 * @param src Array of std::variant elements
 * @param func Visitor lambda which accepts nonhomogeneous tuple. May return a homogeneous type
 */
template<class Type, std::size_t N>
constexpr auto type_expansion(const std::array<Type, N>& src, auto func)
{
    auto tuple = to_homogeneous_tuple(src);
    return cfg_helpers::do_type_expansion(tuple, func, std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{});
}


/**
 * @brief Get a nonhomogeneous element at position i
 * @tparam Type Deduced array element type
 * @tparam N Deduced array length
 * @param src Homogeneous array to access
 * @param i Element index
 * @param func Lambda which takes a nonhomogeneous type. May optionally return a homogeneous type
 */
template<class Type, std::size_t N>
constexpr auto homogeneous_at(const std::array<Type, N>& src, std::size_t i, auto func)
{
    return std::visit(func, src.at(i));
}


/**
 * @brief Get a nonhomogeneous element of a homogeneous tuple at position i. Less efficient than homogeneous array access
 * @tparam SrcTuple Deduced tuple type
 * @param src Homogeneous tuple to access
 * @param i Element index
 * @param func Lambda which takes a nonhomogeneous type. May optionally return a homogeneous type
 */
template<class SrcTuple>
constexpr auto homogeneous_at(const SrcTuple& src, std::size_t i, auto func)
{
    auto array = to_homogeneous_array(src);
    return homogeneous_at(array, i, func);
}


/**
 * @brief Alias for the variant of type true_type/false_type
 */
class variadic_bool : public std::variant<std::true_type, std::false_type>
{
public:
    explicit constexpr variadic_bool(const std::true_type t) : std::variant<std::true_type, std::false_type>(t) {}

    explicit constexpr variadic_bool(const std::false_type t) : std::variant<std::true_type, std::false_type>(t) {}

    constexpr auto value(auto visitor)
    {
        return std::visit([&]<class T>(const T value){
            return visitor.template operator()<T::value>();
        }, this);
    }
};


/**
 * @brief Runtime std::get for tuples, implemented using homogeneous morph
 * @tparam SrcTuple Deduced tuple type
 * @param src Input tuple
 * @param i Tuple element index
 * @param func Lambda which accepts the tuple element. May optionally return a homogeneous type
 */
template<class SrcTuple>
constexpr auto tuple_at(const SrcTuple& src, const std::size_t i, auto func)
{
    auto array = homogeneous_morph(src);
    return homogeneous_at(array, i, func);
}


/**
 * @brief Take each j-th element in i-th sub-tuple of a tuple
 * @tparam SrcTuple Deduced tuple type
 * @tparam Index Which element to take along axis
 * @param src Tuple to morph
 */
template<std::size_t Index, class SrcTuple>
constexpr auto tuple_take_along_axis(const SrcTuple& src)
{
    return tuple_morph([&]<std::size_t i>(const auto& container){ return std::get<Index>(std::get<i>(container)); }, src);
}


/**
 * @brief Merge two equal tuples of the same size element-wise
 * @tparam TupleA Deduced lhs tuple type
 * @tparam TupleB Deduced rhs tuple type
 * @param lhs First tuple to merge
 * @param rhs Second tuple to merge
 */
template<class TupleA, class TupleB>
constexpr auto tuple_merge_along_axis(const TupleA& lhs, const TupleB& rhs)
{
    static_assert(std::tuple_size_v<TupleA> == std::tuple_size_v<TupleB>, "Tuples must be equal");
    return tuple_morph([&]<std::size_t i>(const auto& container){ return std::make_tuple(lhs, rhs); }, lhs);
}


/**
 * @brief Expand a tuple into T types and call lambda with T template arguments
 * @param src Deduced tuple of T types
 * @param func Lambda to call
 */
template<class... T>
constexpr auto tuple_expand_t(const std::tuple<T...>& src, auto func)
{
    return func.template operator()<std::decay_t<T>...>();
}


/**
 * @brief Apply a pairwise operation on a tuple
 * @tparam expand_result Expand the result of func into N individual elements
 * @param src Source tuple
 * @param func Lambda which takes i-th and j-th index in a template and two elements as arguments. It returns either a new element or tuple<>
 */
template<bool expand_result, class SrcTuple>
constexpr auto tuple_apply_pairwise(const SrcTuple& src, auto func)
{
    return cfg_helpers::do_tuple_apply_pairwise<0, expand_result>(src, func);
}


/**
 * @brief Conditional tuple_apply_pairwise operator which replaces two elements with the function result
 * @tparam rt Return type enum
 * @param src Source tuple
 * @param func Lambda which takes i-th and j-th index in a template and two elements as arguments. It returns either a new element or tuple<>. An empty tuple is returned if no elements should be altered
 */
template<PairwiseLambdaRT rt, class SrcTuple>
constexpr auto tuple_apply_pairwise_if(const SrcTuple& src, auto func)
{
    return cfg_helpers::do_tuple_apply_pairwise_if<0, rt>(src, func, std::tuple<>(), []<class... T>(const auto&... args){ return tuple_each_type_or_return<T...>(args...); }, [](const auto&... args){ return tuple_expand_t(args...); });
}


/**
 * @brief Pairwise collapse algorithm takes an element from the stack and collapses it with the remainder. If collapse(elem, rem[i]) returns new elements, they are added to the stack and rem[i] is deleted. If the remainder is collapsed (no operation was performed with elem), elem is added to the remainder
 * @param src Tuple to collapse
 * @param collapse Lambda which takes an element from the stack and a reduced element. It returns a tuple of new elements which are added to the remainder. If it returns an empty tuple, then no operation is performed.
 */
template<class SrcTuple>
constexpr auto tuple_pairwise_collapse(const SrcTuple& src, auto collapse)
{
    return cfg_helpers::do_tuple_pairwise_collapse(std::tuple<>(), src, collapse, []<std::size_t Start, std::size_t End>(const auto& tuple){ return tuple_slice<Start, End>(tuple); });
}


/**
 * @brief Apply pairwise operation to a tuple
 * @param src Tuple to collapse
 * @param value Starting value which is passed to the lambda
 * @param collapse Lambda which takes the previous call result, the element index as a template and the element itself
 */
template<class SrcTuple, class Value>
constexpr auto tuple_pairwise(const SrcTuple& src, const Value& value, auto collapse)
{
    return cfg_helpers::do_tuple_pairwise<0>(src, value, collapse);
}


/**
 * @brief Get index of an element in a tuple. Element should have a unique type
 * @param src Tuple where to search
 * @param elem Target element
 */
template<class SrcTuple, class Elem>
constexpr std::size_t tuple_index_of()
{
    return cfg_helpers::do_tuple_index_of<0, std::decay_t<SrcTuple>, std::decay_t<Elem>>();
}


#endif //SUPERCFG_HELPERS_H
