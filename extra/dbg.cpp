//
// Created by Flynn on 14.07.2025.
//


#include <iostream>

#include "cfg/gbnf.h"
#include "cfg/containers.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/str.h"
#include "cfg/preprocess_factories.h"
#include "extra/prettyprint.h"

// Test grammar
bool test_heuristic_ctx_init()
{
    std::cout << "test_heuristic_ctx_init() :" << std::endl;

    //constexpr EBNFBakery rules;
    //    constexpr auto nozero = NTerm(cs("digit excluding zero"));
    //    constexpr auto d_nozero = Define(nozero, );
    constexpr auto ch = NTerm(cs<"char">());
    constexpr auto d_ch = Define(ch, Repeat(TermsRange(cs<"a">(), cs<"z">())));

    constexpr auto str = NTerm(cs<"string">());
    constexpr auto d_str = Define(str, Repeat(ch));
    constexpr auto op = NTerm(cs<"op">()); // any operator
    constexpr auto group = NTerm(cs<"group">());
    constexpr auto array = NTerm(cs<"array">());

    constexpr auto d_group = Define(group, Concat(Term(cs<"(">()), op, Repeat(Concat(Term(cs<",">()), op)), Term(cs<")">())));
    constexpr auto d_array = Define(array, Concat(Term(cs<"[">()), op, Repeat(Concat(Term(cs<",">()), op)), Term(cs<"]">())));
    constexpr auto d_op = Define(op, Alter(str, group, array));

    constexpr auto ruleset = RulesDef(d_ch, d_str, d_op, d_group, d_array);

    using VStr = StdStr<char>;
    using TokenType = StdStr<char>;

    //std::cout << ruleset.bake(rules) << std::endl;
    // Parser classes init
    // ===================

    auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::AdvancedLexer, LexerConfEnum::HandleDuplicates>());
    //LexerLegacy<VStr, TokenType> lexer(ruleset); // Lexer init

    // Init prettyprinter
    PrettyPrinter printer;

    // Parser init
    constexpr auto conf = mk_sr_parser_conf<SRConfEnum::PrettyPrint, SRConfEnum::Lookahead, SRConfEnum::HeuristicCtx>();
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, lexer, conf, printer);

    //while (printer.process()) {}

    //
    StdStr<char> in("(abc,asdf,[a,(gfds,sdf)])");
    bool ok;
    auto tokens = lexer.run(in, ok);

    if (!ok)
    {
        std::cout << "lexer build error" << std::endl;
        return false;
    }

    TreeNode<VStr> tree;
    std::cout << "======" << std::endl << "SR parser routine : " << std::endl;
    ok = parser.run(tree, op, tokens, printer);

    std::cout << "======" << std::endl << "parser output : " << std::endl;
    tree.traverse([&](const auto& node, std::size_t depth){
        for (std::size_t i = 0; i < depth; i++) std::cout << "|  ";
        std::cout << node.name << " (" << node.nodes.size() << " elems) : " << node.value << std::endl;
    });

    if (!ok)
    {
        std::cout << "parser error" << std::endl;
        return false;
    }
    return true;
}

/*bool test_heuristic_ctx_json()
{
    std::cout << "test_heuristic_ctx_json() :" << std::endl;

    constexpr auto character = NTerm(cs<"char">());

    constexpr auto digit = NTerm(cs<"digit">());
    constexpr auto number = NTerm(cs<"number">());
    constexpr auto boolean = NTerm(cs<"bool">());
    constexpr auto json = NTerm(cs<"json">());
    constexpr auto object = NTerm(cs<"object">());
    constexpr auto null = NTerm(cs<"null">());
    constexpr auto string = NTerm(cs<"string">());
    //constexpr auto ws = NTerm(cs<"ws">());
    constexpr auto array = NTerm(cs<"array">());
    constexpr auto member = NTerm(cs<"member">());

    //constexpr auto d_character = Define(character, Repeat(build_range(cs<s>(), [](const auto&... str){ return Alter(Term(str)...); }, std::make_index_sequence<sizeof(s)-1>{})));
    constexpr auto d_character = Define(character, Repeat(Alter(TermsRange(cs<"-">(), cs<"~">())), Term(cs<" ">())));

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

    //constexpr auto d_ws = Define(ws, Optional(Term(cs<" ">())));

    constexpr auto d_array = Define(array, Concat(Term(cs<"[">()), json, Repeat(Concat(Term(cs<",">()), json)), Term(cs<"]">())));
    constexpr auto d_member = Define(member, Concat(json, Term(cs<":">()), json));
    constexpr auto d_object = Define(object, Concat(Term(cs<"{">()), member, Repeat(Concat(Term(cs<",">()), member)), Term(cs<"}">())));

    constexpr auto d_json = Define(json, Alter(array, boolean, null, number, object, string));

    constexpr auto ruleset = RulesDef(d_character, d_digit, d_number, d_boolean, d_null, d_string, d_array, d_member, d_object, d_json);


    using VStr = StdStr<char>;
    using TokenType = StdStr<char>;

    //std::cout << ruleset.bake(rules) << std::endl;
    // Parser classes init
    // ===================

    auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::AdvancedLexer, LexerConfEnum::HandleDuplicates>());
    //LexerLegacy<VStr, TokenType> lexer(ruleset); // Lexer init

    // Init prettyprinter
    PrettyPrinter printer;

    // Parser init
    constexpr auto conf = mk_sr_parser_conf<SRConfEnum::PrettyPrint, SRConfEnum::Lookahead, SRConfEnum::HeuristicCtx>();
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, lexer, conf, printer);

    //while (printer.process()) {}

    //
    StdStr<char> in("{'a': 1, 'b': [42]}");
    bool ok;
    auto tokens = lexer.run(in, ok);

    if (!ok)
    {
        std::cout << "lexer build error" << std::endl;
        return false;
    }

    TreeNode<VStr> tree;
    std::cout << "======" << std::endl << "SR parser routine : " << std::endl;
    ok = parser.run(tree, json, tokens, printer);

    std::cout << "======" << std::endl << "parser output : " << std::endl;
    tree.traverse([&](const auto& node, std::size_t depth){
        for (std::size_t i = 0; i < depth; i++) std::cout << "|  ";
        std::cout << node.name << " (" << node.nodes.size() << " elems) : " << node.value << std::endl;
    });

    if (!ok)
    {
        std::cout << "parser error" << std::endl;
        return false;
    }
    return true;
}*/

int main()
{
    //test_heuristic_ctx_json();
    test_heuristic_ctx_init();
    return 0;
}


