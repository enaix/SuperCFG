//
// Created by Flynn on 27.01.2025.
//

#ifndef SUPERCFG_PARSER_H
#define SUPERCFG_PARSER_H

#include "cfg/preprocess.h"


/**
 * Alter operation solving method
 */
enum class AlterSolver
{
    PickFirst,    /**< Pick the first match and return */
    PickLongest,  /**< Pick the longest match, return error on same length */
    PickLongestF, /**< Pick the longest match, do not perform check */
    Permute       /**< Permute over all possible permutations, starting from root */
};


/**
 * @brief Parser configuration options
 * @tparam alter Alter operation config
 */
template<AlterSolver alter>
struct ParserOptions
{
    static constexpr AlterSolver alter_conf() { return alter; }
};


 /**
  * @brief Tokens parser class
  * @tparam VStr Variable string class
  * @tparam TokenType Nonterminal type (name) container
  * @tparam RulesSymbol Rules class
  * @tparam ParserOpt Parser config
  * @tparam Tree Parser tree node class
  */
template<class VStr, class TokenType, class Tree, class ParserOpt, class RulesSymbol>
class Parser
{
protected:
    //NTermsStorage<TokenType, RulesSymbol> storage;
    NTermsConstHashTable<RulesSymbol> storage;
    using TokenV = Token<VStr, TokenType>;

public:
    constexpr explicit Parser(const RulesSymbol& rules) : storage(rules) {}

    /**
     * @brief Recursively parse tokens and build parse tree
     * @param node Empty tree to populate
     * @param root Rules starting point
     * @param tokens List of tokens from Tokenizer
     */
    template<class RootSymbol>
    bool run(Tree& node, const RootSymbol& root, const std::vector<TokenV>& tokens) const
    {
        std::size_t index = 0;
        return parse(root, node, index, tokens);
    }

protected:
    template<class TSymbol>
    bool parse(const TSymbol& symbol, Tree& node, std::size_t& index, const std::vector<TokenV>& tokens) const
    {
        if (index >= tokens.size()) return false;

        // Iterate over each operator
        if constexpr (is_operator<TSymbol>())
        {
            std::cout << "op [" << int(get_operator<TSymbol>()) << "] at i=" << index << " tok=" << tokens[index].value << std::endl;
            if constexpr (get_operator<TSymbol>() == OpType::Concat)
            {
                std::size_t index_stack = index;
                std::cout << "concat : ";
                // Iterate over each node, index is moved each time. No need to copy node on stack
                return symbol.each_or_exit([&](const auto& s) -> bool {
                    if (!parse(s, node, index, tokens))
                    {
                        std::cout << "concat failed" << std::endl;
                        index = index_stack; // Revert the first index
                        return false; // Didn't find anything
                    }
                    return true; // Continue
                });
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Alter)
            {
                return parse_alter(symbol, node, index, tokens);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Optional)
            {
                // Try to match if we can, just return true anyway
                Tree node_stack = node;
                if (parse(std::get<0>(symbol.terms), node_stack, index, tokens)) node = node_stack;
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Repeat)
            {
                Tree node_stack = node;
                std::cout << "repeat : ";
                while (parse(std::get<0>(symbol.terms), node_stack, index, tokens))
                {
                    std::cout << "repeat success" << std::endl;
                    // Update stack when we succeed
                    node = node_stack;
                }
                std::cout << "repeat fail, exiting, i=" << index << std::endl;
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Group)
            {
                return parse(std::get<0>(symbol.terms), node, index, tokens);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Except)
            {
                std::size_t i = index;
                Tree node_stack = node;
                if (parse(std::get<0>(symbol.terms), node_stack, index, tokens))
                {
                    // Check if the symbol is an exception
                    if (!parse(std::get<1>(symbol.terms), Tree(), i, tokens))
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
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) { index = index_stack; return false; }
                }
                // We don't need to check the next operator, since it may start from this token
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatGE)
            {
                std::size_t index_stack = index;
                for (std::size_t i = 0; i < get_repeat_times(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) { index = index_stack; return false; }
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
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) { index = index_stack; return false; }
                }
                Tree node_stack = node;
                for (std::size_t i = get_range_from(symbol); i < get_range_to(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) return true;
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
            std::cout << "nt [" << symbol.name.c_str() << "] at i=" << index << " tok=" << tokens[index].value << std::endl;
            // Get definition and get rules for the non-terminal
            const auto& s = std::get<1>(storage.get(symbol)->terms);
            // Create and pass a new leaf node
            return parse(s, symbol.template create_node<Tree>(node), index, tokens);

        } else if constexpr (is_term<TSymbol>()) {
            std::cout << "t  [" << symbol.name.c_str() << "] at i=" << index << " tok=" << tokens[index].value << std::endl;
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
    inline bool parse_alter(const TSymbol& symbol, Tree& node, std::size_t& index, const std::vector<TokenV>& tokens) const
    {
        if constexpr (ParserOpt::alter_conf() == AlterSolver::PickFirst)
        {
            // Check any of these nodes
            std::size_t i = index;

            std::cout << "alter : ";
            // Return true if we match at least one
            // (a+a)+b - in a+a both 'a' and 'a+a' may evaluate to true, so we cannot get the first match
            return !symbol.each_or_exit([&](const auto& s) -> bool {
                // Apply index and return
                Tree node_stack = node;
                if (parse(s, node_stack, i, tokens))
                {
                    std::cout << "alter finished" << std::endl;
                    node = node_stack; // Apply changes
                    index = i;
                    return false; // Exit from callback
                }
                std::cout << ".";
                return true; // Continue
            });
        }
    }
};



#endif //SUPERCFG_PARSER_H
