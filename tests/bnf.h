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
    constexpr auto test = NTerm(ConstStr("abcd")).bake(rules);
    std::cout << test.c_str() << std::endl;
    constexpr auto true1 = ConstStr("abcd");
    static_assert(test == true1);

    constexpr auto test2 = Term(ConstStr("abcd")).bake(rules);
    std::cout << test2.c_str() << std::endl;

//    constexpr auto test3 = BinaryOP<Alter>(Term(ConstStr("abcd")), Term(ConstStr("abcde"))).bake(rules);
    constexpr auto test3 = make_op<Alter>(Term(ConstStr("abcd")), Term(ConstStr("abcde"))).bake(rules);
    std::cout << test3.c_str() << std::endl;

    return true;
}

bool test_gbnf_complex1()
{
    std::cout << "test_gbnf_complex1()" << std::endl;

    constexpr EBNFRules rules;
    constexpr auto term1 = NTerm(ConstStr("A"));
    constexpr auto term2 = NTerm(ConstStr("B"));
    constexpr auto term3 = Term(ConstStr("42"));
    constexpr auto term4 = Term(ConstStr("xyz"));
    constexpr auto term5 = Term(ConstStr("!"));

    constexpr auto res = make_op<Optional>(
        make_op<Concat>(make_op<Group>(make_op<Repeat>(term1, term5), term2), term3), term4
    ).bake(rules);

    std::cout << res.c_str() << std::endl;

    return true;
}


bool test_gbnf()
{
    return test_gbnf_basic() && test_gbnf_complex1();
}

#endif //SUPERCFG_BNF_H
