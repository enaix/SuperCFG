//
// Created by Flynn on 11.05.2025.
//

#ifndef CONTEXT_H
#define CONTEXT_H

#include "cfg/base.h"
#include "cfg/helpers.h"
#include "cfg/hashtable.h"
#include "cfg/preprocess.h"
#include "cfg/preprocess_factories.h"



template<class TMatches>
class CtxTODO
{
protected:
    std::array<std::size_t, std::tuple_size_v<TMatches>> todo;
    std::size_t n; // size of _todo
    std::vector<std::size_t> cached_idx; // last added rule index

public:
    constexpr CtxTODO() : n(0), cached_idx() {}

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
    std::array<std::size_t, std::tuple_size_v<TMatches>> context;
    std::array<std::vector<std::size_t>, std::tuple_size_v<TMatches>> ctx_pos; // Positions of context start at each ctx level
    CtxTODO<TMatches> prefix_todo;
    CtxTODO<TMatches> postfix_todo;

    CtxMeta prefix; // non-ambiguous prefix match (we do not need the whole array)
    CtxMeta postfix; // ditto

    TMatches matches;
    NTermsPosPairs pos_nterm;
    TermsPosPairs pos_term;
    TRules rules; // all rules
    TTerms t_terms; // all terms
    FullRRTree rr_all;

    constexpr ContextManager(const TMatches& m, const NTermsPosPairs& p_nterm, const TermsPosPairs& p_term, const TRules& r, const TTerms& all_terms, const FullRRTree& rr_all) : matches(m), pos_nterm(p_nterm), pos_term(p_term), rules(r), t_terms(all_terms), rr_all(rr_all) {}

    /**
     * @brief Reset the context of the array. Should be performed at the start of parsing
     */
    void reset_ctx()
    {
        context.fill(0);
        for (auto& vec : ctx_pos)
            vec.assign(1, std::numeric_limits<std::size_t>::max()); // Set ctx pos to null at first ctx level
        prefix_todo.reset();
        postfix_todo.reset();
        prefix.reset();
        postfix.reset();
    }

    /**
     * @brief Consume the next token and perform context analysis. Returns false on ambiguity
     */
    template<class GSymbol, class SymbolsHT>
    bool next(const GSymbol& g_symbol, std::size_t stack_size, const SymbolsHT& symbols_ht, auto& prettyprinter)
    {
        // Visit the explicit type. Note that it may return either an NTerm or
        g_symbol.with_types(symbols_ht, [&](const auto& symbol){
            const auto& [related_types, fix_limits] = get_pos(symbol); /* fetch the value from *PosPairs */;

            tuple_each(related_types, [&,fix_limits](std::size_t i, const auto& elem){
                using max_t = std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>;
                const auto& [rule, fix] = elem;
                //const auto& fix = std::get<i>(fix_limits);

                // Get the prefix and postfix positions
                const auto [pre, post_dist] = fix;
                const auto [max_pre, min_post] = fix_limits; // Get max prefix and min postfix in this rule
                // Max pre/postfix -> ok, we successfully resolved the rule
                // TODO fix fix_limits - they are calculated differently for nterms and terms
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




                // TODO add end of input marker to ONLY consider full postfixes
                // Also if we reduce some ambiguity and some CtxTODOs are resolved, we should re-run the previous checks
                // TODO Do we actually handle CtxTODOs checks correctly with sequential next() calls?
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
                            // prefix is non-empty and is the same as the current rule
                            // use _fix as the starting pos
                            if (stack_size - 1 - prefix.fix != pre) [[unlikely]] // same rule, different symbol - looks counterintuitive
                            {
                                // we have already applied the ctx, unexpected behavior
                                if (stack_size - 1 - prefix.fix == pre)
                                {
                                    prettyprinter.guru_meditation("expected static prefix to match with runtime, got a mismatch", __FILE__, __LINE__);
                                    assert(stack_size - 1 - prefix.fix != pre && "next() : guru meditation : expected static prefix to match with runtime, got a mismatch");
                                }

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
                            if (check_ctx(rule))
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
                if constexpr (!std::is_same_v<std::decay_t<decltype(post_dist)>, max_t>)
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
                                prettyprinter.guru_meditation("expected static postfix to match with runtime, got a mismatch", __FILE__, __LINE__);
                                assert(postfix.fix + post != stack_size - 1 && "next() : guru meditation : expected static postfix to match with runtime, got a mismatch");
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
            }); // each possible position
        }); // each type candidate

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
        /*if (postfix.fix == stack_size - 1)
        {
            // We exit ctx now!
            // context[postfix.rule_id]--;
        }*/
        return prefix_todo.size() + postfix_todo.size() == 0;
    }

    /**
     * @brief Check if a match can exist in current ctx
     * @param match Match candidate
     */
    template<class TSymbol>
    constexpr bool check_ctx(const TSymbol& match)
    {
        if constexpr (std::tuple_size_v<std::decay_t<FullRRTree>> > 0)
        {
            const auto& idx = get_rr_all(match);
            for (const std::size_t pos : idx)  // rules where it cannot be present
            {
                if (context[pos] > 0)
                    return false;
            }
        }
        return true;
    }

    /**
     * @brief Update the context for the reduced symbol. This function should be called on each successful reduce operation
     * @param match Chosen match candidate
     * @param stack_size Size of the stack before the reduction
     */
    template<class TSymbol>
    constexpr bool apply_reduce(const TSymbol& match, std::size_t stack_size, auto& prettyprinter = NoPrettyPrinter())
    {
        if constexpr (std::tuple_size_v<std::decay_t<FullRRTree>> > 0)
        {
            constexpr std::size_t index = get_ctx_index<0, std::decay_t<TSymbol>>();
            if (postfix.rule_id == index)
            {
                // Context id matches
                if (postfix.fix != stack_size - 1) [[unlikely]]
                {
                    // The symbol is reduced
                    prettyprinter.guru_meditation("match candidate reduced in the illegal postfix position", __FILE__, __LINE__);
                    assert(postfix.fix == stack_size - 1 && "apply_reduce() : guru meditation : match candidate reduced in the illegal postfix position");
                }

                if (context[index] == 0)
                {
                    prettyprinter.guru_meditation("empty context with non-empty postfix", __FILE__, __LINE__);
                    assert(context[index] > 0 && "apply_reduce() : guru meditation : empty context with non-empty postfix");
                }
                context[index]--;
                postfix.reset();
            }
        }
        return true;
    }


protected:
    template<class TSymbol>
    constexpr auto& get_pos(const TSymbol& symbol) const
    {
        if constexpr (is_nterm<std::decay_t<TSymbol>>())
            return do_get_nterm_pos<0>(symbol);
        else
            return do_get_term_pos<0>(symbol);
    }

    template<std::size_t depth, class TSymbol>
    [[nodiscard]] static constexpr std::size_t get_ctx_index()
    {
        if constexpr (depth >= std::tuple_size_v<TMatches>)
            return std::numeric_limits<std::size_t>::max(); // No such symbol
        else
        {
            if constexpr (std::is_same_v<std::decay_t<std::tuple_element_t<depth, TMatches>>, std::decay_t<TSymbol>>)
                return depth;
            else
                return get_ctx_index<depth+1, TSymbol>();
        }
    }

    template<std::size_t depth, class TSymbol>
    constexpr auto& do_get_nterm_pos(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TMatches>, "NTerm type not found");
        // Get the corresponding NTermsPosPairs element
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::decay_t<std::tuple_element_t<depth, TMatches>>>)
        {
            return std::get<depth>(pos_nterm);
        } else {
            return do_get_nterm_pos<depth + 1>(symbol);
        }
    }

    template<std::size_t depth, class TSymbol>
    constexpr auto& do_get_term_pos(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TTerms>, "Term type not found");
        // Get the corresponding NTermsPosPairs element
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::tuple_element_t<depth, TTerms>>)
        {
            return std::get<depth>(pos_term);
        } else {
            return do_get_term_pos<depth + 1>(symbol);
        }
    }

    template<class TSymbol>
    constexpr auto get_rr_all(const TSymbol& symbol) const
    {
        return do_get_rr_all<0>(symbol);
    }

    template<std::size_t depth, class TSymbol>
    constexpr auto do_get_rr_all(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TMatches>, "NTerm type not found");
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::decay_t<typename std::tuple_element_t<depth, TMatches>>>)
        {
            return std::get<depth>(rr_all);
        } else {
            return do_get_rr_all<depth + 1>(symbol);
        }
    }
};


template<class RulesDef, class RRTree, class NTermsMap, class TTermsTypeMap, class THeuristicsPre>
constexpr auto make_ctx_manager(const RulesDef& rules, const RRTree& tree, const NTermsMap& nterms2defs, const TTermsTypeMap& terms_tmap, const THeuristicsPre& h_pre, auto& prettyprinter)
{
    const auto pairs_nt = cfg_helpers::ctx_get_nterm_match<0>(tree.defs, tree.tree, nterms2defs);
    const auto pairs_t = cfg_helpers::ctx_get_term_match<0>(terms_tmap.terms, terms_tmap.nterms, nterms2defs);
    const auto defs_flatten = tuple_morph([&]<std::size_t i>(const auto& src){ return std::get<0>(std::get<i>(tree.defs).terms); }, tree.defs);
    //static_assert(std::tuple_size_v<std::decay_t<decltype(tree.defs)>> == std::tuple_size_v<std::decay_t<decltype(pairs_nt)>>, "bad pairs_nt");
    prettyprinter.init_ctx_classes(defs_flatten, h_pre.unique_rr, terms_tmap.terms, pairs_nt, pairs_t);
    return ContextManager<decltype(defs_flatten), decltype(pairs_nt), decltype(pairs_t), decltype(h_pre.unique_rr), decltype(terms_tmap.terms), decltype(h_pre.full_rr)>(defs_flatten, pairs_nt, pairs_t, h_pre.unique_rr, terms_tmap.terms, h_pre.full_rr);
}



#endif //CONTEXT_H
