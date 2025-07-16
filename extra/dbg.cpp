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
    constexpr auto conf = mk_sr_parser_conf<SRConfEnum::Lookahead, SRConfEnum::HeuristicCtx>();
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

int main()
{
    if (!test_heuristic_ctx_init()) return 1;
    return 0;
}


#include "extra/prettyprint.h"

