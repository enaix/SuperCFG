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
    CtxTODO<TRules> prefix_todo;
    CtxTODO<TRules> postfix_todo;

    CtxTODO<TRules> prefix; // non-ambiguous prefix match (we do not need the whole array)
    CtxTODO<TRules> postfix; //

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
        const auto& [related_types, fix_limits] = term2nterms.get(symbol);
        tuple_each_or_return(related_types, [&](std::size_t i, const auto& elem){
            using max_t = std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>;
            const auto& [rule, fix] = elem;

            // Get the prefix and postfix positions
            const auto [pre, post] = fix; // fix only contains info for the nterms
            const auto [max_pre, min_post] = fix_limits; // Get max prefix and min postfix in this rule
            // Max pre/postfix -> ok, we successfully resolved the rule
            // If there are no more matches even though we haven't reached the end of the fix, we apply context

            // Get index of rule
            constexpr std::size_t rule_id = get_ctx_index<0, std::decay_t<decltype(rule)>>();
            // Get ctx for current rule
            const std::size_t ctx = context[rule_id];

            // Check if current rule can be in context

            // Try to check if the prefix matches
            // I guess that we do not need to perform full descend() check
            auto is_in_prefix = [&](const std::size_t at, const std::size_t len){
                //std::size_t parsed = descend(at, std::get<1>(nterms_map.get(rule)->terms));
                // Check if the last symbol is in the correct prefix pos
                if constexpr (is_nterm<TSymbol>())
                    return (at == pre); // Something like this
                else
                {
                    // Check the lexical range

                }

                /*if (parsed >= len) // We can theoretically reduce everything up to this symbol OR there is no match at all
                {
                    //context[ctx_pos]++; // Successful match
                    // last_ctx_pos = ctx_pos;
                    return true;
                }
                return false;*/
            };

            auto handle_match_and_todo = [&](const auto& todo){

            };


            // Is in prefix
            if constexpr (!std::is_same_v<std::decay_t<decltype(pre)>, max_t>)
            {
                // Currently not in ctx guessing mode
                // Descend
                if (pre >= stack_size) return false; // cannot fit in current stack
                const std::size_t at = stack_size - 1 - pre;
                if (perform_check_at_pos(at, pre))
                {
                    // Found match

                    // Check if context exists at current ctx level
                    if (ctx_pos[rule_id][ctx] == max_t())
                    {
                        // Symbol has not been matched yet
                    } else {
                        // Symbol already matched, we need to check for recursive ctx
                        // Also need to handle CtxTODO

                    }
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
                    if (pre >= stack_size) return false; // cannot fit in current stack
                    const std::size_t stack_i = stack_size - 1 - pre;
                    if (perform_check_at_pos(stack_i, pre))
                    {
                        // Found match
                    }
                } else {
                    // Context exists, use it as a starting pos
                    // Be careful with ctx level start
                    const std::size_t at = ctx_pos[rule_id][ctx];
                    if (at >= stack_size) return false;
                    if (perform_check_at_pos(at, stack_size - at)) // (stack_size - at) is the number of symbols up to the stack end
                    {
                        // Found match
                    }
                }
            }
        });
    }
};



#endif //CONTEXT_H
