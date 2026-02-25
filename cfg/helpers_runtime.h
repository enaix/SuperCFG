//
// Created by Flynn on 23.04.2025.
//

#ifndef HELPERS_RUNTIME_H
#define HELPERS_RUNTIME_H

#include <cassert>

#include "cfg/containers.h"


// LEXICAL HELPERS
// ===============


template<class TChar, TChar start, TChar end>
constexpr void lexical_range(auto func)
{
    return cfg_helpers::do_lexical_range<0, TChar, start, end>(func);
}


/**
 * @brief Check if a char is in range [start, end]
 */
template<class TChar>
constexpr bool in_lexical_range(TChar c, TChar start, TChar end)
{
    return (start <= c) && (c <= end);
}

/**
 * @brief Check if a char is in range (start, end)
 */
template<class TChar>
constexpr bool in_lexical_range_strict(TChar c, TChar start, TChar end)
{
    return (start < c) && (c < end);
}

/**
 * @brief For a range [start..., c, ...end] lexical_intersect returns ranges [start, c), (c, end]. Does not handle cases when start or end is equal to c
 */
template<class TChar>
constexpr auto lexical_intersect(TChar c, TChar start, TChar end)
{
    return std::make_pair(std::make_pair(start, static_cast<TChar>(static_cast<std::size_t>(c) - 1)), std::make_pair(static_cast<TChar>(static_cast<std::size_t>(c) + 1), end));
}

/**
 * @brief For a range [start, end] and c=start OR c=end lexical_intersect_edge returns (start, end] OR [start, end)
 */
template<class TChar>
constexpr auto lexical_intersect_edge(TChar c, TChar start, TChar end)
{
    if (c == start)
        return std::make_pair(static_cast<TChar>(static_cast<std::size_t>(start) + 1), end);
    return std::make_pair(start, static_cast<TChar>(static_cast<std::size_t>(end) - 1));
}


/**
 * @brief Check if ranges [start_lhs, end_lhs], [start_rhs, end_rhs] intersect
 */
template<class TChar>
constexpr bool in_lexical_range(TChar a_start, TChar a_end, TChar b_start, TChar b_end)
{
    // [   ( ]   )  or  [  (   )  ]
    //return (b_start <= a_end && b_end >= a_start) || (a_start <= b_end || a_end >= b_start);
    return (a_start > b_start ? a_start : b_start) <= (a_end < b_end ? a_end : b_end);
}


/**
 * @brief Enum for lexical_ranges_intersect return type. [ ] - range A, ( ) - range B
 */
enum class RangesIntersect
{
    Partial, /**  [  ( ]  )  Note: 1st/3rd range is of type A, 3rd/1st range is of type B. Returns (A, intersect, B) */
    AInB,    /**  ( [   ] )  Note: 1st and 3rd ranges are of the same type (B). Returns as-is */
    BInA,    /**  [ (   ) ]  Note: 1st and 3rd ranges are of the same type (A). Returns as-is */
    OnlyA,   /**  [(   )  ]  Note: 1st/3rd range is empty, opposite range is of type A. Returns (A, intersect, N/A) */
    OnlyB,   /**  ([   ]  )  Note: 3rd/1st range is empty, opposite range is of type B. Returns (N/A, intersect, B) */
};


/**
 * @brief For ranges [start_lhs, end_lhs] and [start_rhs, end_rhs] lexical_ranges_intersect returns a tuple of 3 ranges: [lhs, intersect, rhs]. Any resulting range may be a singleton (c, c)
 */
template<class TChar>
constexpr auto lexical_ranges_intersect(TChar a_start, TChar a_end, TChar b_start, TChar b_end)
{
    using range_type = std::pair<TChar, TChar>;

    assert(!(a_start == b_start && a_end == b_end) && "lexical_ranges_intersect() : ranges are equal");

    // Calculate intersection boundaries
    const TChar intersect_start = a_start > b_start ? a_start : b_start; // std::max(a_start, b_start)
    const TChar intersect_end = a_end < b_end ? a_end : b_end;           // std::min(a_end,   b_end  )
    const auto i_decr = static_cast<TChar>(static_cast<std::size_t>(intersect_start) - 1);
    const auto i_incr = static_cast<TChar>(static_cast<std::size_t>(intersect_end) + 1);

    const range_type intersect = range_type(intersect_start, intersect_end);


    // [  (   )  ] or [(   )  ] (any combination)
    if ((intersect_start == a_start && intersect_end == a_end) || (intersect_start == b_start && intersect_end == b_end))
    {
        if (a_start == b_start)  // [ == (
            return (a_end < b_end ?
                // ([   ]   )
                std::make_pair(RangesIntersect::OnlyB, std::make_tuple(range_type(), intersect, range_type(i_incr, b_end))) :
                // [(   )   ]
                std::make_pair(RangesIntersect::OnlyA, std::make_tuple(range_type(i_incr, a_end), intersect, range_type())));
        else if (a_end == b_end) // ) == ]
            return (b_start < a_start ?
                // (   [   ])
                std::make_pair(RangesIntersect::OnlyB, std::make_tuple(range_type(), intersect, range_type(b_start, i_decr))) :
                // [   (   )]
                std::make_pair(RangesIntersect::OnlyA, std::make_tuple(range_type(a_start, i_decr), intersect, range_type())));
        else {
            // [  (   )  ] or (  [   ]  )
            // Note: 1st and 3rd ranges are of the same type
            return (a_start == intersect_start ?
                // (  [   ]  )
                std::make_pair(RangesIntersect::AInB, std::make_tuple(range_type(b_start, i_decr), intersect, range_type(i_incr, b_end))) :
                // [  (   )  ]
                std::make_pair(RangesIntersect::BInA, std::make_tuple(range_type(a_start, i_decr), intersect, range_type(i_incr, a_end))));
        }
    }

    return (a_start < intersect_start ?
        // [   ( ]   )
        std::make_pair(RangesIntersect::Partial, std::make_tuple(range_type(a_start, i_decr), intersect, range_type(i_incr, b_end))) :
        // (   [ )   ]
        std::make_pair(RangesIntersect::Partial, std::make_tuple(range_type(i_incr, a_end), intersect, range_type(b_start, i_decr))));
}


// VECTOR OPERATIONS
// =================


/**
 * @brief Calculate union between 2 vectors (ConstVec). It is assumed that lhs and rhs do not contain duplicates
 */
template<class T>
auto vec_union(const ConstVec<T>& lhs, const ConstVec<T>& rhs)
{
    const auto n = lhs.size() + rhs.size(); // Max size
    ConstVec<T> res(0, n);
    res.deepcopy(lhs);
    for (std::size_t i = 0; i < lhs.size(); i++)
    {
        for (std::size_t j = 0; j < rhs.size(); j++)
        {
            if (lhs[i] != rhs[i])
                res += rhs[i];
        }
    }
    return res;
}


/**
 * @brief Type morphing for vectors, lambda is applied to each element of src and the new vector is returned
 */
template<class TDest, class TSrc>
std::vector<TDest> vec_morph(const std::vector<TSrc>& src, auto func)
{
    std::vector<TDest> dest(src.size());
    for (std::size_t i = 0; i < src.size(); i++)
        dest[i] = func(src[i]);
    return dest;
}


#endif //HELPERS_RUNTIME_H
