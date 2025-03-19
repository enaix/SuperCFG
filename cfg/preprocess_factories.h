//
// Created by Flynn on 19.03.2025.
//

#ifndef PREPROCESS_FACTORIES_H
#define PREPROCESS_FACTORIES_H

#include "cfg/helpers.h"
#include "cfg/preprocess.h"
#include "cfg/base.h"

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
        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
    }

    template<class SrcTuple, class NTerm, std::size_t... Ints>
    constexpr auto terms_map_descend_each(const SrcTuple& op, const NTerm& lhs, const std::integer_sequence<std::size_t, Ints...>)
    {
        return std::make_tuple(terms_map_descend_each_def(lhs, std::get<Ints>(op), [&](const auto&... args){ return terms_map_descend_each(args...);})...);
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
        return std::make_tuple(func(std::get<Ints>(op))...);
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
        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
    }

    // rr_tree_helpers
    template<class TSymbol, class TNTerm>
    constexpr bool rr_tree_is_nterm_in_rule(const TSymbol& symbol, const TNTerm& nterm)
    {
        // Descend over all operators and check if it's present in this rule definition
        if constexpr (is_nterm<TSymbol>())
        {
            return std::is_same_v<TSymbol, TNTerm>();
        }
        else if constexpr (is_operator<TSymbol>())
        {
            return rr_tree_process_op<0>(symbol);
        }
        else return false; // We need to gracefully handle this case
    }

    template<std::size_t depth, class TSymbol>
    constexpr bool rr_tree_process_op(const TSymbol& symbol)
    {
        const bool found = rr_tree_is_nterm_in_rule(std::get<depth>(symbol.terms));
        if (found) return true;

        if constexpr (depth + 1 < std::tuple_size_v<std::decay_t<decltype(symbol.terms)>>)
        {
            return found;
        } else {
            return rr_tree_process_op<depth + 1>(symbol);
        }
    }

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
            if constexpr (depth + 1 >= std::tuple_size_v<typename RulesSymbol::term_types_tuple>) return std::tuple<>(); // TODO cover this case when last term is the same
            else return rr_tree_iterate_over_rules<depth+1>(rules, nterm);
        } else {
            // Check if the element is in this definition
            const bool found = rr_tree_is_nterm_in_rule(rules, nterm);

            if constexpr (depth + 1 >= std::tuple_size_v<typename RulesSymbol::term_types_tuple>)
                return (found ? std::make_tuple(rule_nterm) : std::make_tuple<>());
            else
                return (found ? std::tuple_cat(rule_nterm, rr_tree_iterate_over_rules<depth+1>(rules, nterm)) : rr_tree_iterate_over_rules<depth+1>(rules, nterm));
        }
    }

    template<std::size_t depth, class RulesSymbol>
    constexpr auto rr_tree_for_symbol(const RulesSymbol& rules)
    {
        const auto& nterm = std::get<0>(std::get<depth>(rules.terms).terms); // key (nterm)
        const auto nterms = rr_tree_iterate_over_rules<0>(rules, nterm); // value (related definitions)

        if constexpr (depth + 1 >= std::tuple_size<typename RulesSymbol::term_types_tuple>()) return std::make_tuple(nterms);
        else return std::tuple_cat(nterms, rr_tree_for_symbol<depth+1>(rules));
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
    auto nterms = cfg_helpers::symbols_ht_find_nterms(rules);
    auto terms = tuple_unique(cfg_helpers::symbols_ht_find_terms(rules));

    return SymbolsHashTable<TokenType, decltype(terms), decltype(nterms)>(terms, nterms);
}


template<class RulesSymbol>
auto reverse_rules_tree_factory(const RulesSymbol& rules)
{
    typename RulesSymbol::term_ptr_tuple defs(rules.get_ptr_tuple());
    const auto nterms = cfg_helpers::rr_tree_for_symbol<0>(rules);

    return ReverseRuleTree(defs, nterms);
}

#endif //PREPROCESS_FACTORIES_H
