//
// Created by Flynn on 27.01.2025.
//

#ifndef SUPERCFG_PARSER_H
#define SUPERCFG_PARSER_H

#include "cfg/preprocess.h"


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
    template<class RootSymbol>
    bool run(Tree& node, const RootSymbol& root, const std::vector<TokenV>& tokens)
    {
        std::size_t index = 0;
        return parse(root, node, index, tokens, 0);
    }

protected:
    template<class TSymbol>
    bool parse(const TSymbol& symbol, Tree& node, std::size_t& index, const std::vector<TokenV>& tokens, std::size_t depth)
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

    template<class TSymbol>
    inline bool parse_alter(const TSymbol& symbol, Tree& node, std::size_t& index, const std::vector<TokenV>& tokens, std::size_t depth)
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


template<class VStr, class TokenType, class Tree, class RulesSymbol>
class SRParser
{
protected:
    NTermsStorage<TokenType, RulesSymbol> storage;
    //NTermsConstHashTable<RulesSymbol> storage;
    using TokenV = Token<VStr, TokenType>;
public:
    constexpr explicit SRParser(const RulesSymbol& rules) : storage(rules) {}
    // Construct reverse tree (mapping TokenType -> tuple(NTerms)), in which nterms is it contained

    template<class RootSymbol>
    bool run(Tree& node, const RootSymbol& root, std::vector<TokenV>& tokens)
    {
        std::vector<TokenV> stack;
    }

protected:
    bool reduce(std::vector<TokenV>& stack, std::vector<TokenV>& tokens)
    {
        // Get rightmost token type (handle)
        // check shift_reduce_parser_notes.txt
    }
};



#endif //SUPERCFG_PARSER_H
