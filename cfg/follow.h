//
// Created by Flynn on 07.04.2025.
//

#ifndef FOLLOW_H
#define FOLLOW_H

#include "cfg/helpers.h"
#include "cfg/preprocess.h"
#include "cfg/preprocess_factories.h"
#include "cfg/base.h"

namespace cfg_helpers
{
    /**
     * @brief Iteration strategy enum
     */
    enum class IterStrategy
    {
        Normal,
        First,
        Repeat,
        PermuteAll,
        RecurseFirst
    };

    enum class ReturnStrategy
    {
        Sequential,
        Optional
    };

    enum class LookaheadDirection
    {
        Forward,
        Reverse
    };

    struct FollowSetFactory
    {
        template<class Target, class TSymbol>
        static constexpr bool peek_into()
        {
            if constexpr (is_operator<TSymbol>())
                return peek_into<Target, TSymbol>(std::make_index_sequence<std::tuple_size_v<typename TSymbol::term_types_tuple>>{});
            else
                return std::is_same_v<std::decay_t<Target>, std::decay_t<TSymbol>>;
        }

        template<class Target, class TSymbol, std::size_t... Ints>
        static constexpr bool peek_into(const std::index_sequence<Ints...>)
        {
            return (peek_into<Target, std::tuple_element_t<Ints, typename TSymbol::term_types_tuple>>() || ...);
        }

        /**
         * @brief Iterate over symbols in an operator using the provided strategy
         * @tparam dir Lookahead dir
         * @tparam symbol Current symbol position (reverse of index)
         * @tparam strategy Iteration strategy enum
         * @tparam ret_strategy Return strategy enum
         * @tparam is_target True if we have encountered our target
         * @param target Symbol to check
         * @param def Operator with the symbols we need to check
         */
        template<LookaheadDirection dir, std::size_t symbol, IterStrategy strategy, ReturnStrategy ret_strategy, bool is_target, class Target, class TSymbol>
        constexpr auto follow_symbol(const Target& target, const TSymbol& def) const
        {
            // Get actual index from symbol pos
            constexpr std::size_t i = (dir == LookaheadDirection::Forward ? symbol : std::tuple_size_v<decltype(def.terms)> - symbol - 1);
            constexpr std::size_t last = (dir == LookaheadDirection::Forward ? std::tuple_size_v<decltype(def.terms)> - 1 : 0);
            using symbol_type = std::tuple_element_t<i, decltype(def.terms)>;
            constexpr bool target_found = std::is_same_v<symbol_type, std::decay_t<Target>>;

            // We need to recursively process the operator
            if constexpr (is_operator<symbol_type>())
            {
                // is_target should be handled sequentially
                /*if constexpr (strategy == IterStrategy::Normal)
                {
                    if constexpr (i != last)
                    {
                        // We need to check if target exists in the current symbol
                        const auto[found, cur_tuple] = you_must_follow<dir, is_target>(target, std::get<i>(def.terms));
                        // Recurse into the next
                        const auto[found_in_next, tail] = follow_symbol<dir, symbol+1, strategy, ret_strategy, decltype(found)::value>(target, def);

                        // Return the original is_target value (is the value present outside?)
                        if constexpr (symbol == 0 && ret_strategy == ReturnStrategy::Optional)
                            return std::make_pair(std::integral_constant<bool, (is_target || decltype(found_in_next)::value)>(), std::tuple_cat(cur_tuple, tail));
                        else
                            // Return found only from the last element (sequential)
                            return std::make_pair(found_in_next, std::tuple_cat(cur_tuple, tail));
                    }
                    else
                    {
                        return you_must_follow<dir, is_target>(target, std::get<i>(def.terms));
                    }
                }*/
                if constexpr (strategy == IterStrategy::Repeat)
                {
                    constexpr bool has_target = peek_into<Target, TSymbol>();
                    // Check if the element is present in alternation symbol using PermuteAll strategy
                    //const auto[found_repeat, res_repeat] = follow_symbol<dir, symbol, IterStrategy::PermuteAll, ret_strategy, true>(target, def);
                    // Default check
                    const auto[found_once, res] = follow_symbol<dir, symbol, IterStrategy::PermuteAll, ret_strategy, has_target>(target, def);
                    if constexpr (has_target)
                    {
                        // Alternation contains target, we add the target to follow set
                        const auto res_total = std::tuple_cat(res, std::make_tuple(target));
                        return std::make_pair(found_once, res_total);
                    } else {
                        return std::make_pair(found_once, res);
                    }
                }

                // is_target should remain unchanged for all calls, return found if at least one match exists
                else if constexpr (strategy == IterStrategy::Normal || strategy == IterStrategy::PermuteAll)
                {
                    if constexpr (i != last)
                    {
                        // Check if we need to pass target_found
                        constexpr bool found_next = (strategy == IterStrategy::PermuteAll ? is_target : target_found);
                        // Check the first element
                        const auto [found, cur_tuple] = you_must_follow<dir, is_target>(target, std::get<i>(def.terms));
                        // Recursively process symbols 1-end in def
                        const auto [found_in_next, tail] = follow_symbol<dir, symbol+1, strategy, ret_strategy, found_next>(target, def);
                        // Return found OR found_in_next
                        constexpr auto found_optional = std::integral_constant<bool, (is_target || decltype(found_in_next)::value)>();

                        if constexpr (symbol == 0 && ret_strategy == ReturnStrategy::Optional)
                            return std::make_pair(found_optional, std::tuple_cat(cur_tuple, tail));
                        else
                            return std::make_pair(found_in_next, std::tuple_cat(cur_tuple, tail));
                    }
                    else
                    {
                        return you_must_follow<dir, is_target>(target, std::get<i>(def.terms));
                    }
                }

                // Include only the first symbol
                else if constexpr (strategy == IterStrategy::First)
                    return you_must_follow<dir, is_target>(target, std::get<0>(def.terms));

                // RecurseFirst: pass the first symbol into you_must_follow
                else
                {
                    return you_must_follow<dir, is_target>(target, std::get<0>(def.terms));
                }
            }
            // Term or a NTerm
            else
            {
                // Include only one symbol after is_target
                if constexpr (strategy == IterStrategy::Normal || strategy == IterStrategy::PermuteAll)
                {
                    // Check if we need to pass target_found
                    constexpr bool found_next = (strategy == IterStrategy::PermuteAll ? is_target : target_found);
                    if constexpr (strategy == IterStrategy::PermuteAll)
                        std::cout << "PermuteAll: is_tgt: " << is_target << ", next:" << found_next << std::endl;
                    if constexpr (i != last)
                    {
                        const auto[found_in_next, tail] = follow_symbol<dir, symbol + 1, strategy, ret_strategy, found_next>(target, def);
                        constexpr auto found_ret_optional = std::integral_constant<bool, (is_target || decltype(found_in_next)::value)>();

                        // Check if previous symbol is a target
                        if constexpr (is_target)
                        {
                            const auto res = std::tuple_cat(std::make_tuple(std::get<i>(def.terms)), tail);

                            // Check return strategy
                            if constexpr (symbol == 0 && ret_strategy == ReturnStrategy::Optional)
                                return std::make_pair(found_ret_optional, res);
                            else
                                return std::make_pair(found_in_next, res);
                        }
                        else
                        {
                            if constexpr (symbol == 0 && ret_strategy == ReturnStrategy::Optional)
                                return std::make_pair(found_ret_optional, tail);
                            else
                                return std::make_pair(found_in_next, tail);
                        }
                    } else {
                        // Last element, end of recursion
                        constexpr auto found_res = std::integral_constant<bool, found_next>();
                        if constexpr (is_target)
                            return std::make_pair(found_res, std::make_tuple(std::get<i>(def.terms)));
                        else return std::make_pair(found_res, std::tuple<>());
                    }
                }

                // Scan the symbol twice, like Op(A) is actually Op(A, A) (in normal mode)
                else if constexpr (strategy == IterStrategy::Repeat)
                {
                    constexpr bool has_target = peek_into<Target, TSymbol>();
                    // Check if the element is present in alternation symbol using PermuteAll strategy
                    //const auto[found_repeat, res_repeat] = follow_symbol<dir, symbol, IterStrategy::PermuteAll, ret_strategy, true>(target, def);
                    // Default check
                    const auto[found_once, res] = follow_symbol<dir, symbol, IterStrategy::PermuteAll, ret_strategy, has_target>(target, def);
                    if constexpr (has_target)
                    {
                        // Alternation contains target, we add the target to follow set
                        const auto res_total = std::tuple_cat(res, std::make_tuple(target));
                        return std::make_pair(found_once, res_total);
                    } else {
                        return std::make_pair(found_once, res);
                    }
                }

                // Include only the first symbol
                else if constexpr (strategy == IterStrategy::First)
                {
                    if constexpr (i == last)
                    {
                        constexpr auto found = std::integral_constant<bool, (ret_strategy == ReturnStrategy::Optional ? (is_target || target_found) : target_found)>();
                        if constexpr (is_target)
                            return std::make_pair(found, std::make_tuple(std::get<i>(def.terms)));
                        else return std::make_pair(found, std::tuple<>());
                    }
                    // We still may find some match there
                    else return follow_symbol<dir, symbol + 1, strategy, is_target>(target, def);
                }

                // RecurseFirst: pass the first symbol into you_must_follow
                else
                {
                    return you_must_follow<dir, is_target>(target, std::get<0>(def));
                }
            }
        }

        /**
         * @brief Recursively construct a simple follow set
         * @tparam dir Left-to-right or right-to-left parsing method
         * @tparam is_target True if the preceding symbol is a target
         * @param target Symbol to check
         * @param def Symbol in where to recursively search
         */
        template<LookaheadDirection dir, bool is_target, class Target, class TSymbol>
        constexpr auto you_must_follow(const Target& target, const TSymbol& def) const
        {
            // Recurse strategy
            if constexpr (is_operator<TSymbol>())
            {
                // Normal, Sequential: sequential order, include only the next operator in the follow set
                if constexpr (get_operator<TSymbol>() == OpType::Concat)
                    return follow_symbol<dir, 0, IterStrategy::Normal, ReturnStrategy::Sequential, is_target>(target, def);

                // Repeat, Sequential: the next operator should follow itself, include only the next operator in the follow set (except when RepeatRange starts with 0 elements)
                else if constexpr (get_operator<TSymbol>() == OpType::RepeatExact || get_operator<TSymbol>() == OpType::RepeatGE ||
                    (get_operator<TSymbol>() == OpType::RepeatRange && get_range_from<TSymbol>() != 0))
                    return follow_symbol<dir, 0, IterStrategy::Repeat, ReturnStrategy::Sequential, is_target>(target, def);

                // Repeat, Optional: the next operator should follow itself, include the next and the following operator (only when RepeatRange starts with 0 elements)
                else if constexpr (get_operator<TSymbol>() == OpType::Repeat ||
                    (get_operator<TSymbol>() == OpType::RepeatRange && get_range_from<TSymbol>() == 0))
                    return follow_symbol<dir, 0, IterStrategy::Repeat, ReturnStrategy::Optional, is_target>(target, def);

                // PermuteAll, Optional: permute over all symbols, include the next and the following operator
                else if constexpr (get_operator<TSymbol>() == OpType::Optional)
                    return follow_symbol<dir, 0, IterStrategy::PermuteAll, ReturnStrategy::Optional, is_target>(target, def);

                // PermuteAll, Sequential: permute over all symbols, include the next and the following operator
                else if constexpr (get_operator<TSymbol>() == OpType::Alter)
                    return follow_symbol<dir, 0, IterStrategy::PermuteAll, ReturnStrategy::Sequential, is_target>(target, def);

                // Skip: include the symbol following after this operator
                else if constexpr (get_operator<TSymbol>() == OpType::Comment || get_operator<TSymbol>() == OpType::SpecialSeq)
                    return std::make_pair(std::false_type(), std::tuple<>());

                // FollowOnce and include only the first symbol
                else if constexpr (get_operator<TSymbol>() == OpType::Except)
                    return follow_symbol<dir, 0, IterStrategy::First, ReturnStrategy::Sequential, is_target>(target, def);

                // Group: recurse into the first element
                else return follow_symbol<dir, 0, IterStrategy::RecurseFirst, ReturnStrategy::Sequential, is_target>(target, def);
            }

            // We find an NTerm or a Term on the top-level
            else
            {
                constexpr auto target_found = std::is_same<std::decay_t<TSymbol>, std::decay_t<Target>>();
                if constexpr (is_target)
                    return std::make_pair(target_found, std::make_tuple(def));
                else return std::make_pair(target_found, std::tuple<>());
            }
        }

        template<std::size_t depth, class Target, class TDefsTuple>
        constexpr auto follow_set_each_def(const Target& target, const TDefsTuple& defs) const
        {
            // Descend into each definition
            if constexpr (depth + 1 < std::tuple_size_v<std::decay_t<TDefsTuple>>)
                return std::tuple_cat(you_must_follow<LookaheadDirection::Forward, false>(target, std::get<depth>(defs)).second, follow_set_each_def<depth+1>(target, defs));
            else return you_must_follow<LookaheadDirection::Forward, false>(target, std::get<depth>(defs)).second;
        }

        template<std::size_t depth, class NTermsTuple, class RRTree, class NTermsMap>
        constexpr auto follow_set_each_symbol(const NTermsTuple& nterms, const RRTree& reverse_rules, const NTermsMap& nterms2defs) const
        {
            // Iterate over all targets and descend over the related rules
            const auto target = std::get<depth>(nterms);
            std::cout << "tgt: <" << target.type() << "> ";

            // Get related symbols at i + the symbol definition itself
            const auto includes = std::tuple_cat(std::make_tuple(target), reverse_rules.get(target));
            std::cout << "includes tuple : ";
            print_symbols_tuple(includes) << std::endl;

            // Morph the matching symbols into their definitions
            const auto defs = tuple_morph([&]<std::size_t i>(const auto& c){ return std::get<1>(nterms2defs.get(std::get<i>(c))->terms); }, includes);
            std::cout << "defs tuple : ";
            print_symbols_tuple(defs) << std::endl;

            // Descend into each definition
            if constexpr (depth+1 < std::tuple_size_v<NTermsTuple>)
                return std::tuple_cat(std::make_tuple(tuple_unique(follow_set_each_def<0>(target, defs))), follow_set_each_symbol<depth+1>(nterms, reverse_rules, nterms2defs));
            else return std::make_tuple(tuple_unique(follow_set_each_def<0>(target, defs)));
        }
    };
}

template<class NTermsTuple, class MustFollowTuple>
class FollowSet
{
public:
    NTermsTuple defs;
    MustFollowTuple follow;

    constexpr FollowSet(const NTermsTuple& nterms, const MustFollowTuple& follow) : defs(nterms), follow(follow) {}

    template<class TSymbol>
    constexpr auto get(const TSymbol& symbol) const
    {
        return do_get<0>(symbol);
    }

    template<class Target, class TSymbol>
    constexpr bool can_reduce(const Target& match, const TSymbol& next) const
    {
        return !tuple_contains_v<TSymbol, decltype(get(match))>;
    }

protected:
    template<std::size_t depth, class TSymbol>
    constexpr auto do_get(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<NTermsTuple>, "NTerm type not found");
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::tuple_element_t<depth, NTermsTuple>>)
        {
            return std::get<depth>(follow);
        }
        else return do_get<depth + 1, TSymbol>(symbol);
    }
};

template<class RRTree, class NTermsMap>
auto follow_set_factory(const RRTree& reverse_rules, const NTermsMap& nterms2defs)
{
    auto nterms = nterms2defs.nterms;
    auto factory = cfg_helpers::FollowSetFactory();
    auto follow = factory.follow_set_each_symbol<0>(nterms, reverse_rules, nterms2defs);
    return FollowSet(nterms, follow);
}


template<class Follow>
class SimpleLookahead
{
protected:
    std::size_t _lookahead_state;
public:
    Follow follow_set;

    constexpr explicit SimpleLookahead(const Follow& follow) : follow_set(follow), _lookahead_state(0) {}

    [[nodiscard]] std::size_t get_lookahead() const { return _lookahead_state; }

    void set_lookahead(std::size_t lookahead) { _lookahead_state = lookahead; }

    template<class SymbolsHT, class Target, class GSymbolV>
    bool can_reduce(SymbolsHT& ht, const Target& match, const GSymbolV& next) const
    {
        if (next.is_token())
        {
            // term
            return ht.get_term(next.value, [&](const auto& term){
                return follow_set.can_reduce(match, term);
            });
        }
        // nterm
        return ht.get_nterm(next.type, [&](const auto& nterm){
            return follow_set.can_reduce(match, nterm);
        });
    }

    template<class Target, class TSymbol>
    bool can_reduce(const Target& match, const TSymbol& next) const
    {
        return follow_set.can_reduce(match, next);
    }

    template<class VStr>
    void prettyprint() const
    {
        std::cout << "SimpleLookahead::prettyprint() : " << std::endl;
        std::cout << "  FOLLOW SET : " << std::endl;
        do_print<0, VStr>();
    }

protected:
    template<std::size_t depth, class VStr>
    void do_print() const
    {
        const auto& nt = std::get<depth>(follow_set.defs);
        const auto& follow = std::get<depth>(follow_set.follow);
        std::cout << VStr(nt.type()) << " -> ";
        do_print_follow(follow) << std::endl;

        if constexpr (depth + 1 < std::tuple_size_v<decltype(follow_set.defs)>)
            do_print<depth+1, VStr>();
    }

    template<class Tuple>
    std::ostream& do_print_follow(const Tuple& tuple, bool is_op = false) const
    {
        tuple_each_tree(tuple, [&]<typename TSymbol>(std::size_t d, std::size_t i, const TSymbol& elem)
        {
            if constexpr (is_operator<std::decay_t<TSymbol>>())
            {
                std::cout << "(" << static_cast<int>(get_operator<std::decay_t<TSymbol>>()) << ")<";
                do_print_follow(elem.terms, true);
                std::cout << "> ";
            }
            else
            {
                if constexpr (is_term<TSymbol>()) std::cout << "t:";
                else std::cout << "n:";
                std::cout << elem.type() << ", ";
            }
        },
        [&](std::size_t d, std::size_t i, bool tuple_start){ std::cout << (tuple_start || is_op ? "" : "; "); });
        return std::cout;
    }
};


class NoLookahead {};


template<class RRTree, class NTermsMap>
auto simple_lookahead_factory(const RRTree& reverse_rules, const NTermsMap& nterms2defs)
{
    return SimpleLookahead(follow_set_factory(reverse_rules, nterms2defs));
}

#endif //FOLLOW_H
