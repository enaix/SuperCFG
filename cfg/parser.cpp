//
// Created by Flynn on 27.01.2025.
//

#include "parser.h"

template<class VStr, class TokenType, class RulesSymbol, class Tree>
template<class TSymbol>
bool Parser<VStr, TokenType, RulesSymbol, Tree>::parse(const RulesSymbol& symbols, const TSymbol& symbol, const Tree& node,
                                                  size_t& index, const std::vector<TokenV>& tokens)
{
    // Iterate over each operator
    if constexpr (is_operator(symbol))
    {
        if constexpr (get_operator(symbol) == OpType::Concat)
        {
            // Iterate over each node, index is moved each time. No need to copy node on stack
            symbol.each([&](const auto& s){
                if (!parse(symbols, s, node, index, tokens)) return false;
            });
        }
        else if constexpr (get_operator(symbol) == OpType::Alter)
        {
            // Check any of these nodes
            std::size_t i = index;
            symbol.each([&](const auto& s){
                // Apply index and return
                Tree node_stack = node;
                if (parse(symbols, s, node_stack, i, tokens))
                {
                    node = node_stack; // Apply changes
                    index = i;
                    return true;
                }
            });
            return false; // Nothing has matched
        }
        else if constexpr (get_operator(symbol) == OpType::Optional)
        {
            // Try to match if we can, just return true anyway
            Tree node_stack = node;
            if (parse(symbols, std::get<0>(symbol.terms), node_stack, index, tokens)) node = node_stack;
            return true;
        }
        else if constexpr (get_operator(symbol) == OpType::Repeat)
        {
            Tree node_stack = node;
            while (parse(symbols, std::get<0>(symbol.terms), node_stack, index, tokens)) node = node_stack;
            return true;
        }
        else if constexpr (get_operator(symbol) == OpType::Group)
        {
            return parse(symbols, std::get<0>(symbol.terms), node, index, tokens);
        }
        else if constexpr (get_operator(symbol) == OpType::Except)
        {
            std::size_t i = index;
            Tree node_stack = node;
            if (parse(symbols, std::get<0>(symbol.terms), node_stack, index, tokens))
            {
                // Check if the symbol is an exception
                if (!parse(symbols, std::get<1>(symbol.terms), Tree(), i, tokens))
                {
                    node = node_stack;
                    return true;
                }
            }
            return false;
        }
        else
        {
            // Implement RepeatExact, GE, Range and handle exceptions
            // static_assert(false, "Operator not implemented");
        }

    } else if constexpr (is_nterm(symbol)) {
        // Get definition and get rules for the non-terminal
        const auto& s = std::get<1>(storage.get(tokens[index].type)->terms);
        // Create and pass a new leaf node
        return parse(symbols, s, s.create_node(node), index, tokens);

    } else if constexpr (is_term(symbol)) {
        if (index >= tokens.size()) [[unlikely]] abort();

        if (tokens[index].value == symbol.name)
        {
            index++; // Move the pointer only if we succeed
            return true;
        }
        return false;
    } else static_assert(is_term(symbol) || is_nterm(symbol) || is_operator(symbol), "Wrong symbol type");

    return true;
}