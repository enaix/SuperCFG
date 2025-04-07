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
        RecurseFirst,
        First,
        All
    };

    struct FollowSetFactory
    {
        /**
         * @brief Iterate over symbols in an operator using the provided strategy
         * @tparam symbol Index of the current symbol in def
         * @tparam strategy Iteration strategy enum
         * @tparam is_target True if we have encountered our target
         * @param target Symbol to check
         * @param def Operator with the symbols we need to check
         */
        template<std::size_t symbol, IterStrategy strategy, bool is_target, class Target, class TSymbol>
        constexpr auto follow_symbol_rl(const Target& target, const TSymbol& def)
        {
            constexpr std::size_t i = std::tuple_size_v<decltype(def.terms)> - symbol - 1;
            using symbol_type = std::tuple_element_t<i, decltype(def.terms)>;
            constexpr bool target_found = std::is_same_v<symbol_type, std::decay_t<Target>>;

            // We need to recursively process the operator
            if constexpr (is_operator<symbol_type>())
            {
                // Include only one symbol after is_target
                if constexpr (strategy == IterStrategy::Normal)
                {
                    if constexpr (i > 0)
                        return std::tuple_cat(std::make_tuple(you_must_follow_rl<is_target>(target, std::get<i>(def.terms))),
                            follow_symbol_rl<symbol+1, strategy, is_target>(target, def));
                    else return std::make_tuple(you_must_follow_rl<is_target>(target, std::get<i>(def.terms)));
                }
                else if constexpr (strategy == IterStrategy::All)
                {
                    if constexpr (i > 0)
                        return std::tuple_cat(std::make_tuple(you_must_follow_rl<true>(target, std::get<i>(def.terms))),
                            follow_symbol_rl<symbol+1, strategy, true>(target, def));
                    else return std::make_tuple(you_must_follow_rl<true>(target, std::get<i>(def.terms)));
                }
            }
            // Term or a NTerm
            else
            {
                // Include only one symbol after is_target
                if constexpr (strategy == IterStrategy::Normal)
                {
                    if constexpr (i > 0)
                    {
                        if constexpr (is_target)
                            return std::tuple_cat(std::make_tuple(std::get<i>(def.terms)), follow_symbol_rl<symbol + 1, strategy, target_found>(target, def));
                        else
                            return follow_symbol_rl<symbol + 1, strategy, target_found>(target, def);
                    } else {
                        if constexpr (is_target)
                            return std::make_tuple(std::get<i>(def.terms));
                        else return std::tuple<>();
                    }
                }

                // Include all symbols, is_target does not matter
                else if constexpr (strategy == IterStrategy::All)
                {
                    if constexpr (i > 0)
                        return std::tuple_cat(std::make_tuple(std::get<i>(def.terms)), follow_symbol_rl<symbol + 1, strategy, true>(target, def));
                    else
                        return std::make_tuple(std::get<i>(def.terms));
                }

                // Include only the first symbol
                else if constexpr (strategy == IterStrategy::First)
                {
                    if constexpr (i == 0)
                    {
                        if constexpr (is_target)
                            return std::make_tuple(std::get<i>(def.terms));
                        else return std::tuple<>();
                    }
                    // We still may find some match there
                    else return follow_symbol_rl<symbol + 1, strategy, is_target>(target, def);
                }

                // RecurseFirst: pass the first symbol into you_must_follow
                else
                {
                    return you_must_follow_rl<is_target>(target, std::get<0>(def));
                }
            }
        }

        /**
         * @brief Recursively construct a simple follow set. Works in right-to-left manner
         * @tparam is_target True if the preceding symbol is a target
         * @param target Symbol to check
         * @param def Symbol in where to recursively search
         */
        template<bool is_target, class Target, class TSymbol>
        constexpr auto you_must_follow_rl(const Target& target, const TSymbol& def)
        {
            // Recurse strategy
            if constexpr (is_operator<TSymbol>())
            {
                // FollowOnce: include only the next operator in the follow set (except when RepeatRange starts with 0 elements)
                if constexpr (get_operator<TSymbol>() == OpType::Concat || get_operator<TSymbol>() == OpType::Alter ||
                    get_operator<TSymbol>() == OpType::RepeatExact || get_operator<TSymbol>() == OpType::RepeatGE ||
                    !(get_operator<TSymbol>() == OpType::RepeatRange && get_range_from<TSymbol>() == 0))

                    return follow_symbol_rl<0, IterStrategy::Normal, is_target>(target, def);

                // FollowAll: include the next and the following operator (only when RepeatRange starts with 0 elements)
                else if constexpr (get_operator<TSymbol>() == OpType::Optional || get_operator<TSymbol>() == OpType::Repeat ||
                    !(get_operator<TSymbol>() == OpType::RepeatRange && get_range_from<TSymbol>() > 0))
                    return follow_symbol_rl<0, IterStrategy::All, is_target>(target, def);

                // Skip: include the symbol following after this operator
                else if constexpr (get_operator<TSymbol>() == OpType::Comment || get_operator<TSymbol>() == OpType::SpecialSeq)
                    return std::tuple<>();

                // FollowOnce and include only the first symbol
                else if constexpr (get_operator<TSymbol>() == OpType::Except)
                    return follow_symbol_rl<0, IterStrategy::First, is_target>(target, def);

                // Group: recurse into the first element
                else return follow_symbol_rl<0, IterStrategy::RecurseFirst, is_target>(target, def);
            }

            // We find an NTerm or a Term on the top-level
            else
            {
                if constexpr (is_target)
                    return std::make_tuple(def);
                else return std::tuple<>();
            }
        }

        template<std::size_t depth, class Target, class TDefsTuple>
        constexpr auto follow_set_each_def(const Target& target, const TDefsTuple& defs)
        {
            if constexpr (depth + 1 < std::tuple_size_v<std::decay_t<TDefsTuple>>)
                return std::tuple_cat(std::make_tuple(you_must_follow_rl<false>(target, std::get<depth>(defs))), follow_set_each_def<depth+1>(target, defs));
            else return std::make_tuple(you_must_follow_rl<false>(target, std::get<depth>(defs)));
        }

        template<std::size_t depth, class NTermsTuple, class RRTree, class NTermsMap>
        constexpr auto follow_set_each_symbol(const NTermsTuple& nterms, const RRTree& reverse_rules, const NTermsMap& nterms2defs)
        {
            const auto target = std::get<depth>(nterms);

            // Get related symbols at i + the symbol definition itself
            const auto includes = std::tuple_cat(std::make_tuple(target), reverse_rules.get(target));

            // Morph the matching symbols into their definitions
            const auto defs = tuple_morph([&]<std::size_t i>(const auto& c){ return *nterms2defs.get(std::get<i>(c)); }, includes);

            // Descend into each definition
            if constexpr (depth+1 < std::tuple_size_v<NTermsTuple>)
                return std::make_tuple(tuple_unique(follow_set_each_def<0>(target, defs)), follow_set_each_symbol<depth+1>(nterms, reverse_rules, nterms2defs));
            else return tuple_unique( follow_set_each_def<0>(target, defs));
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
    constexpr bool check(const Target& match, const TSymbol& next) const
    {
        return tuple_contains_v<TSymbol, decltype(get(match))>;
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
    bool check(SymbolsHT& ht, const Target& match, const GSymbolV& next)
    {
        if (next.is_token())
        {
            // term
            return ht.get_term(next.value, [&](const auto& term){
                return follow_set.check(match, term);
            });
        }
        // nterm
        return ht.get_nterm(next.type, [&](const auto& nterm){
            return follow_set.check(match, nterm);
        });
    }

    template<class VStr>
    void prettyprint() const
    {
        std::cout << "SimpleLookahead::prettyprint() : " << std::endl;
        do_print<0, VStr>();
    }

protected:
    template<std::size_t depth, class VStr>
    void do_print() const
    {
        const auto& nt = std::get<depth>(follow_set.defs);
        const auto& follow = std::get<depth>(follow_set.follow);
        std::cout << VStr(nt.type()) << " -> ";
        tuple_each(follow, [&](std::size_t i, const auto& elem){ std::cout << VStr(elem.type()) << " "; });
        std::cout << std::endl;
        if constexpr (depth + 1 < std::tuple_size_v<decltype(follow_set.defs)>)
            do_print<depth+1, VStr>();
    }
};


class NoLookahead {};


template<class RRTree, class NTermsMap>
auto simple_lookahead_factory(const RRTree& reverse_rules, const NTermsMap& nterms2defs)
{
    return SimpleLookahead(follow_set_factory(reverse_rules, nterms2defs));
}

#endif //FOLLOW_H
