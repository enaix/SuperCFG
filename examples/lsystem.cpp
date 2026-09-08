#include <iostream>
#include <chrono>

#include "cfg/gbnf.h"
#include "cfg/str.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/containers.h"
#include "cfg/tiling.h"

#include "extra/superdbg.h"


int lsystem_tiling()
{
    // Binary tree
    constexpr auto zero = NTerm(cs<"zero">());
    constexpr auto one = NTerm(cs<"one">());
    constexpr auto d_zero = Define(zero, Alter(Term(cs<"0">()), Concat(Term(cs<"1">()), Term(cs<"[">()), Term(cs<"0">()), Term(cs<"]">()), Term(cs<"0">()))));
    constexpr auto d_one = Define(one, Alter(Term(cs<"1">()), Concat(Term(cs<"1">()), Term(cs<"1">()))));
    constexpr auto ruleset = RulesDef(d_zero, d_one);

    constexpr auto d_zero_short = Define(zero, Alter(Term(cs<"0">()), Term(cs<"1[0]0">())));
    constexpr auto d_one_short = Define(one, Alter(Term(cs<"1">()), Term(cs<"11">())));
    constexpr auto ruleset_short = RulesDef(d_zero_short, d_one_short);

    using VStr = StdStr<char>; // Variable string class inherited from std::string<TChar>
    using TokenType = StdStr<char>; // Class used for storing a token type in runtime

    // TreeNode<VStr> is the AST class
    auto parser = TilingParser<VStr, TreeNode<VStr>, std::decay_t<decltype(ruleset_short)>>(ruleset_short);

    while(true)
    {
        VStr input;
        std::cout << "lsys> ";
        std::cout.flush();
        std::cin >> input;

        bool ok = true;

        // Create a parse tree
        TreeNode<VStr> tree;

        volatile std::chrono::steady_clock::time_point p_start = std::chrono::steady_clock::now();
        parser.run(tree, input, ok);
        volatile std::chrono::steady_clock::time_point p_end = std::chrono::steady_clock::now();

        if (!ok) {
            std::cout << "lsystem_tiling() : parser failed" << std::endl;
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

        std::cout << "lsystem_tiling() : elapsed (std::chrono overhead)" << std::endl
        << "  tiling : " << std::chrono::duration_cast<std::chrono::milliseconds>(*const_cast<std::chrono::steady_clock::time_point*>(&p_end) -
                                                                                *const_cast<std::chrono::steady_clock::time_point*>(&p_start)).count() << " ms" << std::endl;
    }
}

int lsystem()
{
    // Binary tree
    constexpr auto zero = NTerm(cs<"zero">());
    constexpr auto one = NTerm(cs<"one">());
    constexpr auto d_zero = Define(zero, Alter(Term(cs<"0">()), Concat(one, Term(cs<"[">()), zero, Term(cs<"]">()))));
    constexpr auto d_one = Define(one, Alter(Term(cs<"1">()), Concat(one, one)));
    constexpr auto ruleset = RulesDef(d_zero, d_one);

    // Target string: 1111[11[1[0]0]1[0]0]11[1[0]0]1[0]0
    // Common subsequences of len 2 (with step over):
    // 11 : 4 (store inclusion positions)
    // 1[ : 7 - *1
    // [1 : 4
    // ]1 : 3
    // [0; 0]; - *1
    // *1 - set of mut. excl. positions
    // Strong condition: two substrings are mut.excl iff \exists s1={i1, j1}, s2={i2, j2}, s1\ne s2, |s1|<|s2|: i1>=i2 && j1<=j2
    //
    // Grammar generator: need to obtain 2 rules
    // Deduce the list of variables and constants, supposedly randomly
    //
    // pick two longest repeating strings
    //

    using VStr = StdStr<char>; // Variable string class inherited from std::string<TChar>
    using TokenType = StdStr<char>; // Class used for storing a token type in runtime

    // Configure the parser with desired options
    constexpr auto conf = mk_sr_parser_conf<
        //SRConfEnum::PrettyPrint  // Enable pretty printing for debugging
        //SRConfEnum::Lookahead// Enable lookahead(1)
        >();//SRConfEnum::HeuristicCtx>();

    // Initialize the tokenizer
    auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::AdvancedLexer, LexerConfEnum::HandleDuplicates>());

    //DBGPrinter printer;
    NoPrettyPrinter printer;
    //printer.init_signal_handler();

    // Create the shift-reduce parser
    // TreeNode<VStr> is the AST class
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, lexer, conf, printer);

    while(true)
    {
        VStr input;
        std::cout << "lsys> ";
        std::cout.flush();
        std::cin >> input;

        bool ok;

        // Tokenize the input
        volatile std::chrono::steady_clock::time_point lex_start = std::chrono::steady_clock::now();
        auto tokens = lexer.run(input, ok);
        volatile std::chrono::steady_clock::time_point lex_end = std::chrono::steady_clock::now();

        if (!ok) {
            std::cout << "lsystem() : lexer failed" << std::endl;
            //return 1;
        }

        // Create a parse tree
        TreeNode<VStr> tree;

        volatile std::chrono::steady_clock::time_point p_start = std::chrono::steady_clock::now();
        ok = parser.run(tree, zero, tokens, printer);
        volatile std::chrono::steady_clock::time_point p_end = std::chrono::steady_clock::now();

        if (!ok) {
            std::cout << "lsystem() : parser failed" << std::endl;
            //return 1;
        }

        // Process the parse tree
        tree.traverse([&](const auto& node, std::size_t depth) {
            // Print the tree structure
            for (std::size_t i = 0; i < depth; i++)
                std::cout << "|  ";
            std::cout << node.name << " (" << node.nodes.size()
                      << " elems) : " << node.value << std::endl;
        });

        std::cout << "lsystem() : elapsed (std::chrono overhead)" << std::endl
                  << "  lexer : " << std::chrono::duration_cast<std::chrono::milliseconds>(*const_cast<std::chrono::steady_clock::time_point*>(&lex_end) -
                                                                                          *const_cast<std::chrono::steady_clock::time_point*>(&lex_start)).count() << " ms" << std::endl
        << "  sr(1) : " << std::chrono::duration_cast<std::chrono::milliseconds>(*const_cast<std::chrono::steady_clock::time_point*>(&p_end) -
                                                                                *const_cast<std::chrono::steady_clock::time_point*>(&p_start)).count() << " ms" << std::endl;
    }


    return 0;
}


int main()
{
    //return lsystem_tiling();
    return lsystem();
}
