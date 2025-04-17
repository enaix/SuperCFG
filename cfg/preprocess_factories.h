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
        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
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

    template<class TSymbol>
    constexpr auto terms_cache_each_elem(const TSymbol& symbol)
    {
        if constexpr (is_operator<TSymbol>())
        {
            // Morph each symbol s into each_elem(s) and concat
            return tuple_morph_each<true>(symbol.terms, [&](std::size_t i, const auto& s){ return terms_cache_each_elem(s); });
        }
        else if constexpr (is_term<TSymbol>())
        {
            return std::make_tuple(symbol);
        } else return std::tuple<>();
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
        if constexpr (tuple_contains_v<TSymbol, std::decay_t<decltype(std::get<i>(cache.terms))>>)
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

template<class VStr, class TokenType, class TypesCache>
auto terms_type_map_factory(const TypesCache& cache)
{
    // Find duplicated terms
    /*auto dup = tuple_apply_pairwise<true>(cache.all_terms, []<class ElemA, class ElemB>(const ElemA& lhs, const ElemB& rhs){
        // Add check for range
        if constexpr (std::is_same_v<std::decay_t<ElemA>, std::decay_t<ElemB>>)
            return lhs;
        else
            return std::tuple<>();
    });*/
    //auto terms_map = std::tuple_cat(cfg_helpers::find_term_in_cache_all<0>(dup), cfg_helpers::find_term_in_cache_single<>())

    // It's faster to find all related elements in the cache
    // We should check intersecting elements in range
    auto terms_map = tuple_morph_each(cache.all_terms, [&](std::size_t i, const auto& elem){ return cfg_helpers::find_term_in_cache_all<0>(elem, cache); });
    return TermsTypeMap<VStr, TokenType, std::decay_t<decltype(cache.all_terms)>, std::decay_t<decltype(terms_map)>>(cache.all_terms, terms_map);
}


enum class LexerConfEnum : std::uint64_t
{
    Legacy = 0x0,
    AdvancedLexer = 0x1,
};

template<std::uint64_t Conf>
class LexerConfig
{
public:
    constexpr explicit LexerConfig() {}

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
        auto terms_type_map = terms_type_map_factory<VStr, TokenType>(terms_cache);
        return Lexer<VStr, TokenType, std::decay_t<decltype(terms_type_map)>>(terms_type_map);
    } else {
        return LexerLegacy<VStr, TokenType>(rules);
    }
}

#endif //PREPROCESS_FACTORIES_H
