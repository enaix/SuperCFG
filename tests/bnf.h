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

    constexpr GBNFRules rules;
    constexpr auto test = BaseNTerm(ConstStr("abcd")).bake(rules);
    std::cout << test.c_str() << std::endl;
    constexpr auto true1 = ConstStr("abcd");
    static_assert(test == true1);

    constexpr auto test2 = BaseTerm(ConstStr("abcd")).bake(rules);
    std::cout << test2.c_str() << std::endl;
    return true;
}


bool test_gbnf()
{
    return test_gbnf_basic();
}

#endif //SUPERCFG_BNF_H
