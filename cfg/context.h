//
// Created by Flynn on 11.05.2025.
//

#ifndef CONTEXT_H
#define CONTEXT_H

#include "cfg/base.h"
#include "cfg/helpers.h"
#include "cfg/hashtable.h"



template<class TRules>
class CtxTODO
{
public:
    std::array<std::size_t, std::tuple_size_v<TRules>> todo;
    std::size_t n;

    constexpr CtxTODO() : n(0) {}
};


template<class TMatches, class RulesPosPairs, class TRules, class FullRRTree>
class ContextManager
{
public:
    std::array<std::size_t, std::tuple_size_v<TRules>> context;
    std::array<std::vector<std::size_t>, std::tuple_size_v<TRules>> ctx_pos; // Positions of context start at each ctx level
    CtxTODO<TRules> prefix;
    CtxTODO<TRules> postfix;

    TMatches matches;
    RulesPosPairs pos;
    TRules rules;
    FullRRTree rr_all;
    std::size_t last_ctx_pos;

    constexpr ContextManager(const TMatches& m, const RulesPosPairs& p, const TRules& r, const FullRRTree& rr_all) : matches(m), pos(p), rules(r), rr_all(rr_all) {}

    /**
     * @brief Reset the context of the array. Should be performed at the start of parsing
     */
    void reset_ctx()
    {
        context.fill(0);
        for (const auto& vec : ctx_pos)
            vec.assign(1, std::numeric_limits<std::size_t>::max()); // Set ctx pos to null at first ctx level
        last_ctx_pos = std::numeric_limits<std::size_t>::max();
    }

    template<class TSymbol, class TermsTypes, class NTermsMap>
    bool next(const TSymbol& symbol, std::size_t stack_size, const TermsTypes& term2nterms, const NTermsMap& nterms_map, auto descend)
    {
        // HandleTODO

        // CtxTODO is empty:
        const auto& related_types = term2nterms.get(symbol);
        tuple_morph_each(related_types, [&](std::size_t i, const auto& elem){
            using max_t = std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>;
            const auto& [rule, fix] = elem;

            // Get the prefix and postfix positions
            const auto [pre, post] = fix;
            // Get index of rule
            constexpr std::size_t rule_id = get_ctx_index<0, std::decay_t<decltype(rule)>>();
            // Get ctx for current rule
            const std::size_t ctx = context[rule_id];

            // Check if current rule can be in context

            // Try to reduce symbol
            auto perform_check_at_pos = [&](const std::size_t symbol_pos){
                // Out of bounds check: the rule cannot fit in current stack
                if (symbol_pos >= stack_size)
                    return false; // Continue

                std::size_t stack_i = stack_size - 1 - symbol_pos;
                std::size_t parsed = descend(stack_i, std::get<1>(nterms_map.get(rule)->terms));

                if (parsed >= symbol_pos) // We can theoretically reduce everything up to this symbol OR there is no match at all
                {
                    //context[ctx_pos]++; // Successful match
                    // last_ctx_pos = ctx_pos;
                    return true;
                }
                return false;
            };


            // Is in prefix
            if constexpr (!std::is_same_v<std::decay_t<decltype(pre)>, max_t>)
            {
                // Check if context exists at current ctx level
                if (ctx_pos[rule_id][ctx] == max_t())
                {
                    // Currently not in ctx guessing mode
                    // Descend
                    if (perform_check_at_pos(pre))
                    {
                        // Found match
                    }
                } else {
                    // Symbol already matched, we only need to check for recursive ctx
                    // Also need to handle CtxTODO
                }
            }

            // Is in postfix
            if constexpr (!std::is_same_v<std::decay_t<decltype(post)>, max_t>)
            {
                // No context exists at current ctx level
                if (ctx_pos[rule_id][ctx] == max_t())
                {
                    // Not in prefix -> cannot obtain starting pos
                    if constexpr (!std::is_same_v<std::decay_t<decltype(pre)>, max_t>)
                        return false; // Not found

                    // Use pre as a starting pos
                    if (perform_check_at_pos(pre))
                    {
                        // Found match
                    }
                } else {
                    // Context exists, use it as a starting pos
                    // Be careful with ctx level start

                }
            }
        });
    }
};



#endif //CONTEXT_H
