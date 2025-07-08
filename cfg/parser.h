//
// Created by Flynn on 27.01.2025.
//

#ifndef SUPERCFG_PARSER_H
#define SUPERCFG_PARSER_H

#include "follow.h"
#include "cfg/preprocess.h"
#include "cfg/preprocess_factories.h"
#include "cfg/context.h"


/**
 * Alter operation solving method
 */
enum class LL1AlterSolver
{
    PickFirst,    /**< Pick the first match and return */
    PickLongest,  /**< Pick the longest match, return error on same length */
    PickLongestF, /**< Pick the longest match, do not perform check */
    Permute       /**< Permute over all possible permutations, starting from root */
};


enum class LL1ParserError
{
    None,
    AmbiguousAlter, /**< Alter operation could returned >1 possible variant */
};


/**
 * @brief Struct containing error metadata
 * @tparam VStr Variable string class
 */
template<class VStr>
struct LL1ParserErrorMeta
{
    std::size_t token_pos;
    VStr type;
};


/**
 * @brief Parser configuration options
 * @tparam alter Alter operation config
 */
template<LL1AlterSolver alter>
struct LL1ParserOptions
{
    static constexpr LL1AlterSolver alter_conf() { return alter; }
};


 /**
  * @brief Tokens parser class, can handle LL(1) grammars
  * @tparam VStr Variable string class
  * @tparam TokenType Nonterminal type (name) container
  * @tparam RulesSymbol Rules class
  * @tparam ParserOpt LL1 Parser config
  * @tparam Tree Parser tree node class
  */
template<class VStr, class TokenType, class Tree, class ParserOpt, class RulesSymbol>
class LL1Parser
{
protected:
    //NTermsStorage<TokenType, RulesSymbol> storage;
    NTermsConstHashTable<RulesSymbol> storage;
    using TokenV = Token<VStr, TokenType>;

public:
    LL1ParserError err;
    LL1ParserErrorMeta<VStr> err_meta;

    constexpr explicit LL1Parser(const RulesSymbol& rules) : storage(rules), err(LL1ParserError::None), err_meta{} {}

    /**
     * @brief Recursively parse tokens and build parse tree
     * @param node Empty tree to populate
     * @param root Rules starting point
     * @param tokens List of tokens from Tokenizer
     */
    template<class RootSymbol, class TokenTWrapper>
    bool run(Tree& node, const RootSymbol& root, const std::vector<TokenTWrapper>& tokens)
    {
        std::size_t index = 0;
        return parse(root, node, index, tokens, 0);
    }

protected:
    template<class TSymbol, class TokenTWrapper>
    bool parse(const TSymbol& symbol, Tree& node, std::size_t& index, const std::vector<TokenTWrapper>& tokens, std::size_t depth)
    {
        if (index >= tokens.size()) return false;
//        std::cout << "<d " << depth << "> ";

        // Iterate over each operator
        if constexpr (is_operator<TSymbol>())
        {
//            std::cout << "op [" << int(get_operator<TSymbol>()) << "] at i=" << index << " tok=" << tokens[index].value << std::endl;
            if constexpr (get_operator<TSymbol>() == OpType::Concat)
            {
                std::size_t index_stack = index;
                std::cout << "concat : ";
                Tree node_stack = node;
                // Iterate over each node, index is moved each time. No need to copy node on stack
                bool ok = symbol.each_or_exit([&](const auto& s) -> bool {
                    if (!parse(s, node_stack, index, tokens, depth+1))
                    {
                        std::cout << "<d " << depth << "> " << "concat failed at tok " << tokens[index].value << " (" << index << ")" << std::endl;
                        index = index_stack; // Revert the first index
                        return false; // Didn't find anything
                    }
                    return true; // Continue
                });
                std::cout << "<d " << depth << "> " << "concat ok for tok " << tokens[index].value << " (" << index << ")" << std::endl;
                if (ok) node = node_stack;
                return ok;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Alter)
            {
                return parse_alter(symbol, node, index, tokens, depth);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Optional)
            {
                // Try to match if we can, just return true anyway
                Tree node_stack = node;
                if (parse(std::get<0>(symbol.terms), node_stack, index, tokens, depth+1)) node = node_stack;
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Repeat)
            {
                Tree node_stack = node;
//                std::cout << "repeat : ";
                while (parse(std::get<0>(symbol.terms), node_stack, index, tokens, depth+1))
                {
                    std::cout << "repeat success" << std::endl;
                    // Update stack when we succeed
                    node = node_stack;
                }
//                std::cout << "repeat fail, exiting, i=" << index << std::endl;
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Group)
            {
                return parse(std::get<0>(symbol.terms), node, index, tokens, depth+1);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Except)
            {
                std::size_t i = index;
                Tree node_stack = node;
                if (parse(std::get<0>(symbol.terms), node_stack, index, tokens, depth+1))
                {
                    // Check if the symbol is an exception
                    if (!parse(std::get<1>(symbol.terms), Tree(), i, tokens, depth+1))
                    {
                        index = i;
                        node = node_stack;
                        return true;
                    }
                }
                return false;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatExact)
            {
                std::size_t index_stack = index;
                for (std::size_t i = 0; i < get_repeat_times(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens, depth+1)) { index = index_stack; return false; }
                }
                // We don't need to check the next operator, since it may start from this token
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatGE)
            {
                std::size_t index_stack = index;
                for (std::size_t i = 0; i < get_repeat_times(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens, depth+1)) { index = index_stack; return false; }
                }
                Tree node_stack = node;
                while (parse(std::get<0>(symbol.terms), node_stack, index, tokens))
                {
                    // Update stack when we succeed
                    node = node_stack;
                }
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatRange)
            {
                std::size_t index_stack = index;
                for (std::size_t i = 0; i < get_range_from(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens, depth+1)) { index = index_stack; return false; }
                }
                Tree node_stack = node;
                for (std::size_t i = get_range_from(symbol); i < get_range_to(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens, depth+1)) return true;
                    else node = node_stack; // Update symbol
                }
                // We don't need to check the next operator, since it may start from this token
                return true;
            }
            else
            {
                // TODO handle Comment and SpecialSeq
                static_assert(get_operator<TSymbol>() == OpType::Comment || get_operator<TSymbol>() == OpType::SpecialSeq, "Wrong operator type");
            }

        } else if constexpr (is_nterm<TSymbol>()) {
            std::cout << "<d " << depth << "> " << "nt [" << symbol.name.c_str() << "] at i=" << index << " tok=" << tokens[index].value << std::endl;
            // Get definition and get rules for the non-terminal
            const auto& s = std::get<1>(storage.get(symbol)->terms);

            // Create and pass a new leaf node
            //Tree next = symbol.template create_node<Tree>(node);
            Tree next(symbol.name, &node);
            node.add(next);
            return parse(s, node.last(), index, tokens, depth+1);

        } else if constexpr (is_term<TSymbol>()) {
//            std::cout << "t  [" << symbol.name.c_str() << "] at i=" << index << " tok=" << tokens[index].value << std::endl;
            if (index >= tokens.size()) return false;

            if (tokens[index].value == symbol.name)
            {
                std::cout << "matched!" << std::endl;
                node.add_value(tokens[index].value);
                index++; // Move the pointer only if we succeed
                return true;
            }
            return false;
        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");

        return true;
    }

    template<class TSymbol, class TokenTWrapper>
    inline bool parse_alter(const TSymbol& symbol, Tree& node, std::size_t& index, const std::vector<TokenTWrapper>& tokens, std::size_t depth)
    {
        if constexpr (ParserOpt::alter_conf() == LL1AlterSolver::PickFirst)
        {
            // Check any of these nodes
            std::size_t i = index;

            // Return true if we match at least one
            return !symbol.each_or_exit([&](const auto& s) -> bool {
                // Apply index and return
                Tree node_stack = node;
                if (parse(s, node_stack, i, tokens, depth+1))
                {
                    node = node_stack; // Apply changes
                    index = i;
                    return false; // Exit from callback
                }
                return true; // Continue
            });
        } else if constexpr (ParserOpt::alter_conf() == LL1AlterSolver::PickLongestF) {
            std::size_t i_max = 0;
            bool found = false;
            Tree node_stack_max;

            // (a+a)+b - in a+a both 'a' and 'a+a' may evaluate to true, so we cannot get the first match
            symbol.each([&](const auto& s) {
                // Apply index and return
                Tree node_stack = node;
                std::size_t i = index;
                if (parse(s, node_stack, i, tokens, depth+1))
                {
                    if (i + 1 > i_max) // 0 tokens case
                    {
                        node_stack_max = node_stack;
                        i_max = i + 1;
                    }
                    found = true;
                }
            });

            if (found)
            {
                index = i_max - 1;
                node = node_stack_max;
            }
            return found;

        } else if constexpr (ParserOpt::alter_conf() == LL1AlterSolver::PickLongest) {
            std::size_t i_max = 0, i_max2 = 0; // We need to perform check if second max == max
            bool found = false;
            Tree node_stack_max;

            // (a+a)+b - in a+a both 'a' and 'a+a' may evaluate to true, so we cannot get the first match
            symbol.each([&](const auto& s) {
                // Apply index and return
                Tree node_stack(node);
                std::size_t i = index;
                if (parse(s, node_stack, i, tokens, depth+1))
                {
                    if (i + 1 >= i_max) // 0 tokens case
                    {
                        if (i + 1 == i_max) i_max2 = i + 1;
                        node_stack_max = node_stack;
                        i_max = i + 1;
                    }
                    found = true;
                }
            });

            if (found && i_max == i_max2) [[unlikely]] // Error condition should be unlikely
            {
                err = LL1ParserError::AmbiguousAlter;
                set_error(symbol, index);
                return false;
            }

            if (found)
            {
                std::cout << "<d " << depth << "> " << "found largest ind : " << i_max << "; " << tokens[index].value << " -> " << tokens[i_max].value << std::endl;
                index = i_max - 1;
                node = node_stack_max;
            }
            return found;
        }
    }

    template<class TSymbol>
    inline void set_error(const TSymbol& symbol, std::size_t index)
    {
        err_meta.token_pos = index;
        err_meta.type = VStr(typeid(TSymbol).name());
        abort();
    }
};


enum class SRConfEnum : std::uint64_t
{
    PrettyPrint = 0x1, ///< Global flag to enable printing
    Lookahead = 0x10, ///< Prevent partial reductions using FOLLOW set lookahead
    ReducibilityChecker = 0x100, ///< Enable ReducibilityChecker(1) module which checks if a rule can be reduced 1 step in the future
    RC1CheckContext = 0x1000, ///< Enable RC(1) partial context analysis feature. Inferior to a full context manager
    HeuristicCtx = 0x10000,   ///< Enable (pre/post)fix based context analyzer (aka ContextManager)
};


template<std::uint64_t Conf>
class SRParserConfig
{
public:
    constexpr explicit SRParserConfig() {}

    static constexpr std::uint64_t value() { return Conf; }

    template<SRConfEnum value>
    [[nodiscard]] static constexpr bool flag() { return (Conf & static_cast<std::uint64_t>(value)) > 0; }
};

template<SRConfEnum... Values>
constexpr auto mk_sr_parser_conf()
{
    constexpr std::uint64_t conf = (static_cast<const std::uint64_t>(Values) | ...);
    return SRParserConfig<conf>();
}


template<class VStr, class TokenType, class TokenTSet, class Tree, std::size_t STACK_MAX, class RulesSymbol, class RRTree, class SymbolsHT, class TermsMap, std::uint64_t Conf, class Lookahead, class RChecker, class CtxMgr>
class SRParser
{
protected:
    SymbolsHT symbols_ht;
    TermsMap terms_storage;
    RRTree reverse_rules;
    // std::unordered_map<TokenType, std::vector<TokenType>> reverse_rules_ht;
    NTermsConstHashTable<RulesSymbol> defs;
    SRParserConfig<Conf> conf;
    using SrC = SRParserConfig<Conf>;
    Lookahead look;
    RChecker r_checker;
    CtxMgr ctx_mgr;

    using TokenV = Token<VStr, TokenTSet>;
    using GSymbolV = GrammarSymbol<VStr, TokenTSet>;
public:
    constexpr explicit SRParser(const RulesSymbol& rules, const RRTree& rr_tree, const SymbolsHT& ht, const TermsMap& t_map, SRParserConfig<Conf> conf, const Lookahead& lookahead, const RChecker& checker, const CtxMgr& h_ctx) : symbols_ht(ht), terms_storage(t_map), reverse_rules(rr_tree), defs(rules), conf(conf), look(lookahead), r_checker(checker), ctx_mgr(h_ctx) {}
    // Construct reverse tree (mapping TokenType -> tuple(NTerms)), in which nterms is it contained

    template<class RootSymbol>
    bool run(Tree& node, const RootSymbol& root, std::vector<TokenV>& tokens)
    {
        if constexpr (enabled<SRConfEnum::ReducibilityChecker>())
            r_checker.reset_ctx();
        if constexpr (enabled<SRConfEnum::HeuristicCtx>())
            ctx_mgr.reset_ctx();
        // Initialize point at zero
        std::vector<GSymbolV> stack{GSymbolV(tokens[0].value, tokens[0].type)};
        std::size_t i = 1;

        while (true) //(i < tokens.size())
        {
            if (!reduce_lookahead_runtime(stack, &node, tokens, i))
            {
                // Shift operation
                if (i == tokens.size()) [[unlikely]]
                    //return false;
                    break;
                if constexpr (enabled<SRConfEnum::HeuristicCtx>())
                {
                    // CtxManager analyzes the symbols which cannot be reduced right away
                    // Take the last symbol and try resolving context

                    auto tok = stack.back();

                    for (std::size_t j = 0; !ctx_mgr.next(tok, stack.size(), symbols_ht); j++)
                    {
                        // Ambiguity found, move tok
                        if (j >= tokens.size()) [[unlikely]]
                        {
                            if constexpr (enabled<SRConfEnum::PrettyPrint>()) std::cout << "unresolved ambiguity encountered" << std::endl;
                            return false;
                        }
                        // TODO replace Token class with GSymbol
                        tok = GSymbolV(tokens[j].value, tokens[j].type); // take the next term from the tokens
                    }
                    // ambiguity resolved
                }

                stack.push_back(GSymbolV(tokens[i].value, tokens[i].type));
                i++;
                if constexpr (enabled<SRConfEnum::PrettyPrint>()) std::cout << "[sh] s: [";
            } else if constexpr (enabled<SRConfEnum::PrettyPrint>()) std::cout << "[re] s: [";
            if constexpr (enabled<SRConfEnum::PrettyPrint>()) { prettyprint(stack); std::cout << "]" << std::endl; }
        }
        // We only have the root symbol, nothing to parse
        if (stack.size() == 1 && !stack[0].is_token() && stack[0].type.front() == root.type())
            return true;
        return false;
    }

protected:
    void prettyprint(std::vector<GSymbolV>& stack, std::size_t start = 0) const
    {
        for (std::size_t i = start; i < stack.size(); i++)
        {
            if (i != start) std::cout << " ";
            const auto& g = stack[i];
            if (g.is_token()) std::cout << g.value;
            else std::cout << "<" << g.type << ">";
        }
    }

    bool reduce(std::vector<GSymbolV>& stack, Tree* root, Tree* cur_node)
    {
        // Get rightmost token type (handle)
        // check shift_reduce_parser_notes.txt

        // iterate from right to left over stack n times
        // at step i:
        //   for token : get its nterm (type) from TokenV
        //   for nterm : get its related nterms from reverse rules tree
        //   find intersect with the previous one
        // for each common nterm : execute parse() for each definition
        // if found 1 : reduce, else goto next iter


        // Trivial case with 1 element
        // Simply try to reduce the element on the top
        /*GSymbolV& top_elem = stack.back();
        top_elem.visit([&](const VStr&& token, const TokenType& type){
            // Parse the token

        }, [&](const TokenType& type){
            // Parse the nterm
        });*/

        // We define a list of possible token types
        using terms_types = typename SymbolsHT::TermsTuple;
        using nterms_types = typename SymbolsHT::NTermsTuple;

        // Terms and nterms
        using all_types = decltype(std::tuple_cat(terms_types(), nterms_types()));
        using token_types_variant = variadic_morph_t<all_types>;

        // Terms -> NTerms mapping
        //using terms2nterms_t = decltype(terms_storage.get_all(terms_types()));

        // All possible types are nterms
        using token_types = nterms_types;

        // Morph each token type into its possible related type (underlying type for a token, related types of a nterm)
        using window_types = decltype(
            //nterms_types()
            std::tuple_cat(std::tuple<>(), // TODO Do we need an empty tuple?
            token_types(),
            tuple_morph_t<nterms_types>([&]<std::size_t index>(){
                // Morph NTerm into its related type (for a nterm)
                // Reverse rules tree returns a tuple of matching types
                return reverse_rules.get(std::get<index>(nterms_types()));
            }))
        );

        // First loop over the stack
        tuple_for([&]<std::size_t i>(){
            if (i < stack.size())
            {
                std::size_t i_rev = stack.size() - i - 1; // Get position from the top

                // Get each related type (h_types) and the symbol (symbols) from the window
                auto h_types_and_symbols = to_homogeneous_tuple(tuple_for([&]<std::size_t j>(){
                    std::size_t j_rev = j + i_rev; // Get jth position from the top
                    GSymbolV& elem = stack[j_rev];

                    // HERE we access the hashmap
                    // Check the stack element type
                    return elem.visit([&](){
                        // It's a token
                        // We CANNOT return the nonhomogeneous type here at all
                        // Here we also need to merge 2 tuples element-wise and unpack them later
                        return symbols_ht.get_term(elem.type, [&](const auto& term){
                            return std::make_tuple(homogeneous_elem_morph<window_types>(terms_storage.get(term)), token_types_variant(term));
                        }); // hashtable access

                    }, [&](){
                        // It's a nterm
                        // The return types HERE and ABOVE must be equal
                        return symbols_ht.get_nterm(elem.type, [&](const auto& nterm){
                            return std::make_tuple(homogeneous_elem_morph<window_types>(reverse_rules.get(nterm)), token_types_variant(nterm));
                        }); // hashtable access
                    }); // element type
                }, IntegralWrapper<i>())); // the second loop

                // We need to unpack the tuple
                auto h_types = tuple_take_along_axis<0>(h_types_and_symbols); // Related types
                auto symbols = tuple_take_along_axis<1>(h_types_and_symbols); // Only the found symbols

                // Expand a tuple of homogeneous type
                type_expansion(h_types, [&](const auto& related_types){
                    // Find common types among these
                    // If there are less matches than elements, then we don't even need to check
                    if constexpr (std::tuple_size_v<std::decay_t<decltype(related_types)>> < i + 1) return false; // No idea what to return
                    else
                    {
                        auto common_types = tuple_intersect(related_types);

                        if constexpr (std::tuple_size<decltype(common_types)>() > 0)
                        {
                            return tuple_each_or_return(common_types, [&](std::size_t, const auto& type){
                                type_expansion(symbols, [&](const auto& sequence){
                                    std::size_t index = 0;
                                    if (descend_batch(sequence, type, index)) // TODO get definition from type and pass that instead
                                    {
                                        // Found
                                        for (std::size_t j = i_rev; j < stack.size(); ++j)
                                        {
                                            if (stack[j].is_token())
                                                cur_node->add_value(stack[j].value);
                                            else
                                            {
                                                // Hella inefficient
                                                cur_node->parent = root;
                                                cur_node->name = stack[j].type;
                                                root->add(cur_node);
                                                // Cur node is now the root
                                                cur_node = root;
                                                // Create new root node
                                                *root = Tree();
                                            }
                                        }

                                        stack.erase(stack.begin() + i_rev, stack.end()); // May be inefficient
                                        stack.push_back(GSymbolV(type.type())); // insert the matched nterm
                                        return true; // Performed reduce, tell tuple_each_or_return to exit
                                    } // Not found
                                    return false;
                                });
                            });
                        }
                    }
                });
            }// else return false; // TODO fix early exit
        }, IntegralWrapper<STACK_MAX>()); // the first for loop
        return false;
    }

    bool reduce_lookahead_runtime(std::vector<GSymbolV>& stack, Tree* root, const std::vector<TokenV>& tokens, std::size_t tokens_ind)
    {
        if constexpr (enabled<SRConfEnum::Lookahead>())
        {
            if (tokens_ind != tokens.size()) [[likely]]
            {
                if constexpr (enabled<SRConfEnum::PrettyPrint>())
                    std::cout << "l: " << tokens[tokens_ind].type << std::endl;
                return reduce_runtime(stack, root, tokens, tokens_ind, tokens[tokens_ind].type);
            }
            // Disable lookahead check
            return reduce_runtime(stack, root, tokens, tokens_ind, std::false_type());
        } else return reduce_runtime(stack, root, tokens, tokens_ind, std::false_type());
    }

    template<class LookaheadS>
    bool reduce_runtime(std::vector<GSymbolV>& stack, Tree* root, const std::vector<TokenV>& tokens, std::size_t tokens_ind, const LookaheadS& lookahead)
    {
        // First loop over the stack
        // Greedy mode: check longer substr first
        //for (std::int64_t i = stack.size() - 1; i >= 0; i--)



        for (std::int64_t i = 0; i < stack.size(); i++)
        {
            // Efficient vector of common types
            ConstVec<TokenType> intersect;
            // First element (optimized j=0)
            const GSymbolV& first = stack[i];
            if (first.is_token())
            {
                // Size of the set will be N symbols where it is present
                if constexpr (TokenTSet::is_singleton())
                    intersect.init(first.type);
                else
                    intersect.init(first.type.vec());
            } else {
                //intersect.push_back(reverse_rules_ht[first.type]);

                // Size of the set will be no greater than the related element
                symbols_ht.get_nterm(first.type.front(), [&](const auto& nterm){
                    // Tuple of related elements
                    const auto& related_types = tuple_morph([&]<std::size_t k>(const auto& rr){ return TokenType(std::get<k>(rr).type()); }, reverse_rules.get(nterm));
                    intersect.init(related_types);
                });
            }

            // Loop over the window
            for (std::int64_t j = i + 1; j < stack.size(); j++)
            {
                if (intersect.size() == 0) break;
                const GSymbolV& elem = stack[j];

                if (elem.is_token())
                {
                    if constexpr (TokenTSet::is_singleton())
                    {
                        // Find intersection with token - only a single type
                        [&]{
                            for (std::size_t k = 0; k < intersect.size(); k++)
                            {
                                if (intersect[k] == elem.type)
                                {
                                    intersect.replace_with(elem.type);
                                    return; // Success
                                }
                            }
                            intersect.erase(); // Failure
                        }();
                    } else {
                        // We need to iterate over all types
                        std::size_t found = 0;
                        for (std::size_t l = 0; l < elem.type.size(); l++)
                        {
                            // Iterate over the set
                            for (std::size_t k = found; k < intersect.size(); k++)
                            {
                                if (intersect[k] == elem.type[l])
                                {
                                    // Move to the beginning
                                    std::swap(intersect[found], intersect[k]); // We assume that same element swap is safe
                                    found++;
                                }
                            }
                        }
                        // Crop the array to the new size
                        intersect.set_size(found);
                    }

                } else {
                    // Size of the set will be not greater than the related element
                    symbols_ht.get_nterm(elem.type.front(), [&](const auto& nterm){
                        // Tuple of related elements
                        const auto& related_types = reverse_rules.get(nterm);

                        // Number of elements already found. Matching elements are pushed to the beginning of the array
                        std::size_t found = 0;
                        // Iterate over the set
                        for (std::size_t k = 0; k < intersect.size(); k++)
                        {
                            tuple_each(related_types, [&](std::size_t l, const auto& t){
                                // Found
                                if (intersect[k] == t.type())
                                {
                                    // Move to the beginning
                                    std::swap(intersect[found], intersect[k]); // We assume that same element swap is safe
                                    found++;
                                }
                            });
                        }
                        // Crop the array to the new size
                        intersect.set_size(found);
                    });
                }
            }

            if constexpr (enabled<SRConfEnum::PrettyPrint>())
            {
                std::cout << "  " << "s: [";
                prettyprint(stack, i);

                std::cout << "], i: {";
                for (std::size_t k = 0; k < intersect.size(); k++)
                {
                    if (k != 0) std::cout << " ";
                    std::cout << intersect[k];
                }
                std::cout << "}" << std::endl;
            }

            // Iterate over the matching elements
            for (std::size_t k = 0; k < intersect.size(); k++)
            {
                bool found = symbols_ht.get_nterm(intersect[k], [&](const auto& match){
                    // It is cheaper to perform context check now
                    if constexpr (enabled<SRConfEnum::HeuristicCtx>())
                    {
                        if (!ctx_mgr.check_ctx(match))
                            return false; // not allowed in current ctx
                    }

                    // Get definition of the common type
                    const auto& def = std::get<1>(defs.get(match)->terms);

                    std::size_t index = 0;

                    bool success = descend_batch_runtime(stack, i, def, index, [](const auto&... args){});
                    if constexpr (enabled<SRConfEnum::PrettyPrint>())
                    {
                        std::cout << "  " << "found : " << success << ", i: " << index << "/" << stack.size() - i << std::endl;
                    }

                    // We need to cover all stack with one iteration
                    bool match_success = success && index + i == stack.size();
                    if (!match_success) return false;

                    if constexpr (enabled<SRConfEnum::ReducibilityChecker>())
                    {
                        // We need to check if at least one top-level rule will be able to reduce the stack
                        // This routine requires the stack to have the match to be applied
                        std::vector<GSymbolV> stack_copy(stack.begin(), stack.begin() + i); // keep [0 - i-1]
                        stack_copy.push_back(GSymbolV(TokenTSet(intersect[k])));
                        bool ok = r_checker.can_reduce(match, stack_copy.size(), defs, [&](std::size_t index_stack, const auto& def_r){
                            std::size_t index_check = 0, index_check_max = 0;
                            descend_batch_runtime(stack_copy, index_stack, def_r, index_check, [&](const std::size_t ind, bool match_ok){
                                // Handle lost index
                                if (ind > index_check_max) index_check_max = ind;
                            });
                            return std::max(index_check, index_check_max);
                        });

                        r_checker.apply_ctx(); // Apply the context
                        if (!ok)
                        {
                            std::cout << "  rc(1) doesn't allow to reduce" << std::endl;
                            return false;
                        }
                    }// else return true;

                    // Check lookahead symbol
                    if constexpr (enabled<SRConfEnum::Lookahead>() && !std::is_same_v<std::decay_t<LookaheadS>, std::false_type>)
                    {
                        // The next symbol is of the same type
                        for (std::size_t l = 0; l < lookahead.size(); l++)
                        {
                            if (!symbols_ht.get_nterm(lookahead[l], [&](const auto& nterm){ return look.can_reduce(match, nterm); }))
                            {
                                // No suitable symbol found
                                if constexpr (enabled<SRConfEnum::PrettyPrint>())
                                    std::cout << "^ look mismatch" << std::endl;
                                return false; // Not found
                            }
                        }
                    }

                    if constexpr (enabled<SRConfEnum::ReducibilityChecker>())
                        r_checker.apply_reduce(match); // If the matched symbol has context, we need to decrement
                    if constexpr (enabled<SRConfEnum::HeuristicCtx>())
                        ctx_mgr.apply_reduce(match, stack.size()); // ditto
                    return true;
                });

                if (!found) continue;

                // New node of the matched type
                Tree new_node(intersect[k], root);
                for (std::size_t j = i; j < stack.size(); ++j)
                {
                    if (stack[j].is_token())
                        new_node.add_value(stack[j].value);
                    else
                    {
                        // We need to move these nodes from root into the new element
                        Tree& elem = root->nodes.back(); // Get the nterm from root
                        elem.parent = &new_node; // It will be invalidated anyway!
                        new_node.add(elem);
                        root->nodes.erase(root->nodes.end() - 1); // Hella inefficient
                    }
                }
                // Insert the new node
                root->add(new_node);

                stack.erase(stack.begin() + i, stack.end()); // May be inefficient
                stack.push_back(GSymbolV(TokenTSet(intersect[k]))); // insert the matched nterm
                return true; // Performed reduce, return to shift
            }
        }
        return false;
    }

    /**
     * @brief Recursively descend over the grammar rule and check if current sequence matches the rule
     * @tparam Types Types sequence tuple
     * @param symbol Grammar rule
     */
    template<class Types, class TSymbol, class DefSymbol>
    constexpr bool descend_batch(const Types& sequence, const TSymbol& symbol, std::size_t& index) const
    {
        auto indexer = TupleIndexer(sequence);
        if constexpr (is_operator<TSymbol>())
        {
            if constexpr (get_operator<TSymbol>() == OpType::Concat)
            {
                std::size_t index_stack = index;
                bool ok = symbol.each_or_exit([&](const auto& s) -> bool {
                    if (!descend_batch(sequence, s, index_stack))
                        return false; // Didn't find anything
                    return true; // Continue
                });
                if (ok) index = index_stack;
                return ok;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Alter)
            {
                // Get each element and check if at least one matches
                return !symbol.each_or_exit([&](const auto& s) -> bool {
                    if (descend_batch(sequence, s, index)) return false; // Found
                    return true; // Continue
                });
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Optional)
            {
                // Try to match if we can, just return true anyway
                return descend_batch(sequence, std::get<0>(symbol.terms), index);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Repeat)
            {
                while (descend_batch(sequence, std::get<0>(symbol.terms), index)) {}
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Group)
            {
                return descend_batch(sequence, std::get<0>(symbol.terms), index);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Except)
            {
                std::size_t i = index;
                if (descend_batch(sequence, std::get<0>(symbol.terms), i))
                {
                    // Check if the symbol is an exception
                    if (!descend_batch(sequence, std::get<1>(symbol.terms), i))
                    {
                        index = i;
                        return true;
                    }
                }
                return false;
            }
            else
            {
                // TODO implement other operators
            }
        } else if constexpr (is_nterm<TSymbol>()) {
            return tuple_at(sequence, index, [&](const auto& nterm){
                if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::decay_t<decltype(nterm)>>())
                {
                    index++;
                    return true; // Matched
                } else return false;
            });
        } else if constexpr (is_term<TSymbol>()) {
            // We can move this comparison to reduce() loop
            // Check if current NTerm is equal to the definition symbol
            if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::decay_t<DefSymbol>>())
            {
                index++;
                return true;
            } else return false;

        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
        return false;
    }

    template<class TSymbol>
    constexpr bool descend_batch_runtime(const std::vector<GSymbolV>& stack, std::size_t start, const TSymbol& symbol, std::size_t& index, auto handle_index) const
    {
        if (start + index >= stack.size()) return false;

        if constexpr (is_operator<TSymbol>())
        {
            if constexpr (get_operator<TSymbol>() == OpType::Concat)
            {
                std::size_t index_stack = index;
                bool ok = symbol.each_or_exit([&](const auto& s) -> bool {
                    if (!descend_batch_runtime(stack, start, s, index_stack, handle_index))
                        return false; // Didn't find anything
                    return true; // Continue
                });
                if (ok) index = index_stack;
                handle_index(index_stack, ok); // We need to handle case when index_stack is lost
                return ok;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Alter)
            {
                // Get each element and check if at least one matches
                return !symbol.each_or_exit([&](const auto& s) -> bool {
                    if (descend_batch_runtime(stack, start, s, index, handle_index)) return false; // Found
                    return true; // Continue
                });
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Optional)
            {
                // Try to match if we can, just return true anyway
                return descend_batch_runtime(stack, start, std::get<0>(symbol.terms), index, handle_index);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Repeat)
            {
                while (descend_batch_runtime(stack, start, std::get<0>(symbol.terms), index, handle_index)) {}
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Group)
            {
                return descend_batch_runtime(stack, start, std::get<0>(symbol.terms), index, handle_index);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Except)
            {
                std::size_t i = index;
                if (descend_batch_runtime(stack, start, std::get<0>(symbol.terms), i, handle_index))
                {
                    // Check if the symbol is an exception
                    if (!descend_batch_runtime(stack, start, std::get<1>(symbol.terms), i, handle_index))
                    {
                        index = i;
                        return true;
                    }
                    handle_index(i, false); // Else we need to store the lost index
                }
                return false;
            }
            else
            {
                // TODO implement other operators
            }
        } else if constexpr (is_nterm<TSymbol>()) {
            const GSymbolV& elem = stack[start + index];
            if (!elem.is_token() && elem.type.front() == symbol.type())
            {
                index++;
                return true;
            }
            return false;

        } else if constexpr (terminal_type<TSymbol>()) {
            const GSymbolV& elem = stack[start + index];
            if constexpr (is_term<TSymbol>())
            {
                if (elem.is_token() && elem.value == symbol.name)
                {
                    index++;
                    return true;
                }
            } else { // Range
                if (elem.is_token() && in_terms_range<TSymbol>(elem.value[0])) // TODO cover case when value is > 1 symbol
                {
                    index++;
                    return true;
                }
            }
            return false;

        } else static_assert(terminal_type<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
        return false;
    }

    template<SRConfEnum Value>
    static constexpr bool enabled() { return SrC::template flag<Value>(); }
};


template<class VStr, class TokenType, class Tree, class RulesSymbol, class TLexer, class Conf>
constexpr auto make_sr_parser(const RulesSymbol& rules, const TLexer& lex, Conf conf)
{
    // Initialize reverse rules tree
    auto rr_tree = reverse_rules_tree_factory(rules); //ReverseRuleTreeFactory().build(root);
    // Initialize symbols hashtable
    // Cannot be constexpr due to std::unordered_map
    auto symbols_ht = symbols_ht_factory<TokenType>(rules); //SymbolsHashTableFactory().build<TokenType>(root);
    // Initialize terms2nterms map
    auto terms_map = terms_map_factory(rules); //TermsMapFactory::build(root);
    // Initialize nterms2defs map
    auto defs = NTermsConstHashTable(rules);

    // Get tokens container class
    using TokenSetClass = typename TLexer::TokenSetClass;

    auto instantiate_parser = [&](const auto& lookahead, const auto& rchecker, const auto& ctx_mgr){
        return SRParser<VStr, TokenType, TokenSetClass, Tree, 1, std::decay_t<decltype(rules)>, std::decay_t<decltype(rr_tree)>, std::decay_t<decltype(symbols_ht)>, std::decay_t<decltype(terms_map)>, decltype(conf)::value(), std::decay_t<decltype(lookahead)>, std::decay_t<decltype(rchecker)>, std::decay_t<decltype(ctx_mgr)>>(rules, rr_tree, symbols_ht, terms_map, conf, lookahead, rchecker, ctx_mgr);
    };

    auto instantiate_lookahead = [&](){
        if constexpr (conf.template flag<SRConfEnum::Lookahead>())
        {
            auto look = simple_lookahead_factory(rr_tree, defs);
            if constexpr (conf.template flag<SRConfEnum::PrettyPrint>())
            {
                std::cout << "  REVERSE RULES TREE : " << std::endl;
                rr_tree.template prettyprint<VStr>();
                look.template prettyprint<VStr>();
                std::cout << "  LEXER TERMS TYPES : " << std::endl;
                lex.prettyprint();
            }
            return look;
        } else
            return NoLookahead();
    };

    auto instantiate_heuristic_pre = [&](){
        // We need to determine which heuristics preprocessing data to compute
        constexpr std::uint64_t h_feat = ((conf.template flag<SRConfEnum::RC1CheckContext>() || conf.template flag<SRConfEnum::HeuristicCtx>()) * (std::uint64_t)HeuristicFeatures::ContextManagement);
        return make_heuristic_preprocessor<conf.template flag<SRConfEnum::PrettyPrint>(), HeuristicFeatures(h_feat)>(rr_tree);
    };

    auto instantiate_ctx_manager = [&](const auto& h_pre){
        if constexpr (conf.template flag<SRConfEnum::HeuristicCtx>())
        {
            if constexpr (!std::decay_t<TLexer>::is_legacy())
            {
                auto ctx_mgr = make_ctx_manager(rr_tree, defs, lex.terms_map, h_pre);
                return ctx_mgr;
            } else {
                static_assert(!std::decay_t<TLexer>::is_legacy(), "Cannot use legacy lexer with HeuristicCtx");
            }
        } else
            return NoReducibilityChecker();
    };


    auto instantiate_rchecker = [&](){
        if constexpr (conf.template flag<SRConfEnum::ReducibilityChecker>())
        {
            auto checker = make_reducibility_checker1<conf.template flag<SRConfEnum::PrettyPrint>(), conf.template flag<SRConfEnum::RC1CheckContext>()>(rr_tree, defs);
            if constexpr (conf.template flag<SRConfEnum::PrettyPrint>())
            {
                std::cout << "  RC(1) match -> {related_rule, first_pos} : " << std::endl;
                checker.template prettyprint<VStr>();
            }
            return checker;
        }
        else
            return NoReducibilityChecker();
    };

    if constexpr (conf.template flag<SRConfEnum::ReducibilityChecker>() || conf.template flag<SRConfEnum::HeuristicCtx>())
    {
        auto h_pre = instantiate_heuristic_pre();
        // Parser init
        // TODO pass h_pre to RC(1)
        return instantiate_parser(instantiate_lookahead(), instantiate_rchecker(), instantiate_ctx_manager(h_pre));
    } else {
        // Parser init
        return instantiate_parser(instantiate_lookahead(), NoReducibilityChecker(), NoReducibilityChecker());
    }
}


#endif //SUPERCFG_PARSER_H
