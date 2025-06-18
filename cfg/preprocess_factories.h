//
// Created by Flynn on 19.03.2025.
//

#ifndef PREPROCESS_FACTORIES_H
#define PREPROCESS_FACTORIES_H

#include "cfg/helpers.h"
#include "cfg/preprocess.h"
#include "cfg/base.h"
#include "cfg/helpers_runtime.h"

namespace cfg_helpers
{
    // terms map factory helpers
    template<class NTerm, class TSymbol>
    constexpr auto terms_map_descend_each_def(const NTerm& lhs, const TSymbol& symbol, auto descend_each_func)
    {
        if constexpr (is_operator<TSymbol>())
        {
            return descend_each_func(symbol.terms, lhs, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(symbol.terms)>>>{});
        } else if constexpr (is_nterm<TSymbol>()) {
            return std::make_tuple<>();
        } else if constexpr (is_term<TSymbol>()) {
            return std::make_tuple(std::make_tuple(symbol, lhs));
        } else if constexpr (is_terms_range<TSymbol>()) {
            return std::make_tuple<>(); // Does not support range
        } else static_assert(terminal_type<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
    }

    template<class SrcTuple, class NTerm, std::size_t... Ints>
    constexpr auto terms_map_descend_each(const SrcTuple& op, const NTerm& lhs, const std::integer_sequence<std::size_t, Ints...>)
    {
        return std::tuple_cat(terms_map_descend_each_def(lhs, std::get<Ints>(op), [&](const auto&... args){ return terms_map_descend_each(args...);})...);
    }

    template<class SrcTuple, std::size_t... Ints>
    constexpr auto terms_map_descend_each_ruledef(const SrcTuple& op, const std::integer_sequence<std::size_t, Ints...>)
    {
        return std::make_tuple(terms_map_descend_each_def(std::get<0>(std::get<Ints>(op).terms), std::get<1>(std::get<Ints>(op).terms), [&](const auto&... args){ return terms_map_descend_each(args...);})...);
    }

    // symbols HT helpers
    template<class SrcTuple, std::size_t... Ints>
    constexpr auto symbols_ht_descend_each(const SrcTuple& op, const std::integer_sequence<std::size_t, Ints...>, auto func)
    {
        return std::tuple_cat(func(std::get<Ints>(op))...);
    }

    template<class TSymbol>
    constexpr auto symbols_ht_find_nterms(const TSymbol& elem)
    {
        static_assert(is_operator<TSymbol>(), "Symbol is not an operator");
        static_assert(get_operator<TSymbol>() == OpType::RulesDef, "Operator is not a root symbol");
        return tuple_morph([&]<std::size_t i>(const auto& def){
            return std::get<0>(std::get<i>(def).terms); // Get the nterm definition (on the left)
        }, elem.terms);
    }

    /**
     * @brief Recursively iterate over the tree and return all terms we could find
     */
    template<class TSymbol>
    constexpr auto symbols_ht_find_terms(const TSymbol& elem)
    {
        if constexpr (is_operator<TSymbol>())
        {
            return symbols_ht_descend_each(elem.terms, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(elem.terms)>>>{}, [&](const auto& e){ return symbols_ht_find_terms(e); });
        } else if constexpr (is_nterm<TSymbol>()) {
            return std::make_tuple<>();
        } else if constexpr (is_term<TSymbol>()) {
            return std::make_tuple(elem);
        } else if constexpr (is_terms_range<TSymbol>()) {
            return std::make_tuple<>(); // Do not include range
        } else static_assert(terminal_type<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
    }

    // rr_tree_helpers

    /**
     * @brief Check if the nterm is present in the operator
     * @tparam TSymbol Operator where we should check
     * @tparam TNTerm Symbol to check
     * @param is_nterm_in_rule rr_tree_is_nterm_in_rule function
     */
    template<std::size_t depth, class TSymbol, class TNTerm>
    constexpr bool rr_tree_process_op(auto is_nterm_in_rule)
    {
        using Op = std::tuple_element_t<depth, typename TSymbol::term_types_tuple>;
        if (is_nterm_in_rule.template operator()<Op, TNTerm>()) return true;

        if constexpr (depth + 1 < std::tuple_size_v<typename TSymbol::term_types_tuple>)
            return rr_tree_process_op<depth + 1, TSymbol, TNTerm>(is_nterm_in_rule);
        else return false;
    }

    /**
     * @brief Check if a symbol is present in this definition
     * @tparam TSymbol Definition where we should check
     * @tparam TNTerm Symbol to check
     */
    template<class TSymbol, class TNTerm>
    constexpr bool rr_tree_is_nterm_in_rule()
    {
        // Descend over all operators and check if it's present in this rule definition
        if constexpr (is_nterm<TSymbol>())
        {
            return std::is_same_v<TSymbol, TNTerm>;
        }
        else if constexpr (is_operator<TSymbol>())
        {
            return rr_tree_process_op<0, TSymbol, TNTerm>([&]<class D, class N>(){ return rr_tree_is_nterm_in_rule<D, N>(); });
        }
        else return false; // We need to gracefully handle this case
    }

    /**
     * @brief Find which rule definitions contain nterm
     */
    template<std::size_t depth, class RulesSymbol, class TSymbol>
    constexpr auto rr_tree_iterate_over_rules(const RulesSymbol& rules, const TSymbol& nterm)
    {
        // Iterate over all rules
        const auto& define_op = std::get<depth>(rules.terms); // get i-th definition
        const auto& rule_nterm = std::get<0>(define_op.terms);
        const auto& rule_def = std::get<1>(define_op.terms);

        // Check if we compare the type with its own definition
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(rule_nterm)>, TSymbol>)
        {
            // We need to skip over this element
            if constexpr (depth + 1 >= std::tuple_size_v<typename RulesSymbol::term_types_tuple>) return std::tuple<>();
            else return rr_tree_iterate_over_rules<depth+1>(rules, nterm);
        } else {
            // Check if the element is in this definition
            constexpr bool found = rr_tree_is_nterm_in_rule<std::decay_t<decltype(rule_def)>, TSymbol>();

            if constexpr (depth + 1 >= std::tuple_size_v<typename RulesSymbol::term_types_tuple>)
            {
                if constexpr (found)
                    return std::make_tuple(rule_nterm);
                else
                    return std::tuple<>();
            }
            else
            {
                if constexpr (found)
                    return std::tuple_cat(std::make_tuple(rule_nterm), rr_tree_iterate_over_rules<depth+1>(rules, nterm));
                else
                    return rr_tree_iterate_over_rules<depth+1>(rules, nterm);
            }
        }
    }

    /**
     * @brief Generate a reverse rules set for i-th definition
     */
    template<std::size_t depth, class RulesSymbol>
    constexpr auto rr_tree_for_symbol(const RulesSymbol& rules)
    {
        const auto& nterm = std::get<0>(std::get<depth>(rules.terms).terms); // key (nterm)
        const auto nterms = rr_tree_iterate_over_rules<0>(rules, nterm); // value (related definitions)

        if constexpr (depth + 1 >= std::tuple_size<typename RulesSymbol::term_types_tuple>()) return std::make_tuple(nterms);
        else return std::tuple_cat(std::make_tuple(nterms), rr_tree_for_symbol<depth+1>(rules));
    }

    // terms cache

    template<class TSymbol>
    constexpr auto terms_cache_each_elem(const TSymbol& symbol)
    {
        if constexpr (is_operator<TSymbol>())
        {
            // Morph each symbol s into each_elem(s) and concat
            return tuple_morph_each<true>(symbol.terms, [&](std::size_t i, const auto& s){ return terms_cache_each_elem(s); });
        }
        else if constexpr (is_term<TSymbol>() || is_terms_range<TSymbol>())
        {
            return std::make_tuple(symbol);
        }
        else return std::tuple<>();
    }

    template<std::size_t depth, class RulesSymbol>
    constexpr auto terms_cache_each_rule(const RulesSymbol& rules)
    {
        const auto& def = std::get<depth>(rules.terms);
        auto terms = terms_cache_each_elem(std::get<1>(def.terms));
        // Build a mapping
        if constexpr (depth + 1 < std::tuple_size_v<std::decay_t<decltype(rules.terms)>>)
            return std::tuple_cat(std::make_tuple(terms), terms_cache_each_rule<depth + 1>(rules));
        else return std::make_tuple(terms);
    }

    template<std::size_t i, class TSymbol, class TypesCache>
    constexpr auto find_term_in_cache_all(const TSymbol& symbol, const TypesCache& cache)
    {
        // Check if the symbol is present in cache
        if constexpr (tuple_each_type_or_return<std::tuple_element_t<i, std::decay_t<decltype(cache.terms)>>>([&]<std::size_t j, class TElem>() constexpr { return std::is_same_v<std::decay_t<TSymbol>, std::decay_t<TElem>>; }))
        {
            if constexpr (i + 1 < std::tuple_size_v<std::decay_t<decltype(cache.terms)>>)
                return std::tuple_cat(std::make_tuple(std::get<i>(cache.defs)), find_term_in_cache_all<i+1>(symbol, cache));
            else
                return std::make_tuple(std::get<i>(cache.defs));
        } else {
            if constexpr (i + 1 < std::tuple_size_v<std::decay_t<decltype(cache.terms)>>)
                return find_term_in_cache_all<i+1>(symbol, cache);
            else
                return std::tuple<>();
        }
    }

    template<std::size_t i, class TSymbol, class TypesCache>
    constexpr auto find_term_in_cache_single(const TSymbol& symbol, const TypesCache& cache)
    {
        if constexpr (tuple_contains_v<TSymbol, std::decay_t<decltype(std::get<i>(cache.terms))>>)
            return std::get<i>(cache.defs);
        else if constexpr (i + 1 < std::tuple_size_v<std::decay_t<decltype(cache.terms)>>)
            return find_term_in_cache_single<i+1>(symbol, cache);
        static_assert(i+1 < std::tuple_size_v<std::decay_t<decltype(cache.terms)>>, "Term not found in cache");
    }

    // reducibility checker

    /**
     * @brief Get position of a symbol in the prefix or postfix of a given rule. (Pre/Post)fix is a sequence of symbols which is always present at a particular position. If a symbol always occurs more than once, only the first match is resolved
     * @tparam dir_up Up (true) - prefix, down (false) - postfix
     * @param target Symbol to check
     * @param symbol Symbol to descend into
     */
    template<bool dir_up, std::size_t pos, std::size_t i, class TDef, class TSymbol>
    constexpr auto rc1_rule_get_fix(const TDef& target, const TSymbol& symbol)
    {
        // Can we move to the next symbol?
        constexpr bool next = (dir_up ? i < std::tuple_size_v<std::decay_t<decltype(symbol.terms)>> : i > 0);
        constexpr std::size_t i_next = (dir_up ? i + 1 : i - 1);

        if constexpr (is_operator<TSymbol>())
        {
            if constexpr (get_operator<TSymbol>() == OpType::Concat)
            {
                // At each step increase the pos and return if we find one encounter
                // Descend into symbol
                const auto res = rc1_rule_get_fix<pos, 0>(target, std::get<i>(symbol.terms));
                if constexpr (next && std::is_same_v<std::decay_t<decltype(res)>, std::tuple<>>)
                {
                    // Target not found, check the next term in symbol
                    return rc1_rule_get_fix<pos+1, i_next>(target, symbol);
                } else return res;
            }
            // Only deterministic prefix or postfix is Concat and repeat with M >= 1
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatExact || get_operator<TSymbol>() == OpType::RepeatRange || get_operator<TSymbol>() == OpType::RepeatGE)
            {
                if constexpr (get_range_from<TSymbol>() > 0)
                    // Get the FIRST entry
                    return rc1_rule_get_fix<pos, 0>(target, std::get<0>(symbol.terms));
                else return std::tuple<>();
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Group)
            {
                // Check only the first symbol
                if constexpr (std::tuple_size_v<std::decay_t<decltype(symbol.terms)>> == 0)
                    return std::tuple<>();
                else
                    return rc1_rule_get_fix<pos, 0>(target, std::get<0>(symbol.terms));
            }
            // Other symbols are not deterministic
            else return std::tuple<>();
        }
        else if constexpr (is_nterm<TSymbol>())
        {
            if constexpr (std::is_same_v<std::decay_t<TDef>, std::decay_t<TSymbol>>)
                return std::integral_constant<std::size_t, pos>{}; // We do not need to wrap it into a tuple
            else
                return std::tuple<>();
        } else {
            // We need to handle cases when target is a range
            if constexpr (terms_intersect_v<std::decay_t<TDef>, std::decay_t<TSymbol>>)
                return std::integral_constant<std::size_t, pos>{};
            else return std::tuple<>(); // No intersection or target is an nterm
        }
    }

    /**
     * @brief Get the first element encounter position
     * @param target Symbol to check
     * @param symbol Symbol to descend into
     * @param rule Original rule that we are checking
     */
    template<std::size_t pos, std::size_t i, class TDef, class TSymbol, class TRuleDef>
    constexpr auto rc1_get_elem_pos_in_rule(const TDef& target, const TSymbol& symbol, const TRuleDef& rule)
    {
        // TODO Delete this dead code
        if constexpr (is_operator<TSymbol>())
        {
            if constexpr (get_operator<TSymbol>() == OpType::Concat)
            {
                // At each step increase the pos and return if we find one encounter
                if constexpr (i >= std::tuple_size_v<std::decay_t<decltype(symbol.terms)>>)
                    return std::tuple<>();
                else
                {
                    // Descend into symbol
                    const auto res = rc1_get_elem_pos_in_rule<pos, 0>(target, std::get<i>(symbol.terms), rule);
                    if constexpr (std::is_same_v<std::decay_t<decltype(res)>, std::tuple<>>)
                    {
                        // Target not found, check the next term in symbol
                        return rc1_get_elem_pos_in_rule<pos+1, i+1>(target, symbol, rule);
                    } else return res;
                }
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Alter)
            {
                // Permute over all possible symbols and return all positions that match
                if constexpr (i >= std::tuple_size_v<std::decay_t<decltype(symbol.terms)>>)
                    return std::tuple<>();
                else
                {
                    // Descend into symbol
                    const auto res = rc1_get_elem_pos_in_rule<pos, 0>(target, std::get<i>(symbol.terms), rule);
                    return std::tuple_cat(res, rc1_get_elem_pos_in_rule<pos, i+1>(target, symbol, rule));
                }
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Group || get_operator<TSymbol>() == OpType::Repeat || get_operator<TSymbol>() == OpType::Optional)
            {
                // Check only the first symbol
                if constexpr (std::tuple_size_v<std::decay_t<decltype(symbol.terms)>> == 0)
                    return std::tuple<>();
                else
                    return rc1_get_elem_pos_in_rule<pos, 0>(target, std::get<0>(symbol.terms), rule);
            }
        }
        else if constexpr (is_nterm<TSymbol>())
        {
            if constexpr (std::is_same_v<std::decay_t<TDef>, std::decay_t<TSymbol>>)
                return std::make_tuple(std::make_pair(rule, std::integral_constant<std::size_t, pos>{}));
            else
                return std::tuple<>();
        } else return std::tuple<>();
        // TODO handle other symbols
    }

    /**
     * @brief Iterate over each nterm, get the related rules and find the starting position in prefix and postfix
     * @param defs RRTree defs tuple
     * @param rules RRTree rules tuple
     * @param nterms2defs NTerms to definitions mapping
     */
    template<std::size_t depth, class TDefsTuple, class TRuleTree, class NTermsMap>
    constexpr auto rc1_get_match(const TDefsTuple& defs, const TRuleTree& rules, const NTermsMap& nterms2defs)
    {
        const auto& def = std::get<0>(std::get<depth>(defs).terms);
        const auto& r_rules = std::get<depth>(rules);

        const auto res = concat_each<std::tuple_size_v<std::decay_t<decltype(r_rules)>>, true>([&]<std::size_t i>(){
            const auto& rule_nterm = std::get<i>(r_rules);
            const auto& rule_def = std::get<1>(nterms2defs.get(rule_nterm)->terms);

            auto null_to_max = []<class TRes>(const TRes& res){
                if constexpr (std::is_same_v<std::decay_t<TRes>, std::tuple<>>)
                    return std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>{};
                else return res;
            };

            const auto prefix = null_to_max(rc1_rule_get_fix<true, 0, 0>(def, rule_def));
            const auto postfix = null_to_max(rc1_rule_get_fix<false, 0, std::tuple_size_v<std::decay_t<decltype(rule_def.terms)>> - 1>(def, rule_def));

            return std::make_tuple(std::make_pair(rule_nterm, std::make_pair(prefix, postfix)));
        });

        if constexpr (depth + 1 < std::tuple_size_v<TDefsTuple>)
            return std::tuple_cat(std::make_tuple(res), rc1_get_match<depth+1>(defs, rules, nterms2defs));
        else
            return std::make_tuple(res);
    }


    /**
     * @brief Same as rc1_get_match, but finds Terms and TermsRange in rules
     * @param terms TermsTypeMap terms
     * @param nterms TermsTypeMap nterms (rules)
     * @param nterms2defs NTerms to definitions mapping
     */
    template<std::size_t depth, class TTermsTuple, class TDefsTuple, class NTermsMap>
    constexpr auto ctx_get_term_match(const TTermsTuple& terms, const TDefsTuple& nterms, const NTermsMap& nterms2defs)
    {
        const auto& def = std::get<depth>(terms);
        const auto& r_rules = std::get<depth>(nterms);

        const auto res = concat_each<std::tuple_size_v<std::decay_t<decltype(r_rules)>>, true>([&]<std::size_t i>(){
            const auto& rule_nterm = std::get<i>(r_rules);
            const auto& rule_def = std::get<1>(nterms2defs.get(rule_nterm)->terms);

            auto null_to_max = []<class TRes>(const TRes& res){
                if constexpr (std::is_same_v<std::decay_t<TRes>, std::tuple<>>)
                    return std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>{};
                else return res;
            };

            // Warning: due to TermsRange symbol pos is still kind of ambiguous!
            const auto prefix = null_to_max(rc1_rule_get_fix<true, 0, 0>(def, rule_def));
            const auto postfix = null_to_max(rc1_rule_get_fix<false, 0, std::tuple_size_v<std::decay_t<decltype(rule_def.terms)>> - 1>(def, rule_def));

            return std::make_tuple(std::make_pair(rule_nterm, std::make_pair(prefix, postfix)));
        });

        if constexpr (depth + 1 < std::tuple_size_v<TDefsTuple>)
            return std::tuple_cat(std::make_tuple(res), ctx_get_term_match<depth+1>(terms, nterms, nterms2defs));
        else
            return std::make_tuple(res);
    }


    template<class TSymbol, class TVisitedTuple, class TRuleTree>
    constexpr auto rc1_full_rrtree_recurse(const TSymbol& symbol, const TVisitedTuple& visited, const TRuleTree& rules)
    {
        if constexpr (tuple_contains_v<std::decay_t<TSymbol>, std::decay_t<TVisitedTuple>>)
            return std::tuple<>();
        else
        {
            const auto& related = rules.get(symbol);
            const auto visited_next = std::tuple_cat(visited, std::make_tuple(symbol));
            return tuple_morph_each<true>(related, [&](std::size_t i, const auto& elem){ return std::tuple_cat(std::make_tuple(elem), rc1_full_rrtree_recurse(elem, visited_next, rules)); });
        }
    }

    template<std::size_t depth, class TSymbol, class SymbolsTuple>
    [[nodiscard]] constexpr std::size_t rc1_get_ctx_index()
    {
        static_assert(depth < std::tuple_size_v<SymbolsTuple>, "No such symbol found");
        if constexpr (std::is_same_v<std::decay_t<std::tuple_element_t<depth, SymbolsTuple>>, std::decay_t<TSymbol>>)
            return depth;
        else
            return rc1_get_ctx_index<depth+1, TSymbol, SymbolsTuple>();
    }

    template<std::size_t N, std::array<std::size_t, N> ctx, std::size_t i, std::size_t MAX>
    constexpr auto ctx_inv()
    {
        if constexpr ([&](){
            for (std::size_t j = 0; j < N; j++)
            {
                if (ctx[j] == i)
                    return false;
            }
            return true;
        }())
        {
            if constexpr (i + 1 < MAX)
                return std::tuple_cat(std::make_tuple(i), ctx_inv<N, ctx, i+1, MAX>());
            else
                return std::make_tuple(i);
        } else {
            if constexpr (i + 1 < MAX)
                return ctx_inv<N, ctx, i+1, MAX>();
            else
                return std::tuple<>();
        }
    }

    /**
     * @brief Generate a reverse rules tree which contains all possible nterms. The resulting tuple contains N arrays of rule indices, where element is never present
     */
    template<bool do_prettyprint, std::size_t depth, class TDefsTuple, class TRuleTree, class TRules>
    constexpr auto rc1_get_full_rrtree(const TDefsTuple& defs, const TRuleTree& rules, const TRules& all_rules)
    {
        const auto& def = std::get<0>(std::get<depth>(defs).terms);
        // iterate over related rules and recurse until we find a loop
        const auto related = tuple_unique(rc1_full_rrtree_recurse(def, std::tuple<>(), rules));
        if constexpr (do_prettyprint)
        {
            std::cout << def.type() << " -> ";
            print_symbols_tuple(related);
        }

        constexpr std::size_t M = std::tuple_size_v<std::decay_t<decltype(related)>>;
        // Convert explicit symbols into idx
        constexpr auto related_idx = h_type_morph<std::array<std::size_t, M>, true>([&]<std::size_t i>(const auto& src){
            return rc1_get_ctx_index<0, std::decay_t<std::tuple_element_t<i, std::decay_t<decltype(related)>>>, std::decay_t<TRules>>();
        }, IntegralWrapper<M>{}, related);

        constexpr std::size_t ctx_size_inv = std::tuple_size_v<std::decay_t<TRules>> - M;
        constexpr auto ctx_inv_tup = ctx_inv<M, related_idx, 0, std::tuple_size_v<std::decay_t<TRules>>>();
        constexpr auto related_idx_inv = h_type_morph<std::array<std::size_t, ctx_size_inv>, true>([&]<std::size_t i>(const auto& src){ return std::get<i>(ctx_inv_tup); }, IntegralWrapper<ctx_size_inv>{}, ctx_inv_tup);

        if constexpr (do_prettyprint)
        {
            std::cout << ", idx : {";
            for (std::size_t idx : related_idx) std::cout << idx << " ";
            std::cout << "}, inv : {";
            for (std::size_t idx : related_idx_inv) std::cout << idx << " ";
            std::cout << "}" << std::endl;
        }

        if constexpr (depth + 1 < std::tuple_size_v<TDefsTuple>)
            return std::tuple_cat(std::make_tuple(related_idx_inv), rc1_get_full_rrtree<do_prettyprint, depth+1>(defs, rules, all_rules));
        else
            return std::make_tuple(related_idx_inv);
    }

    template<std::size_t i, class TRange, class TSybmol>
    constexpr auto get_terms_intersection_symbols_ab_step(const TRange& range, const TSybmol& s, const auto& value_lhs, const auto& value_rhs)
    {
        // Need to cover cases where i > 0 (multiple chars in TSymbol)
        // Range + symbol : (   * ) -> (    ) * ( )
        // (Another call) : ( *   ) -> ( ) * (    )
        // Result (next)  :            ( )( )( )( )
        constexpr auto range_start = TRange::_name_type_start::template at<0>();
        constexpr auto range_end = TRange::_name_type_end::template at<0>();
        constexpr auto term_char = TSybmol::_name_type::template at<i>();

        //using mkchar = TSybmol::_name_type::make();

        if constexpr (in_lexical_range(term_char, range_start, range_end))
        {
            const auto v_union = tuple_unique(std::tuple_cat(value_lhs, value_rhs));
            if constexpr (in_lexical_range_strict(term_char, range_start, range_end))
            {
                // Get 2 new ranges
                constexpr auto res = lexical_intersect(term_char, range_start, range_end);
                constexpr auto l_pair = res.first, r_pair = res.second;
                return std::make_tuple(
                    std::make_pair(TermsRange(range.start.template make<l_pair.first>(), range.start.template make<l_pair.second>()), value_lhs), // First range
                    std::make_pair(TermsRange(range.start.template make<r_pair.first>(), range.start.template make<r_pair.second>()), value_lhs), // Second range
                    std::make_pair(Term(range.start.template make<term_char>()), v_union)); // Symbol
            } else {
                // Edge case
                constexpr auto pair = lexical_intersect_edge(term_char, range_start, range_end);
                constexpr auto start = pair.first, end = pair.second;
                return std::make_tuple(std::make_pair(TermsRange(range.start.template make<start>(), range.start.template make<end>()), value_lhs),
                    std::make_pair(Term(range.start.template make<term_char>()), v_union));
            }
        }
        else
        {
            if constexpr (i + 1 < TSybmol::_name_type::size())
                return get_terms_intersection_symbols_ab_step<i+1>(range, s, value_lhs, value_rhs);
            else return std::tuple<>();
        }
    }

    template<class TRange, class TSybmol>
    constexpr auto get_terms_intersection_symbols_ab(const TRange& range, const TSybmol& s, const auto& value_lhs, const auto& value_rhs)
    {
        return get_terms_intersection_symbols_ab_step<0>(range, s, value_lhs, value_rhs);
    }

    template<class TRangeA, class TRangeB>
    constexpr auto get_terms_intersection_ranges(const TRangeA& range_a, const TRangeB& range_b, const auto& value_a, const auto& value_b)
    {
        // Iterate over a term and check each symbol
        constexpr auto a_start = TRangeA::_name_type_start::template at<0>();
        constexpr auto a_end = TRangeA::_name_type_end::template at<0>();
        constexpr auto b_start = TRangeB::_name_type_start::template at<0>();
        constexpr auto b_end = TRangeB::_name_type_end::template at<0>();

        using char_t = typename TRangeA::_name_type_start::char_t;
        //using mkchar = typename TRangeA::_name_type_start::make<char_t>();

        if constexpr (in_lexical_range(a_start, a_end, b_start, b_end))
        {
            auto new_symbol = [&]<const auto start, const auto end>(auto value){
                if constexpr (start == end)
                    return std::make_pair(Term(range_a.start.template make<start>()), value);
                else
                    return std::make_pair(TermsRange(range_a.start.template make<start>(), range_a.start.template make<end>()), value);
            };
            const auto v_union = tuple_unique(std::tuple_cat(value_a, value_b));

            constexpr auto e_p = lexical_ranges_intersect(a_start, a_end, b_start, b_end);
            constexpr auto lhs = std::get<0>(e_p.second), intersect = std::get<1>(e_p.second), rhs = std::get<2>(e_p.second);

            // Manage intersection type
            if constexpr (e_p.first == RangesIntersect::AInB)
                return std::make_tuple(new_symbol.template operator()<lhs.first, lhs.second>(value_b), new_symbol.template operator()<intersect.first, intersect.second>(v_union), new_symbol.template operator()<rhs.first, rhs.second>(value_b));
            else if constexpr (e_p.first == RangesIntersect::BInA)
                return std::make_tuple(new_symbol.template operator()<lhs.first, lhs.second>(value_a), new_symbol.template operator()<intersect.first, intersect.second>(v_union), new_symbol.template operator()<rhs.first, rhs.second>(value_a));
            else if constexpr (e_p.first == RangesIntersect::OnlyA)
                return std::make_tuple(new_symbol.template operator()<lhs.first, lhs.second>(value_a), new_symbol.template operator()<intersect.first, intersect.second>(v_union));
            else if constexpr (e_p.first == RangesIntersect::OnlyB)
                return std::make_tuple(new_symbol.template operator()<intersect.first, intersect.second>(v_union), new_symbol.template operator()<rhs.first, rhs.second>(value_b));
            else
            {
                // Partial
                return std::make_tuple(new_symbol.template operator()<lhs.first, lhs.second>(value_a), new_symbol.template operator()<intersect.first, intersect.second>(v_union), new_symbol.template operator()<rhs.first, rhs.second>(value_b));
            }
        } else return std::tuple<>();
    }

    template<class TSymbolA, class TSymbolB>
    constexpr auto get_terms_intersection_symbols(const TSymbolA& a, const TSymbolB& b, const auto& value_lhs, const auto& value_rhs)
    {
        if constexpr (is_term<TSymbolA>() && is_terms_range<TSymbolB>())
            // Term and TermsRange
            return get_terms_intersection_symbols_ab(b, a, value_rhs, value_lhs);
        else if constexpr (is_terms_range<TSymbolA>() && is_term<TSymbolB>())
            // Term and TermsRange
            return get_terms_intersection_symbols_ab(a, b, value_lhs, value_rhs);
        else if constexpr (is_terms_range<TSymbolA>() && is_terms_range<TSymbolB>())
            // Both are TermsRange
            return get_terms_intersection_ranges(a, b, value_lhs, value_rhs);
            //return std::tuple<>();
        else return std::tuple<>();
    }
};


/**
 * @brief Terms to related NTerms mapping builder
 * @tparam RulesSymbol Rules operator
 */
template<class RulesSymbol>
auto terms_map_factory(const RulesSymbol& rules)
{
    auto terms_list = cfg_helpers::terms_map_descend_each_ruledef(rules.terms, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(rules.terms)>>>{});

    auto terms = tuple_flatten_layer(terms_list);

    return TermsMap(terms);
}


template<class TokenType, class RulesSymbol>
auto symbols_ht_factory(const RulesSymbol& rules)
{
    auto nterms = cfg_helpers::symbols_ht_find_nterms(rules); // Find all nterms in rules
    auto terms = tuple_unique(cfg_helpers::symbols_ht_find_terms(rules)); // Find all terms in rules, only get the unique ones

    return SymbolsHashTable<TokenType, decltype(terms), decltype(nterms)>(terms, nterms);
}


template<class RulesSymbol>
auto reverse_rules_tree_factory(const RulesSymbol& rules)
{
    const auto defs = rules.terms;
    const auto nterms = cfg_helpers::rr_tree_for_symbol<0>(rules);

    return ReverseRuleTree(defs, nterms);
}

template<class RulesSymbol>
auto terms_tree_cache_factory(const RulesSymbol& rules)
{
    auto terms = cfg_helpers::terms_cache_each_rule<0>(rules);
    auto defs = tuple_morph([]<std::size_t i>(const auto src){ return std::get<0>(std::get<i>(src).terms); }, rules.terms);
    auto all_terms = tuple_flatten_layer(terms);
    return TermsTreeCache<std::decay_t<decltype(defs)>, std::decay_t<decltype(terms)>, std::decay_t<decltype(all_terms)>>(defs, terms, all_terms);
}

template<class VStr, class TokenType, bool handle_duplicate_types, bool handle_dup_in_rt, class TypesCache>
auto terms_type_map_factory(const TypesCache& cache)
{
    // It's faster to find all related elements in the cache
    // We also need to concat keys and values

    if constexpr (handle_duplicate_types && !handle_dup_in_rt)
    {
        auto collapse = [&](const auto& lhs, const auto& rhs){
            const auto& [a, value_lhs] = lhs;
            const auto& [b, value_rhs] = rhs;
            // auto c = std::get<42>(b);
            if constexpr (std::is_same_v<std::decay_t<decltype(a)>, std::decay_t<decltype(b)>>)
            {
                // Calculate the union between 2 values
                auto value = tuple_unique(std::tuple_cat(value_lhs, value_rhs));
                //auto c = std::get<42>(value);
                return std::make_tuple(std::make_pair(a, value)); // Return the union of 2 types
            }
            else return cfg_helpers::get_terms_intersection_symbols(a, b, value_lhs, value_rhs);
        };
        const auto elems = tuple_morph_each(cache.all_terms, [&](std::size_t i, const auto& elem){ return std::make_pair(elem, cfg_helpers::find_term_in_cache_all<0>(elem, cache)); });
        const auto res = tuple_pairwise_collapse(elems, collapse);

        auto keys = tuple_take_along_axis<0>(res);
        auto values = tuple_take_along_axis<1>(res);

        auto map = TermsTypeMap<VStr, TokenType, std::decay_t<decltype(keys)>, std::decay_t<decltype(values)>>(keys, values);
        map.populate_ht();
        return map;
    } else {
        auto terms_map = tuple_morph_each(cache.all_terms, [&](std::size_t i, const auto& elem){ return cfg_helpers::find_term_in_cache_all<0>(elem, cache); });
        auto map = TermsTypeMap<VStr, TokenType, std::decay_t<decltype(cache.all_terms)>, std::decay_t<decltype(terms_map)>>(cache.all_terms, terms_map);
        if constexpr (handle_dup_in_rt)
            map.populate_ht_with_dup();
        else
            map.populate_ht();
        return map;
    }
}


template<bool do_prettyprint, bool do_context_check, class RRTree, class NTermsMap>
constexpr auto make_reducibility_checker1(const RRTree& tree, const NTermsMap& nterms2defs)
{
    const auto all_related_rules = tuple_unique(tuple_flatten_layer(tree.tree)); // Get a tuple of all unique related rules
    const auto pairs = cfg_helpers::rc1_get_match<0>(tree.defs, tree.tree, nterms2defs);
    if constexpr (do_context_check)
    {
        if constexpr (do_prettyprint)
            std::cout << "  RC(1) FULL REVERSE TREE" << std::endl;
        const auto rr_all = cfg_helpers::rc1_get_full_rrtree<do_prettyprint, 0>(tree.defs, tree, all_related_rules);
        return ReducibilityChecker1<decltype(tree.defs), decltype(pairs), decltype(all_related_rules), decltype(rr_all), do_prettyprint>(tree.defs, pairs, all_related_rules, rr_all);
    }
    else return ReducibilityChecker1<decltype(tree.defs), decltype(pairs), decltype(all_related_rules), std::tuple<>, do_prettyprint>(tree.defs, pairs, all_related_rules, std::tuple<>());
}


/**
 * @brief Lexer configuration
 */
enum class LexerConfEnum : std::uint64_t
{
    Legacy = 0x0, /** Use legacy lexer without duplicate elements handling (default) */
    AdvancedLexer = 0x1, /** Use advanced lexer with duplicate terms handling */
    HandleDuplicates = 0x10, /** Advanced handling of duplicate elements at compile-time. Required for term ranges support and duplicate terms which are present in >2 rules at once */
    HandleDupInRuntime = 0x100, /** Move advanced handling of duplicate element to runtime lexer initialization */
};

template<std::uint64_t Conf>
class LexerConfig
{
public:
    constexpr explicit LexerConfig() = default;

    static constexpr std::uint64_t value() { return Conf; }

    template<LexerConfEnum value>
    [[nodiscard]] static constexpr bool flag() { return (Conf & static_cast<std::uint64_t>(value)) > 0; }
};

template<LexerConfEnum... Values>
constexpr auto mk_lexer_conf()
{
    constexpr std::uint64_t conf = (static_cast<const std::uint64_t>(Values) | ...);
    return LexerConfig<conf>();
}


template<class VStr, class TokenType, class RulesSymbol, class Conf>
constexpr auto make_lexer(const RulesSymbol& rules, Conf conf)
{
    if constexpr (conf.template flag<LexerConfEnum::AdvancedLexer>())
    {
        auto terms_cache = terms_tree_cache_factory(rules);
        auto terms_type_map = terms_type_map_factory<VStr, TokenType, conf.template flag<LexerConfEnum::HandleDuplicates>(), conf.template flag<LexerConfEnum::HandleDupInRuntime>()>(terms_cache);
        return Lexer<VStr, TokenType, std::decay_t<decltype(terms_type_map)>>(terms_type_map);
    } else {
        return LexerLegacy<VStr, TokenType>(rules);
    }
}

#endif //PREPROCESS_FACTORIES_H
