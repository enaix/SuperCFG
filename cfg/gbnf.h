//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_GBNF_H
#define SUPERCFG_GBNF_H

#include "cfg/base.h"


class EBNFRules
{
public:
    template<class CStr>
    constexpr auto bake_nonterminal(const CStr& term) const { return term; }

    template<class CStr>
    constexpr auto bake_terminal(const CStr& term) const { return CStr::make("\"") + term + CStr::make("\""); }

    template<class CStrA, class CStrB>
    constexpr auto bake_alter(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make("|") + rhs; }

    template<class CStrA, class CStrB>
    constexpr auto bake_concat(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(", ") + rhs; }

    template<class CStrA, class CStrB>
    constexpr auto bake_except(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" - ") + rhs; }


    template<class CStrA, class CStrB>
    constexpr auto bake_repeat(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_repeat(const CStr& str) const { return CStr::make("{ ") + str + CStr::make(" }") ; }


    template<class CStrA, class CStrB>
    constexpr auto bake_optional(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_optional(const CStr& str) const { return CStr::make("[ ") + str + CStr::make(" ]"); }


    template<class CStrA, class CStrB>
    constexpr auto bake_group(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_group(const CStr& str) const { return CStr::make("( ") + str + CStr::make(" )") ; }


    template<class CStrA, class CStrB>
    constexpr auto bake_special_seq(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_special_seq(const CStr& str) const { return CStr::make("? ") + str + CStr::make(" ?") ; }
};



#endif //SUPERCFG_GBNF_H
