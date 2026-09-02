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

    const std::size_t& operator[](const std::size_t i) const { return todo[i]; }

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
    CtxMeta(std::size_t rule, std::size_t pos) : rule_id(rule), fix(pos) {}

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

    bool empty() const { return rule_id == std::numeric_limits<std::size_t>::max(); }
};


template<class TMatches, class NTermsPosPairs, class TermsPosPairs, class TRules, class TTerms, class RRTree, class FullRRTree, class FixLimits>
class ContextManager
{
public:
    std::array<std::size_t, std::tuple_size_v<TMatches>> context;
    // TODO either implement or remove this
    std::array<std::vector<std::size_t>, std::tuple_size_v<TMatches>> ctx_pos; // Positions of context start at each ctx level
    CtxTODO<TMatches> prefix_todo;
    CtxTODO<TMatches> postfix_todo;

    std::vector<CtxMeta> prefix; // non-ambiguous prefix match (we may have multiple matches)
    std::vector<CtxMeta> postfix; // ditto
    std::size_t starting_symbol; // index of the starting nterm (needed for restricted ctx check)

    TMatches matches;
    NTermsPosPairs pos_nterm;
    TermsPosPairs pos_term;
    TRules rules; // all rules
    TTerms t_terms; // all terms
    RRTree rr_tree;
    FullRRTree rr_all;
    FixLimits limits;

    constexpr ContextManager(const TMatches& m, const NTermsPosPairs& p_nterm, const TermsPosPairs& p_term, const TRules& r, const TTerms& all_terms, const RRTree& rr_tree, const FullRRTree& rr_all, const FixLimits& limits) : matches(m), pos_nterm(p_nterm), pos_term(p_term), rules(r), t_terms(all_terms), rr_tree(rr_tree), rr_all(rr_all), limits(limits) {}

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
        prefix.clear();
        postfix.clear();
        starting_symbol = std::numeric_limits<std::size_t>::max();
    }

    template<class TSymbol>
    void set_starting_symbol(const TSymbol& nterm)
    {
        constexpr std::size_t index = get_ctx_index<0, std::decay_t<TSymbol>>();
        starting_symbol = index;
    }

    /**
     * @brief Consume the next token and perform context analysis. Returns false on ambiguity
     */
    template<class GSymbol, class SymbolsHT>
    bool next(const GSymbol& g_symbol, const std::vector<GSymbol>& stack, const SymbolsHT& symbols_ht, auto& prettyprinter)
    {
        std::size_t stack_size = stack.size();
        // Visit the explicit type. Note that it may return either an NTerm or
        g_symbol.with_types(symbols_ht, [&](const auto& symbol){
            const auto& related_types = get_pos(symbol); /* fetch the value from *PosPairs */;

            tuple_each(related_types, [&](std::size_t i, const auto& elem){
                using max_t = std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>;
                const auto& [rule, fix] = elem;
                //const auto& fix = std::get<i>(fix_limits);

                // Get index of rule
                constexpr std::size_t rule_id = get_ctx_index<0, std::decay_t<decltype(rule)>>();

                // Get the prefix and postfix positions
                const auto [pre_elems, post_dist_elems] = fix;
                const auto [max_pre, min_post] = std::get<rule_id>(limits); // Get max prefix and min postfix in this rule
                                                                            // MAY BE SIZE_T_MAX
                // Max pre/postfix -> ok, we successfully resolved the rule

                // If there are no more matches even though we haven't reached the end of the fix, we apply context

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
                tuple_each(pre_elems, [&](std::size_t k, const auto& pre){
                // Is in prefix
                if constexpr (!std::is_same_v<std::decay_t<decltype(pre)>, max_t>)  // check that prefix for the rule exists
                {
                    // we can also perform additional end-of-stack check (does the prefix+postfix fit?)

                    for (int j = prefix.size() - 1; j >= 0; j--)  // TODO verify that j.fix is always decreasing
                    {
                        // we're currently matching non-ambiguous ctx, this code handles the ongoing match
                        // applied prefix or postfix are non-blocking: new matches can happen (in case if we have nested symbols)
                        if (prefix[j].rule_id == rule_id)
                        {
                            // prefix is non-empty and is the same as the current rule
                            // use _fix as the starting pos
                            // TODO disabling this check introduced another problem that existing prefix != this rule actually exists (see rule member)
                            // ! This check is incorrect, since the partially reduced prefix may mismatch at any point
                            // TODO add verification that all prefix symbols were visited, or that the prefix fully matches
                            if (stack_size - 1 - prefix[j].fix != pre) [[unlikely]] // same rule, different symbol - looks counterintuitive
                            {
                                // we have already applied the ctx, unexpected behavior
                                prettyprinter.guru_meditation("expected static prefix to match with runtime, got a mismatch", __FILE__, __LINE__);
                                assert(stack_size - 1 - prefix[j].fix == pre && "next() : guru meditation : expected static prefix to match with runtime, got a mismatch");
                            }
                            // TODO this seems to be incorrect if the prefix has duplicate symbols. We need to check for the whole path
                            if (stack_size - 1 - prefix[j].fix == max_pre)
                            {
                                // we reached the end
                                prefix.erase(prefix.begin() + j); // ok to move j
                            }
                        }
                    }

                    if (prefix_todo[rule_id] != max_t())
                    {
                        // Check which candidates should be dropped from prefix_todo
                        // use _todo as the starting pos
                        // TODO (important) This way of checking for bad todo candidates is incorrect, we should check the full rr tree instead
                        // TODO add optional (if pre == max_pre - 1 -> apply)
                        // TODO check the case where pre == 0

                        // GREEDY RESOLVER - allow everything until we find the last element of prefix
                        /*auto fix_todo = prefix_todo[rule_id];
                        if (stack_size - 1 - fix_todo == max_pre)
                        {
                            // Pick the first one greedily. Works fine in an online parse, if the prefixes don't overlap
                            for (std::size_t k = 0; k < prefix_todo.size(); k++)
                            {
                                // We also must validate that the LHS is actually of this symbol (which makes the current implementation totally broken)
                                // Erase all todos with the same starting pos, since we apply the current one right now. This doesn't work in complex cases
                                if (k != rule_id && prefix_todo[k] == fix_todo)
                                    prefix_todo.remove(rule_id);
                                // Since we have applied the current symbol as prefix, apply the context
                                context[rule_id]++;
                                prefix_todo.remove(rule_id);
                            }
                        }*/

                        // LITERAL RESOLVER - drop on single inconsistency, expects prefixes to be literal
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

                        // do we need to check for anything else?
                        if (pre == 0)
                        {
                            if (do_check_ctx(rule, true))
                            {
                                // rule can exist in current ctx
                                // nothing else to check
                                // FOUND
                                prefix_todo.add(rule_id, stack_size - 1);
                                // TODO prefixes of length 1 won't reduce, since we're not performing an additional check
                            } // else discard this match
                        } else {
                            // we need different logic for handling cases when prefix != 0
                            // TODO check partial descend() in order to filter out unwanted matches
                            // permissive handler:
                            /*if (pre < stack_size)  // sanity check
                            {
                                // Right now we check if there is no partially reduced prefix
                                if (std::none_of(prefix.begin(), prefix.end(), [&](const auto& p){ return p.rule_id == rule_id; }))
                                {
                                    prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                                        add_text("Permissive handler: adding nonzero prefix ");
                                        add_symbol(symbol);
                                        add_text(" for rule ");
                                        add_symbol(rule);
                                        add_text(" at pos ");
                                        add_text(std::to_string(pre));
                                    }, __FILE__, __LINE__);
                                    prefix_todo.add(rule_id, stack_size - 1 - pre);
                                }
                            }*/
                        }
                    }
                }
                }); // each prefix position

                // Is in postfix
                tuple_each(post_dist_elems, [&](std::size_t k, const auto& post_dist){
                const auto post = min_post - post_dist; // Convert post from the distance to the end to an id

                if constexpr (!std::is_same_v<std::decay_t<decltype(post_dist)>, max_t>)
                {
                    // we can also perform additional end-of-stack check (does the prefix+postfix fit?)
                    // also we should check that the prefix rule matches (link prefixes and postfixes together)

                    for (int j = postfix.size() - 1; j >= 0; j--)  // Also verify that the order of fix is correct
                    {
                        // we're currently matching non-ambiguous ctx
                        // applied prefix or postfix are blocking: no new matches can happen
                        if (postfix[j].rule_id == rule_id)
                        {
                            // use _fix as the starting pos
                            // Same as in prefix: this is a bad check
                            /*if (postfix[j].fix + post != stack_size - 1) [[unlikely]]
                            {
                                // we have already applied the ctx, unexpected behavior
                                // ASSERT
                                prettyprinter.guru_meditation("expected static postfix to match with runtime, got a mismatch", __FILE__, __LINE__);
                                assert(postfix[j].fix + post == stack_size - 1 && "next() : guru meditation : expected static postfix to match with runtime, got a mismatch");
                            }*/
                            if (post_dist == 0)
                            {
                                // we reached the end
                                // FOUND
                                // TODO verify the correctness of this check
                                postfix.erase(postfix.begin() + j); // ok to move j
                            }
                        }
                    }
                    if (postfix_todo[rule_id] != max_t())
                    {
                        // GREEDY RESOLVER
                        /*auto fix_todo = postfix_todo[rule_id];
                        if (stack_size - 1 - fix_todo == max_pre)
                        {
                            // Pick the first one greedily. Works fine in an online parse, if the prefixes don't overlap
                            for (std::size_t k = 0; k < postfix_todo.size(); k++)
                            {
                                // Erase all todos with the same starting pos, since we apply the current one right now. This doesn't work in complex cases
                                if (k != rule_id && postfix_todo[k] == fix_todo)
                                    postfix_todo.remove(rule_id);
                                postfix_todo.remove(rule_id);
                            }
                        }*/

                        // LITERAL RESOLVER
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

                        // TODO check that the last added prefix rule is different
                        if (post == 0 && prefix_todo[rule_id] != stack_size - 1)  // TODO make a better prefix check (overlap)
                        {
                            // FOUND
                            if (do_check_ctx(rule))
                            {
                                prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                                    add_text("Found postfix ");
                                    add_symbol(symbol);
                                    add_text(" for rule ");
                                    add_symbol(rule);
                                    add_text(" at post_dist ");
                                    add_text(std::to_string(post_dist));
                                }, __FILE__, __LINE__);
                                postfix_todo.add(rule_id, stack_size - 1);
                            }
                        } else {
                            // we need different logic for handling cases when prefix != 0
                            // permissive handler:
                            /*if (post < stack_size)  // sanity check (we need to account for prefix overlap)
                                postfix_todo.add(rule_id, stack_size - 1 - post);*/
                        }
                    }
                }
                }); // each postfix position
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
                prefix.push_back(CtxMeta(match_id, prefix_todo[match_id]));
                prefix_todo.remove(match_id);
            } else {
                std::size_t match_id = postfix_todo.last_added();
                postfix.push_back(CtxMeta(match_id, postfix_todo[match_id]));
                postfix_todo.remove(match_id);
            }
        }
        /*if (postfix.fix == stack_size - 1)
        {
            // We exit ctx now!
            // context[postfix.rule_id]--;
        }*/
        prettyprinter.update_heur_ctx_at_next(context, matches, prefix, postfix, prefix_todo, postfix_todo, stack);
        return prefix_todo.size() + postfix_todo.size() == 0;
    }

    /**
     * @brief Check if a match can exist in current ctx
     * @param match Match candidate
     */
    template<class TSymbol>
    constexpr bool check_ctx(const TSymbol& match, auto& prettyprinter)
    {
        bool res = _do_check_ctx(match, prettyprinter);
        prettyprinter.update_heur_ctx_at_check(match, res);
        return res;
    }

    /**
     * @brief Update the context for the reduced symbol. This function should be called on each successful reduce operation
     * @param match Chosen match candidate
     * @param def Match candidate rule definition
     * @param stack Stack before the reduction
     */
    template<class TSymbol, class TRule, class GSymbol>
    constexpr bool apply_reduce(const TSymbol& match, const TRule& def, const std::vector<GSymbol>& stack, const std::size_t new_stack_size, auto& prettyprinter = NoPrettyPrinter())
    {
        std::size_t stack_size = stack.size();
        //std::size_t reduction_size = std::decay_t<TRule>::size(); // Size of the candidate rule, N symbols will be removed from the end of the stack
        //std::size_t new_stack_size = stack_size - reduction_size;
        if constexpr (std::tuple_size_v<std::decay_t<FullRRTree>> > 0)
        {
            constexpr std::size_t index = get_ctx_index<0, std::decay_t<TSymbol>>();
            // We need to reduce current prefixes/postfixes if they match
            for (int j = prefix.size() - 1; j >= 0; j--)  // we don't need fix positions to match exactly
            {
                if (prefix[j].fix >= new_stack_size - 1)  // -1 because the last symbol is also changed
                {
                    if (prefix[j].rule_id == index)
                    {
                        prefix.erase(prefix.begin() + j);
                        if (context[index] == 0)
                        {
                            prettyprinter.guru_meditation("empty context with existing prefix", __FILE__, __LINE__);
                            assert(context[index] > 0 && "apply_reduce() : guru meditation : empty context with existing prefix");
                        }
                    } else {
                        // TODO make this a warning instead (since a prefix is only a heuristic)
                        prettyprinter.guru_meditation("unexpected prefix found during candidate reduction", __FILE__, __LINE__);
                        assert(prefix[j].rule_id == index && "apply_reduce() : guru meditation : unexpected prefix found during candidate reduction");
                    }
                }
            }
            for (int j = postfix.size() - 1; j >= 0; j--)
            {
                if (postfix[j].fix >= new_stack_size - 1)
                {
                    if (postfix[j].rule_id == index)
                    {
                        postfix.erase(postfix.begin() + j);
                    } else {
                        // TODO make this a warning instead
                        prettyprinter.guru_meditation("unexpected postfix found during candidate reduction", __FILE__, __LINE__);
                        assert(postfix[j].rule_id == index && "apply_reduce() : guru meditation : unexpected postfix found during candidate reduction");
                    }
                }
            }

            if (context[index] > 0)
            {
                context[index]--;
            }
        }
        prettyprinter.update_heur_ctx_at_apply(context, matches, prefix, postfix, prefix_todo, postfix_todo, stack, match);

        // We can optimize this part by moving this to runtime
        //const auto fix_rt = h_type_morph<std::array<std::pair<std::size_t, std::size_t>, std::tuple_size_v<std::decay_t<FixLimits>>>, true>([]<std::size_t i>(const auto& src){ const auto p = std::get<i>(src); return std::make_pair((std::size_t)p.first, (std::size_t)p.second); }, IntegralWrapper<std::tuple_size_v<std::decay_t<FixLimits>>>{}, limits);

        // TODO Currently, we crash if some symbol is reduced. This may cause some false-positive crashes
        // Check if positions are invalidated after this operation
        for (const auto& pre : prefix)
        {
            if (pre.fix >= new_stack_size)
            {
                /*prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                    add_text(std::to_string(pre.fix));
                    add_text(" >= new stack size ");
                    add_text(std::to_string(new_stack_size));
                }, __FILE__, __LINE__);*/
                // TODO partially reduced prefix technically shouldn't be in the reduced symbol, but we should check this
                prettyprinter.guru_meditation("partially matched prefix found in the reduced symbol", __FILE__, __LINE__);
                assert(pre.fix < new_stack_size && "apply_reduce() : guru meditation : partially matched prefix found in the reduced symbol");
            }
        }
        /*for (int j = postfix.size() - 1; j >= 0; j--)
        {
            // postfix.fix is increasing (not post_dist)
            if (post.fix >= new_stack_size)
            {
                prettyprinter.guru_meditation("partially matched postfix found in the reduced symbol", __FILE__, __LINE__);
                assert(post.fix < new_stack_size && "apply_reduce() : guru meditation : partially matched postfix found in the reduced symbol");
            }
        }*/
        // Do the same position invalidation check with todos
        for (std::size_t i = 0; i < std::tuple_size_v<std::decay_t<FixLimits>>; i++)
        {
            if (prefix_todo[i] != std::numeric_limits<std::size_t>::max())
            {
                // TODO add message for potential bug (ditto)
                if ((int)prefix_todo[i] >= (int)new_stack_size - 1)  // -1 because the last symbol is changed
                {
                    prefix_todo.remove(i);
                }
                if (prefix_todo[i] >= new_stack_size)
                {
                    prettyprinter.guru_meditation("prefix todo prefix found in the reduced symbol", __FILE__, __LINE__);
                    assert(prefix_todo[i] < new_stack_size && "apply_reduce() : guru meditation : prefix todo found in the reduced symbol");
                }
            }
            if (postfix_todo[i] != std::numeric_limits<std::size_t>::max())
            {
                // TODO add message for potential bug (ditto)
                // postfix_todo[i] is increasing (not post_dist)
                if ((int)postfix_todo[i] >= (int)new_stack_size - 1)  // -1 because the last symbol is changed
                {
                    postfix_todo.remove(i);
                }
                if (postfix_todo[i] >= new_stack_size)
                {
                    prettyprinter.guru_meditation("postfix todo found in the reduced symbol", __FILE__, __LINE__);
                    assert(postfix_todo[i] < new_stack_size && "apply_reduce() : guru meditation : postfix todo found in the reduced symbol");
                }
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
        if constexpr (depth >= std::tuple_size_v<TTerms>)
            static_assert(std::is_same_v<std::false_type, TSymbol>, "heheheha");
        static_assert(depth < std::tuple_size_v<TTerms>, "Term type not found");
        // Get the corresponding TTerms element
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


    template<class TSymbol>
    constexpr bool do_check_ctx(const TSymbol& match, bool is_prefix = false)
    {
        auto printer = NoPrettyPrinter();
        return _do_check_ctx(match, printer, is_prefix);
    }

    /**
     * @brief Check if a match can exist in current ctx
     * @param match Match candidate
     */
    template<class TSymbol>
    constexpr bool _do_check_ctx(const TSymbol& match, auto& prettyprinter, bool is_prefix = false)
    {
        // Permissive mode: drop everything which cannot be in the current pos
        if constexpr (std::tuple_size_v<std::decay_t<FullRRTree>> > 0)
        {
            constexpr std::size_t rule_id = get_ctx_index<0, std::decay_t<TSymbol>>();

            const auto& idx = get_rr_all(match);
            for (const std::size_t pos : idx)  // rules where it cannot be present
            {
                if (pos == rule_id)
                {
                    if (is_prefix && context[pos] > 0)
                    {
                        prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                            add_text("Rule ");
                            add_symbol(match);
                            add_text(" prefix discarded: no self-insertion ");
                        }, __FILE__, __LINE__);
                        // No new prefixes can be here, so we need a stricter check
                        return false;
                    }
                    else if (context[pos] > 1)
                    {
                        prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                            add_text("Rule ");
                            add_symbol(match);
                            add_text(" discarded: no self-insertion ");
                            add_text(std::to_string(pos));
                        }, __FILE__, __LINE__);
                        return false;
                    }
                }
                else if (context[pos] > 0)
                {
                    prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                        add_text("Rule ");
                        add_symbol(match);
                        add_text(" discarded: cannot be in rule ");
                        add_text(std::to_string(pos));
                    }, __FILE__, __LINE__);
                    return false;
                }
            }
        }
        //return true;
        return _do_check_ctx_restrictive(match, prettyprinter);
    }


    /**
     * @brief Same as do_check_ctx, but is more restrictive
     */
    template<class TSymbol>
    constexpr bool _do_check_ctx_restrictive(const TSymbol& match, auto& prettyprinter)
    {
        /*
         * for each related rule of match:
         *     if exists(rule.prefix) and rule is not a part of prefix:
         *         return true
         *     else
         *         return false
         */
        constexpr std::size_t index = get_ctx_index<0, std::decay_t<TSymbol>>();  // get idx of match
        const auto& rr = std::get<index>(rr_tree.tree);  // Get related rules
        // TODO check for a lower level of inclusion, since there may be rr -> rr -> symbol

        bool ok = tuple_each_or_return(rr, [&](const std::size_t i, const auto& rule){
            /*prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                add_text("Match ");
                add_symbol(match);
                add_text(" has RR ");
                add_symbol(rule);
            }, __FILE__, __LINE__);*/
            // Check if prefix exists by looking at limits
            constexpr std::size_t rule_id = get_ctx_index<0, std::decay_t<decltype(rule)>>();  // get idx of the related rule
            const auto [max_pre, min_post] = std::get<rule_id>(limits);

            if constexpr (decltype(max_pre)::value != std::numeric_limits<std::size_t>::max())  // check that some prefix exists in the related rule
            {
                if (context[rule_id] > 0) return true;
                // TODO check if match is not a part of the rule prefix (otherwise we wouldn't be able to reduce the prefix in the first place)
                // ELSE we check if the parent rule is in todo (we need to allow all possible candidates here)
                if (prefix_todo[rule_id] != std::numeric_limits<std::size_t>::max())
                {
                    prettyprinter.debug_message([&](auto add_text, auto add_symbol){
                        add_text("Prefix TODO has a rule ");
                        add_symbol(rule);
                        add_text(", allowing ");
                        add_symbol(match);
                        add_text(" here...");
                    }, __FILE__, __LINE__);
                    return true;
                }

                else return false;  // try next
            } else {
                // Prefix doesn't exist for this rule, so we cannot check if this match is present in this rule at all
                return true;
            }
        });
        if (ok) return true;

        prettyprinter.debug_message([&](auto add_text, auto add_symbol){
            add_text("Discarded rule ");
            add_symbol(match);
        }, __FILE__, __LINE__);
        return false;
    }
};


template<class RulesDef, class RRTree, class NTermsMap, class TTermsTypeMap, class THeuristicsPre>
constexpr auto make_ctx_manager(const RulesDef& rules, const RRTree& tree, const NTermsMap& nterms2defs, const TTermsTypeMap& terms_tmap, const THeuristicsPre& h_pre, auto& prettyprinter)
{
    //const auto pairs = cfg_helpers::ctx_get_matches<0>(tree.defs, terms_tmap.nterms, terms_tmap.terms, tree.tree, nterms2defs);
    //const auto pairs_nt = tuple_take_along_axis<0>()
    const auto pairs_nt = cfg_helpers::ctx_get_nterm_match<0>(tree.defs, tree.tree, nterms2defs);
    const auto pairs_t = cfg_helpers::ctx_get_term_match<0>(terms_tmap.terms, terms_tmap.nterms, nterms2defs);
    const auto fix_limits = cfg_helpers::ctx_get_fix_limits<0>(tree.defs, pairs_nt, pairs_t);

    #if defined(DBG_PRINT_FIX_POS) && !defined(NO_DBG_PRINT_FIX_POS)
    const auto defs_nterms = tuple_morph([]<std::size_t i>(const auto& src){ return std::get<0>(std::get<i>(src).terms); }, tree.defs);
    static_assert(std::is_same_v<std::false_type, std::decay_t<decltype(tuple_merge_along_axis(defs_nterms, pairs_nt))>>, "NTerms pairs; run the format_template_inst.py script with this template");
    static_assert(std::is_same_v<std::false_type, std::decay_t<decltype(tuple_merge_along_axis(terms_tmap.terms, pairs_t))>>, "Terms pairs; run the format_template_inst.py script with this template");
    static_assert(std::is_same_v<std::false_type, std::decay_t<decltype(fix_limits)>>, "Fix limits; run the format_template_inst.py script with this template");
    #endif

    const auto defs_flatten = tuple_morph([&]<std::size_t i>(const auto& src){ return std::get<0>(std::get<i>(tree.defs).terms); }, tree.defs);
    //static_assert(std::is_same_v<std::false_type, decltype(tree.tree)>, "Regular tree");
    //static_assert(std::is_same_v<std::false_type, decltype(h_pre.full_rr)>, "Full tree");

    //static_assert(std::tuple_size_v<std::decay_t<decltype(tree.defs)>> == std::tuple_size_v<std::decay_t<decltype(pairs_nt)>>, "bad pairs_nt");
    prettyprinter.init_ctx_classes(defs_flatten, h_pre.unique_rr, terms_tmap.terms, terms_tmap.nterms, pairs_nt, pairs_t, fix_limits, h_pre.full_rr);
    return ContextManager<decltype(defs_flatten), decltype(pairs_nt), decltype(pairs_t), decltype(h_pre.unique_rr), decltype(terms_tmap.terms), RRTree, decltype(h_pre.full_rr), decltype(fix_limits)>(defs_flatten, pairs_nt, pairs_t, h_pre.unique_rr, terms_tmap.terms, tree, h_pre.full_rr, fix_limits);
}



#endif //CONTEXT_H
