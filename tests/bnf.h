//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_BNF_H
#define SUPERCFG_BNF_H

#include <iostream>

#include "cfg/gbnf.h"
#include "cfg/containers.h"
#include "cfg/base.h"
#include "cfg/parser.h"
#include "cfg/str.h"
#include "cfg/preprocess_factories.h"


bool test_gbnf_basic()
{
    std::cout << "test_gbnf_basic() :" << std::endl;

    constexpr EBNFBakery rules;
    constexpr auto test = NTerm(cs<"abcd">()).bake(rules);
    std::cout << test.c_str() << std::endl;
    constexpr auto true1 = cs<"abcd">();
    static_assert(test == true1);

    constexpr auto test2 = Term(cs<"abcd">()).bake(rules);
    std::cout << test2.c_str() << std::endl;

//    constexpr auto test3 = BinaryOP<Alter>(Term(cs("abcd")), Term(cs("abcde"))).bake(rules);
    constexpr auto test3 = Alter(Term(cs<"abcd">()), Term(cs<"abcde">())).bake(rules);
    std::cout << test3.c_str() << std::endl;

    return true;
}

bool test_gbnf_complex1()
{
    std::cout << "test_gbnf_complex1() :" << std::endl;

    constexpr EBNFBakery rules;
    constexpr auto term1 = NTerm(cs<"A">());
    constexpr auto term2 = NTerm(cs<"B">());
    constexpr auto term3 = Term(cs<"42">());
    constexpr auto term4 = Term(cs<"xyz">());
    constexpr auto term5 = Term(cs<"!">());

    constexpr auto res = Optional(
        Concat(Group(Repeat(Except(term1, term5)), term2), term3), term4
    ).bake(rules);

    std::cout << res.c_str() << std::endl;

    constexpr auto nozero = NTerm(cs<"digit excluding zero">());
    constexpr auto d_nozero = Define(nozero, Alter(Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), Term(cs<"4">()), Term(cs<"5">()),
                                             Term(cs<"6">()), Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">())));
    constexpr auto d_digit = Define(NTerm(cs<"digit">()), Alter(Term(cs<"0">()), nozero));
    constexpr auto root = RulesDef(d_nozero, d_digit).bake(rules);

    constexpr char digit_check[] = "digit excluding zero = \"1\" | \"2\" | \"3\" | \"4\" | \"5\" | \"6\" | \"7\" | \"8\" | \"9\" ;\n"
                                   "digit = \"0\" | digit excluding zero ;";
    static_assert(cs<digit_check>() == root, "test_gbnf_complex1() : digits : deviation from vanilla EBNF");

    std::cout << root.c_str() << std::endl;

    return true;
}

bool test_gbnf_extended()
{
    std::cout << "test_gbnf_extended() :" << std::endl;

    constexpr EBNFBakery rules;
    constexpr ExtEBNFBakery rules_ext;
    constexpr auto ex1 = RepeatExact<5>(Term(cs<"abc">())).bake(rules);
    constexpr auto ex1e = RepeatExact<5>(Term(cs<"abc">())).bake(rules_ext);
    std::cout << ex1.c_str() << std::endl << ex1e.c_str() << std::endl;

    constexpr auto ex2 = RepeatGE<4>(Term(cs<"abcd">())).bake(rules);
    constexpr auto ex2e = RepeatGE<4>(Term(cs<"abcd">())).bake(rules_ext);
    std::cout << ex2.c_str() << std::endl << ex2e.c_str() << std::endl;

    constexpr auto ex_r1 = RepeatRange<2, 5>(Term(cs<"a">()));
    constexpr auto ex_r1a = ex_r1.bake(rules);
    constexpr auto ex_r1b = ex_r1.bake(rules_ext);
    std::cout << ex_r1a.c_str() << std::endl << ex_r1b.c_str() << std::endl;

    constexpr auto ex_r2 = RepeatRange<0, 7>(Term(cs<"42">()));
    constexpr auto ex_r2a = ex_r2.bake(rules);
    constexpr auto ex_r2b = ex_r2.bake(rules_ext);
    std::cout << ex_r2a.c_str() << std::endl << ex_r2b.c_str() << std::endl;

    return true;
}

bool test_gbnf_parse_1()
{
    std::cout << "test_gbnf_parse_1() :" << std::endl;

    constexpr EBNFBakery rules;
    constexpr auto d_digit = Define(NTerm(cs<"digit">()), Repeat(Alter(Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), Term(cs<"4">()), Term(cs<"5">()),
                                                                    Term(cs<"6">()), Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()), Term(cs<"0">()))));
    constexpr auto root = RulesDef(d_digit);

    auto lexer = make_lexer<StdStr<char>, StdStr<char>>(root, mk_lexer_conf<LexerConfEnum::Legacy>());
    //LexerLegacy<StdStr<char>, StdStr<char>> lexer(root);
    StdStr<char> in("1452");
    bool ok;
    std::cout << "======" << std::endl << "terminals hashtable : " << std::endl;
    for (const auto& kv : lexer.ht)
        std::cout << kv.first << ": " << kv.second << std::endl;

    auto res = lexer.run(in, ok);

    std::cout << "======" << std::endl << "lexer output : " << std::endl;
    for (const auto& tok : res)
        std::cout << "<" << tok.type << ">(" << tok.value << "), ";
    std::cout << std::endl;
    if (!ok)
    {
        std::cout << "lexer build error" << std::endl;
        return false;
    }

    LL1Parser<StdStr<char>, StdStr<char>, TreeNode<StdStr<char>>, LL1ParserOptions<LL1AlterSolver::PickFirst>, decltype(root)> parser(root);
    TreeNode<StdStr<char>> tree;
    std::cout << "======" << std::endl << "parser output : " << std::endl;
    ok = parser.run(tree, NTerm(cs<"digit">()), res);
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

bool test_gbnf_parse_calc()
{
    std::cout << "test_gbnf_parse_calc() :" << std::endl;

    constexpr EBNFBakery rules;
//    constexpr auto nozero = NTerm(cs("digit excluding zero"));
//    constexpr auto d_nozero = Define(nozero, );
    constexpr auto digit = NTerm(cs<"digit">());
    constexpr auto d_digit = Define(digit, Alter(Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), Term(cs<"4">()), Term(cs<"5">()),
                                                        Term(cs<"6">()), Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()), Term(cs<"0">())));

    constexpr auto number = NTerm(cs<"number">());
    constexpr auto d_number = Define(number, Concat(digit, Repeat(digit)));
    constexpr auto add = NTerm(cs<"add">());
    constexpr auto sub = NTerm(cs<"sub">());
    constexpr auto mul = NTerm(cs<"mul">());
    constexpr auto div = NTerm(cs<"div">());
    constexpr auto op = NTerm(cs<"op">()); // any operator
    constexpr auto arithmetic = NTerm(cs<"arithmetic">());

    constexpr auto d_add = Define(add, Concat(number, Term(cs<"+">()), number));
    constexpr auto d_sub = Define(sub, Concat(number, Term(cs<"-">()), number));
    constexpr auto d_mul = Define(mul, Concat(number, Term(cs<"*">()), number));
    constexpr auto d_div = Define(div, Concat(number, Term(cs<"/">()), number));

    constexpr auto d_arithmetic = Define(arithmetic, Alter(add, sub, mul, div));
    constexpr auto d_op = Define(op, Alter(number, arithmetic));

    constexpr auto ruleset = RulesDef(d_digit, d_number, d_add, d_sub, d_mul, d_div, d_arithmetic, d_op);

    auto bake = ruleset.bake(rules);
    //std::cout << res.c_str() << std::endl;

    auto lexer = make_lexer<StdStr<char>, StdStr<char>>(ruleset, mk_lexer_conf<LexerConfEnum::Legacy>());
    // LexerLegacy<StdStr<char>, StdStr<char>> lexer(ruleset);
    std::cout << "======" << std::endl << "terminals hashtable : " << std::endl;
    for (const auto& kv : lexer.ht)
        std::cout << kv.first << ": " << kv.second << std::endl;

    StdStr<char> in("5*1234");
    bool ok;
    auto res = lexer.run(in, ok);

    std::cout << "======" << std::endl << "lexer output : " << std::endl;
    for (const auto& tok : res)
        std::cout << "<" << tok.type << "> ";
    std::cout << std::endl;

    for (const auto& tok : res)
        std::cout << "<" << tok.value << "> ";
    std::cout << std::endl;

    if (!ok)
    {
        std::cout << "lexer build error" << std::endl;
        return false;
    }

    LL1Parser<StdStr<char>, StdStr<char>, TreeNode<StdStr<char>>, LL1ParserOptions<LL1AlterSolver::PickLongest>, decltype(ruleset)> parser(ruleset);

    TreeNode<StdStr<char>> tree;
    std::cout << "======" << std::endl << "parser output : " << std::endl;
    ok = parser.run(tree, op, res);
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

bool test_sr_init()
{
    std::cout << "test_sr_init() :" << std::endl;

    constexpr EBNFBakery rules;

    constexpr auto digit = NTerm(cs<"digit">());
    constexpr auto digit4 = NTerm(cs<"digit4">());
    constexpr auto d_digit = Define(digit, Repeat(Alter(Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), Term(cs<"4">()), Term(cs<"5">()),
                                                                Term(cs<"6">()), Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()), Term(cs<"0">()))));
    constexpr auto d_digit4 = Define(digit4, Repeat(digit));
    constexpr auto root = RulesDef(d_digit, d_digit4);

    using VStr = StdStr<char>;
    using TokenType = StdStr<char>;

    // Parser classes init
    // ===================

    //LexerLegacy<VStr, TokenType> lexer(root); // Lexer init
    auto lexer = make_lexer<VStr, TokenType>(root, mk_lexer_conf<LexerConfEnum::Legacy>());

    // Parser init
    constexpr auto conf = mk_sr_parser_conf<SRConfEnum::PrettyPrint, SRConfEnum::Lookahead>();
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(root, lexer, conf);

    StdStr<char> in("1452");
    bool ok;
    auto tokens = lexer.run(in, ok);

    if (!ok)
    {
        std::cout << "lexer build error" << std::endl;
        return false;
    }

    TreeNode<VStr> tree;
    std::cout << "======" << std::endl << "SR parser routine : " << std::endl;
    ok = parser.run(tree, digit4, tokens);

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


bool test_sr_calc()
{
    std::cout << "test_sr_calc() :" << std::endl;

    constexpr EBNFBakery rules;
    //    constexpr auto nozero = NTerm(cs("digit excluding zero"));
    //    constexpr auto d_nozero = Define(nozero, );
    constexpr auto digit = NTerm(cs<"digit">());
    constexpr auto d_digit = Define(digit, Repeat(Alter(Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), Term(cs<"4">()), Term(cs<"5">()),
                                                        Term(cs<"6">()), Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()), Term(cs<"0">()))));

    constexpr auto number = NTerm(cs<"number">());
    constexpr auto d_number = Define(number, Repeat(digit));
    constexpr auto add = NTerm(cs<"add">());
    constexpr auto sub = NTerm(cs<"sub">());
    constexpr auto mul = NTerm(cs<"mul">());
    constexpr auto div = NTerm(cs<"div">());
    constexpr auto op = NTerm(cs<"op">()); // any operator
    constexpr auto arithmetic = NTerm(cs<"arithmetic">());
    constexpr auto group = NTerm(cs<"group">());

    // There is no operator precedence calculation
    constexpr auto d_add = Define(add, Concat(op, Term(cs<"+">()), op));
    constexpr auto d_sub = Define(sub, Concat(op, Term(cs<"-">()), op));
    constexpr auto d_mul = Define(mul, Concat(op, Term(cs<"*">()), op));
    constexpr auto d_div = Define(div, Concat(op, Term(cs<"/">()), op));

    constexpr auto d_group = Define(group, Concat(Term(cs<"(">()), op, Term(cs<")">())));
    constexpr auto d_arithmetic = Define(arithmetic, Alter(add, sub, mul, div));
    constexpr auto d_op = Define(op, Alter(number, arithmetic, group));

    constexpr auto ruleset = RulesDef(d_digit, d_number, d_add, d_sub, d_mul, d_div, d_arithmetic, d_op, d_group);

    using VStr = StdStr<char>;
    using TokenType = StdStr<char>;

    std::cout << ruleset.bake(rules) << std::endl;
    // Parser classes init
    // ===================

    auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::Legacy>());
    //LexerLegacy<VStr, TokenType> lexer(ruleset); // Lexer init

    // Parser init
    constexpr auto conf = mk_sr_parser_conf<SRConfEnum::PrettyPrint, SRConfEnum::Lookahead>();
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, lexer, conf);

    StdStr<char> in("12*(3+42)");
    bool ok;
    auto tokens = lexer.run(in, ok);

    if (!ok)
    {
        std::cout << "lexer build error" << std::endl;
        return false;
    }

    TreeNode<VStr> tree;
    std::cout << "======" << std::endl << "SR parser routine : " << std::endl;
    ok = parser.run(tree, op, tokens);

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


bool test_adv_lexer()
{
    std::cout << "test_adv_lexer() :" << std::endl;

    constexpr EBNFBakery rules;
    //    constexpr auto nozero = NTerm(cs("digit excluding zero"));
    //    constexpr auto d_nozero = Define(nozero, );
    constexpr auto digit = NTerm(cs<"digit">());
    constexpr auto d_digit = Define(digit, Repeat(Alter(Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()), Term(cs<"4">()), Term(cs<"5">()),
                                                        Term(cs<"6">()), Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()), Term(cs<"0">()))));

    constexpr auto number = NTerm(cs<"number">());
    constexpr auto d_number = Define(number, Repeat(digit));
    constexpr auto add = NTerm(cs<"add">());
    constexpr auto sub = NTerm(cs<"sub">());
    constexpr auto mul = NTerm(cs<"mul">());
    constexpr auto div = NTerm(cs<"div">());
    constexpr auto op = NTerm(cs<"op">()); // any operator
    constexpr auto group = NTerm(cs<"group">());
    constexpr auto array = NTerm(cs<"array">());

    constexpr auto d_group = Define(group, Concat(Term(cs<"(">()), op, Repeat(Concat(Term(cs<",">()), op)), Term(cs<")">())));
    constexpr auto d_array = Define(array, Concat(Term(cs<"[">()), op, Repeat(Concat(Term(cs<",">()), op)), Term(cs<"]">())));
    constexpr auto d_op = Define(op, Alter(number, group, array));

    constexpr auto ruleset = RulesDef(d_digit, d_number, d_op, d_group, d_array);

    using VStr = StdStr<char>;
    using TokenType = StdStr<char>;

    std::cout << ruleset.bake(rules) << std::endl;
    // Parser classes init
    // ===================

    auto lexer = make_lexer<VStr, TokenType>(ruleset, mk_lexer_conf<LexerConfEnum::AdvancedLexer, LexerConfEnum::HandleDuplicates>());
    //LexerLegacy<VStr, TokenType> lexer(ruleset); // Lexer init

    // Parser init
    constexpr auto conf = mk_sr_parser_conf<SRConfEnum::PrettyPrint, SRConfEnum::Lookahead>();
    auto parser = make_sr_parser<VStr, TokenType, TreeNode<VStr>>(ruleset, lexer, conf);

    StdStr<char> in("(12,42,[45,(4,24)])");
    bool ok;
    auto tokens = lexer.run(in, ok);

    if (!ok)
    {
        std::cout << "lexer build error" << std::endl;
        return false;
    }

    TreeNode<VStr> tree;
    std::cout << "======" << std::endl << "SR parser routine : " << std::endl;
    ok = parser.run(tree, op, tokens);

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


bool test_gbnf()
{
    return test_gbnf_basic() && test_gbnf_complex1() && test_gbnf_extended() && test_gbnf_parse_1() && test_gbnf_parse_calc() && test_sr_init() && test_sr_calc() && test_adv_lexer();
}

#endif //SUPERCFG_BNF_H
