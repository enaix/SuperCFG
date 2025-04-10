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


template<class CStr, std::size_t... Ints>
constexpr auto build_range(const CStr& s, auto builder, const std::index_sequence<Ints...>)
{
    return builder(CStr::template slice<Ints, 1>()...);
}

/*
 * ==================
 *    JSON PARSER
 * ==================
 *
 *    NOTE: WHITESPACES AND ESCAPE CHARACTERS ARE NOT SUPPORTED!
 *
 */

int main()
{
    constexpr char s[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ _-.!";

    constexpr auto character = NTerm(cs<"char">());

    constexpr auto digit = NTerm(cs<"digit">());
    constexpr auto number = NTerm(cs<"number">());
    constexpr auto boolean = NTerm(cs<"bool">());
    constexpr auto json = NTerm(cs<"json">());
    constexpr auto object = NTerm(cs<"object">());
    constexpr auto null = NTerm(cs<"null">());
    constexpr auto string = NTerm(cs<"string">());
    constexpr auto ws = NTerm(cs<"ws">());
    constexpr auto array = NTerm(cs<"array">());
    constexpr auto member = NTerm(cs<"member">());

    constexpr auto d_character = Define(character, Repeat(build_range(cs<s>(), [](const auto&... str){ return Alter(Term(str)...); }, std::make_index_sequence<sizeof(s)-1>{})));

    constexpr auto d_digit = Define(digit, Repeat(Alter(
        Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()),
        Term(cs<"4">()), Term(cs<"5">()), Term(cs<"6">()),
        Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()),
        Term(cs<"0">())
    )));
    constexpr auto d_number = Define(number, Repeat(digit));

    constexpr auto d_boolean = Define(boolean, Alter(Term(cs<"true">()), Term(cs<"false">())));
    constexpr auto d_null = Define(null, Alter(Term(cs<"null">())));
    constexpr auto d_string = Define(string, Concat(Term(cs<"\"">()), Repeat(character), Term(cs<"\"">())));

    constexpr auto d_ws = Define(ws, Optional(Term(cs<" ">())));

    constexpr auto d_array = Define(array, Concat(Term(cs<"[">()), json, Repeat(Concat(Term(cs<",">()), json)), Term(cs<"]">())));
    constexpr auto d_member = Define(member, Concat(json, Term(cs<":">()), json));
    constexpr auto d_object = Define(object, Concat(Term(cs<"{">()), member, Repeat(Concat(Term(cs<";">()), member)), Term(cs<"}">())));

    constexpr auto d_json = Define(json, Alter(array, boolean, null, number, object, string));

    constexpr auto ruleset = RulesDef(d_character, d_digit, d_number, d_boolean, d_null, d_string, d_ws, d_array, d_member, d_object, d_json);

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
    Tokenizer<256, VStr, TokenType> lexer(ruleset);

    // Generate hashtable for terminals
    auto ht = lexer.init_hashtable();

    while(true)
    {
        VStr input;
        std::cout << "json> ";
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
        ok = parser.run(tree, json, tokens);
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


}