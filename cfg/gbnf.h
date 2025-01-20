//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_GBNF_H
#define SUPERCFG_GBNF_H

#include "cfg/base.h"


class GBNFRules
{
public:
    template<class CStr>
    constexpr auto bake_nonterminal(const CStr& name) const { return name; }

    template<class CStr>
    constexpr auto bake_terminal(const CStr& name) const { return CStr::make("\"") + name + CStr::make("\""); }
};



#endif //SUPERCFG_GBNF_H
