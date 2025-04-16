//
// Created by Flynn on 10.04.2025.
//
#include <iostream>
#include <chrono>

#include "cfg/gbnf.h"
#include "cfg/str.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/containers.h"


int main()
{
    constexpr auto digit = NTerm(cs<"digit">());
    constexpr auto d_digit = Define(digit, Repeat(Alter(
        Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()),
        Term(cs<"4">()), Term(cs<"5">()), Term(cs<"6">()),
        Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()),
        Term(cs<"0">())
    )));

    constexpr auto number = NTerm(cs<"number">());
    constexpr auto d_number = Define(number, Repeat(digit));

    constexpr auto add = NTerm(cs<"add">());
    constexpr auto sub = NTerm(cs<"sub">());
    constexpr auto mul = NTerm(cs<"mul">());
    constexpr auto div = NTerm(cs<"div">());
    constexpr auto op = NTerm(cs<"op">());
    constexpr auto arithmetic = NTerm(cs<"arithmetic">());
    constexpr auto group = NTerm(cs<"group">());

    // No operator order defined: grammar is ambiguous
    constexpr auto d_add = Define(add, Concat(op, Term(cs<"+">()), op));
    constexpr auto d_sub = Define(sub, Concat(op, Term(cs<"-">()), op));
    constexpr auto d_mul = Define(mul, Concat(op, Term(cs<"*">()), op));
    constexpr auto d_div = Define(div, Concat(op, Term(cs<"/">()), op));

    constexpr auto d_group = Define(group, Concat(Term(cs<"(">()), op, Term(cs<")">())));
    constexpr auto d_arithmetic = Define(arithmetic, Alter(add, sub, mul, div));
    constexpr auto d_op = Define(op, Alter(number, arithmetic, group));

    constexpr auto ruleset = RulesDef(d_digit, d_number, d_add, d_sub, d_mul, d_div,
                                     d_arithmetic, d_op, d_group);

    using VStr = StdStr<char>; // Variable string class inherited from std::string<TChar>
    using TokenType = StdStr<char>; // Class used for storing a token type in runtime

    // Configure the parser with desired options
    constexpr auto conf = mk_sr_parser_conf<
        SRConfEnum::PrettyPrint,  // Enable pretty printing for debugging
        SRConfEnum::Lookahead>(); // Enable lookahead(1)

    // Create the shift-reduce parser
    // TreeNode<VStr> is the AST class
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, conf);

    // Initialize the tokenizer
    LexerLegacy<VStr, TokenType> lexer(ruleset);

    // Generate hashtable for terminals
    auto ht = lexer.init_hashtable();

    while(true)
    {
        VStr input;
        std::cout << "calc> ";
        std::cout.flush();
        std::cin >> input;

        bool ok;

        // Tokenize the input
        volatile std::chrono::steady_clock::time_point lex_start = std::chrono::steady_clock::now();
        auto tokens = lexer.run(ht, input, ok);
        volatile std::chrono::steady_clock::time_point lex_end = std::chrono::steady_clock::now();

        if (!ok) {
            std::cout << "main() : lexer failed" << std::endl;
            return 1;
        }

        // Create a parse tree
        TreeNode<VStr> tree;

        volatile std::chrono::steady_clock::time_point p_start = std::chrono::steady_clock::now();
        ok = parser.run(tree, op, tokens);
        volatile std::chrono::steady_clock::time_point p_end = std::chrono::steady_clock::now();

        if (!ok) {
            std::cout << "main() : parser failed" << std::endl;
            return 1;
        }

        // Process the parse tree
        tree.traverse([&](const auto& node, std::size_t depth) {
            // Print the tree structure
            for (std::size_t i = 0; i < depth; i++)
                std::cout << "|  ";
            std::cout << node.name << " (" << node.nodes.size()
                      << " elems) : " << node.value << std::endl;
        });

        std::cout << "main() : elapsed (std::chrono overhead)" << std::endl
                  << "  lexer : " << std::chrono::duration_cast<std::chrono::milliseconds>(*const_cast<std::chrono::steady_clock::time_point*>(&lex_end) -
                                                                                          *const_cast<std::chrono::steady_clock::time_point*>(&lex_start)).count() << " ms" << std::endl
        << "  sr(1) : " << std::chrono::duration_cast<std::chrono::milliseconds>(*const_cast<std::chrono::steady_clock::time_point*>(&p_end) -
                                                                                *const_cast<std::chrono::steady_clock::time_point*>(&p_start)).count() << " ms" << std::endl;
    }


    return 0;
}