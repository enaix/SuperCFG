//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_BNF_H
#define SUPERCFG_BNF_H

#include <iostream>

#include "cfg/gbnf.h"
#include "cfg/containers.h"
#include "cfg/base.h"

bool test_gbnf_basic()
{
    std::cout << "test_gbnf_basic()" << std::endl;

    constexpr EBNFRules rules;
    constexpr auto test = NTerm(cs("abcd")).bake(rules);
    std::cout << test.c_str() << std::endl;
    constexpr auto true1 = cs("abcd");
    static_assert(test == true1);

    constexpr auto test2 = Term(cs("abcd")).bake(rules);
    std::cout << test2.c_str() << std::endl;

//    constexpr auto test3 = BinaryOP<Alter>(Term(cs("abcd")), Term(cs("abcde"))).bake(rules);
    constexpr auto test3 = Alter(Term(cs("abcd")), Term(cs("abcde"))).bake(rules);
    std::cout << test3.c_str() << std::endl;

    return true;
}

bool test_gbnf_complex1()
{
    std::cout << "test_gbnf_complex1()" << std::endl;

    constexpr EBNFRules rules;
    constexpr auto term1 = NTerm(cs("A"));
    constexpr auto term2 = NTerm(cs("B"));
    constexpr auto term3 = Term(cs("42"));
    constexpr auto term4 = Term(cs("xyz"));
    constexpr auto term5 = Term(cs("!"));

    constexpr auto res = Optional(
        Concat(Group(Repeat(Except(term1, term5)), term2), term3), term4
    ).bake(rules);

    std::cout << res.c_str() << std::endl;

    constexpr auto nozero = NTerm(cs("digit excluding zero"));
    constexpr auto d_nozero = Define(nozero, Alter(Term(cs("1")), Term(cs("2")), Term(cs("3")), Term(cs("4")), Term(cs("5")),
                                             Term(cs("6")), Term(cs("7")), Term(cs("8")), Term(cs("9"))));
    constexpr auto d_digit = Define(NTerm(cs("digit")), Alter(Term(cs("0")), nozero));
    constexpr auto root = Root(d_nozero, d_digit).bake(rules);

    constexpr char digit_check[] = "digit excluding zero = \"1\" | \"2\" | \"3\" | \"4\" | \"5\" | \"6\" | \"7\" | \"8\" | \"9\" ;\n"
                                   "digit = \"0\" | digit excluding zero ;";
    static_assert(cs(digit_check) == root, "test_gbnf_complex1() : digits : deviation from vanilla EBNF");

    std::cout << root.c_str() << std::endl;

    return true;
}


bool test_gbnf()
{
    return test_gbnf_basic() && test_gbnf_complex1();
}

#endif //SUPERCFG_BNF_H
