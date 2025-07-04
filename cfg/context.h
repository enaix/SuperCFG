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
protected:
    std::array<std::size_t, std::tuple_size_v<TRules>> todo;
    std::size_t n; // size of _todo
    std::vector<std::size_t> cached_idx; // last added rule index

public:
    constexpr CtxTODO() : n(0), cached_idx(std::numeric_limits<std::size_t>::max()) {}

    [[nodiscard]] std::size_t size() const { return n; }

    void reset()
    {
        n = 0;
        cached_idx.clear();
        todo.fill(std::numeric_limits<std::size_t>::max());
    }

    const std::size_t& operator[](const std::size_t i) { return todo[i]; }

    void remove(const std::size_t rule_id)
    {
        todo[rule_id] = std::numeric_limits<std::size_t>::max();
        n--;
        cached_idx.pop_back();
    }

    void add(const std::size_t rule_id, const std::size_t pos)
    {
        todo[rule_id] = pos;
        cached_idx.push_back(rule_id);
        n++;
    }

    [[nodiscard]] std::size_t last_added() const
    {
        return cached_idx.front();
    }
};


class CtxMeta
{
public:
    std::size_t rule_id;
    std::size_t fix;

    constexpr CtxMeta() : rule_id(std::numeric_limits<std::size_t>::max()), fix(std::numeric_limits<std::size_t>::max()) {}

    void reset()
    {
        rule_id = std::numeric_limits<std::size_t>::max();
        fix = std::numeric_limits<std::size_t>::max();
    }

    void set(const std::size_t rule, const std::size_t pos)
    {
        rule_id = rule;
        fix = pos;
    }

    bool empty() { return rule_id == std::numeric_limits<std::size_t>::max(); }
};


template<class TMatches, class NTermsPosPairs, class TermsPosPairs, class TRules, class TTerms, class FullRRTree>
class ContextManager
{
public:
    std::array<std::size_t, std::tuple_size_v<TRules>> context;
    std::array<std::vector<std::size_t>, std::tuple_size_v<TRules>> ctx_pos; // Positions of context start at each ctx level
    CtxTODO<TRules> prefix_todo;
    CtxTODO<TRules> postfix_todo;

    CtxMeta prefix; // non-ambiguous prefix match (we do not need the whole array)
    CtxMeta postfix; // ditto

    TMatches matches;
    NTermsPosPairs pos_nterm;
    TermsPosPairs pos_term;
    TRules rules; // all rules
    TTerms t_terms; // all terms
    FullRRTree rr_all;
    std::size_t last_ctx_pos;

    constexpr ContextManager(const TMatches& m, const NTermsPosPairs& p_nterm, const TermsPosPairs& p_term, const TRules& r, const TTerms& all_terms, const FullRRTree& rr_all) : matches(m), pos_nterm(p_nterm), pos_term(p_term), rules(r), t_terms(all_terms), rr_all(rr_all) {}

    /**
     * @brief Reset the context of the array. Should be performed at the start of parsing
     */
    void reset_ctx()
    {
        context.fill(0);
        for (const auto& vec : ctx_pos)
            vec.assign(1, std::numeric_limits<std::size_t>::max()); // Set ctx pos to null at first ctx level
        last_ctx_pos = std::numeric_limits<std::size_t>::max();
        prefix_todo.reset();
        postfix_todo.reset();
        prefix.reset();
        postfix.reset();
    }

    /**
     * @brief Consume the next token and perform context analysis. Returns false on ambiguity
     */
    template<class TSymbol, class TermsTypes, class NTermsMap>
    bool next(const TSymbol& symbol, std::size_t stack_size, const TermsTypes& term2nterms, const NTermsMap& nterms_map, auto descend)
    {

        const auto& [related_types, fix_limits] = get_pos(symbol); /* fetch the value from *PosPairs */;

        tuple_each_or_return(related_types, [&](std::size_t i, const auto& elem){
            using max_t = std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>;
            const auto& [rule, fix] = elem;

            // Get the prefix and postfix positions
            const auto [pre, post_dist] = fix; // fix only contains info for the nterms
            const auto [max_pre, min_post] = fix_limits; // Get max prefix and min postfix in this rule
            // Max pre/postfix -> ok, we successfully resolved the rule
            const auto post = min_post - post_dist; // Convert post from the distance to the end to an id
            // If there are no more matches even though we haven't reached the end of the fix, we apply context

            // Get index of rule
            constexpr std::size_t rule_id = get_ctx_index<0, std::decay_t<decltype(rule)>>();
            // Get ctx for current rule
            // const std::size_t ctx = context[rule_id];

            // Check if current rule can be in context

            /*
             * Ctx resolver pseudocode:
             * limits <- fix_minmax  // pre/postfix limits
             * new_todo <- {}
             * each rr <- related_type:
             *   pre, post <- fix
             *   // each pre, post:
             *   at <- stack_pos - _todo[rr]  // `at` is the relative position in pre/post. if _todo[rr] and _fix[rr] are empty, no check is performed OR we check all symbols from the beginning
             *                                // if _fix[rr] exists, then we only check when we reach the end. right now we better check and add asserts
             *   if at == pre/post:  // check if the last stack symbol with relative pos `at` is actually on the position pre/post
             *     if new_todo != null:
             *       _todo[rr] += at_pre/at_post  // increment _todo, at_pre/post indicates that we're checking prefix or postfix
             *     else:
             *       new_todo[rr] += at_pre/at_post
             *
             *    if len(new_todo) == 1:  // we need to check this for both prefix and postfix
             *      ctx++/--
             *      // at_pre, at_post:
             *      _fix[rr] ++  // early ctx application check (save info that we have already applied ctx)
             *    else:
             *
             */

            auto check_ctx = [&](){
                if constexpr (std::tuple_size_v<std::decay_t<FullRRTree>> > 0)
                {
                    const auto& idx = get_rr_all(rule);
                    for (const std::size_t pos : idx)  // rules where it cannot be present
                    {
                        if (context[pos] > 0) return false;
                    }
                }
                return true;
            };


            // Is in prefix
            if constexpr (!std::is_same_v<std::decay_t<decltype(pre)>, max_t>)
            {
                // we can also perform additional end-of-stack check (does the prefix+postfix fit?)

                if (prefix.empty() || postfix.empty())
                {
                    // we're currently matching non-ambiguous ctx
                    // applied prefix or postfix are blocking: no new matches can happen
                    if (prefix.rule_id == rule_id)
                    {
                        // use _fix as the starting pos
                        if (stack_size - 1 - prefix.fix != pre)
                        {
                            // we have already applied the ctx, unexpected behavior
                            // ASSERT
                        }
                        if (stack_size - 1 - prefix.fix == max_pre)
                        {
                            // we reached the end
                            prefix.reset();
                        }
                    }
                }
                else if (prefix_todo[rule_id] != max_t())
                {
                    // use _todo as the starting pos
                    // TODO add optional (if pre == max_pre -> apply)
                    if (stack_size - 1 - prefix_todo[rule_id] == pre)
                    {
                        // FOUND
                        // nothing to do - prefix is already in _todo
                    } else {
                        // candidate dropped out
                        prefix_todo.remove(rule_id);
                    }
                } else {
                    // first match

                    if (pre == 0)
                    {
                        if (check_ctx())
                        {
                            // rule can exist in current ctx
                            // nothing else to check
                            // FOUND
                            prefix_todo.add(rule_id, stack_size - 1);
                        }
                    } else {
                        // we need different logic for handling cases when prefix != 0
                        // permissive handler:
                        if (pre < stack_size)  // sanity check
                            prefix_todo.add(rule_id, stack_size - 1 - pre);
                    }
                }
            }

            // Is in postfix
            if constexpr (!std::is_same_v<std::decay_t<decltype(post)>, max_t>)
            {
                // we can also perform additional end-of-stack check (does the prefix+postfix fit?)
                // also we should check that the prefix rule matches (link prefixes and postfixes together)

                if (prefix.empty() || postfix.empty())
                {
                    // we're currently matching non-ambiguous ctx
                    // applied prefix or postfix are blocking: no new matches can happen
                    if (postfix.rule_id == rule_id)
                    {
                        // use _fix as the starting pos
                        if (postfix.fix + post != stack_size - 1)
                        {
                            // we have already applied the ctx, unexpected behavior
                            // ASSERT
                        }
                        if (post_dist == 0)
                        {
                            // we reached the end
                            // FOUND
                            postfix.reset();
                        }
                    }
                }
                else if (postfix_todo[rule_id] != max_t())
                {
                    // use _todo as the starting pos
                    if (postfix_todo[rule_id] + post == stack_size - 1)
                    {
                        // FOUND
                        // nothing to do - prefix is already in _todo
                    } else {
                        // candidate dropped out
                        postfix_todo.remove(rule_id);
                    }
                } else {
                    // first match

                    if (post == 0)
                    {
                        // FOUND
                        postfix_todo.add(rule_id, stack_size - 1);
                    } else {
                        // we need different logic for handling cases when prefix != 0
                        // permissive handler:
                        if (post < stack_size)  // sanity check (we need to account for prefix overlap)
                            postfix_todo.add(rule_id, stack_size - 1 - post);
                    }
                }
            }
        });

        // _todo solver
        if (prefix_todo.size() + postfix_todo.size() == 1)
        {
            // No ambiguities!
            // find non-empty rule
            if (prefix_todo.size() == 1)
            {
                std::size_t match_id = prefix_todo.last_added();
                context[match_id]++;
                prefix.set(match_id, prefix_todo[match_id]);
                prefix_todo.remove(match_id);
            } else {
                std::size_t match_id = postfix_todo.last_added();
                postfix.set(match_id, postfix_todo[match_id]);
                postfix_todo.remove(match_id);
            }
        }
        if (postfix.fix == stack_size - 1)
        {
            // We exit ctx now!
            context[postfix.rule_id]--;
        }
        return prefix_todo.size() + postfix_todo.size() == 0;
    }

    template<class TSymbol>
    constexpr auto& get_pos(const TSymbol& symbol) const
    {
        if constexpr (is_nterm<std::decay_t<TSymbol>>)
            return do_get_nterm_pos<0>(symbol);
        else
            return do_get_term_pos<0>(symbol);
    }

protected:
    template<std::size_t depth, class TSymbol>
    constexpr auto do_get_nterm_pos(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TMatches>, "NTerm type not found");
        // Get the corresponding NTermsPosPairs element
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::tuple_element_t<depth, TRules>>)
        {
            return std::get<depth>(pos_nterm);
        } else {
            return do_get<depth + 1>(symbol);
        }
    }

    template<std::size_t depth, class TSymbol>
    constexpr auto do_get_term_pos(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TMatches>, "Term type not found");
        // Get the corresponding NTermsPosPairs element
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::tuple_element_t<depth, TTerms>>)
        {
            return std::get<depth>(pos_term);
        } else {
            return do_get<depth + 1>(symbol);
        }
    }
};


template<bool do_prettyprint, bool do_context_check, class RRTree, class NTermsMap, class TTermsTypeMap, class THeuristicsPre>
constexpr auto make_ctx_manager(const RRTree& tree, const NTermsMap& nterms2defs, const TTermsTypeMap& terms_map, const THeuristicsPre& h_pre)
{
    const auto pairs_nt = cfg_helpers::ctx_get_nterm_match<0>(tree.defs, tree.tree, nterms2defs);
    const auto pairs_t = cfg_helpers::ctx_get_term_match<0>(terms_map.terms, terms_map.nterms, nterms2defs);
    return ContextManager<decltype(tree.defs), decltype(pairs_nt), decltype(pairs_t), decltype(h_pre.unique_rr), decltype(terms_map.terms), decltype(h_pre.full_rr), do_prettyprint>(tree.defs, pairs_nt, pairs_t, h_pre.unique_rr, terms_map.terms, h_pre.full_rr);
}



#endif //CONTEXT_H
