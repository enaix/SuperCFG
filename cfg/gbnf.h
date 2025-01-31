//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_GBNF_H
#define SUPERCFG_GBNF_H

#include "cfg/base.h"


class EBNFRules
{
public:
    // Rules options
    // =============
    [[nodiscard]] constexpr static bool feature_repeat_exact() { return false; }
    [[nodiscard]] constexpr static bool feature_repeat_ge() { return false; }
    [[nodiscard]] constexpr static bool feature_repeat_range() { return false; }

    // Operator precedence
    static constexpr auto precedence() { return EnumMap<OpType, OpType::None>(false, OpType::Repeat, OpType::Except, OpType::Concat, OpType::Alter, OpType::End); }

    // Baking functions
    // ================

    template<class CStr>
    constexpr auto bake_nonterminal(const CStr& term) const { return term; }

    template<class CStr>
    constexpr auto bake_terminal(const CStr& term) const { return CStr::make("\"") + term + CStr::make("\""); }

    template<class CStrA, class CStrB>
    constexpr auto bake_alter(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" | ") + rhs; }

    template<class CStr>
    constexpr auto bake_alter(const CStr& str) const { return str; }


    template<class CStrA, class CStrB>
    constexpr auto bake_concat(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(", ") + rhs; }

    template<class CStr>
    constexpr auto bake_concat(const CStr& str) const { return str; }

    template<class CStrA, class CStrB>
    constexpr auto bake_except(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" - ") + rhs; }


    template<class CStrA, class CStrB>
    constexpr auto bake_repeat(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_repeat(const CStr& str) const { return CStr::make("{ ") + str + CStr::make(" }") ; }


    template<class CStrA, class CStrB>
    constexpr auto bake_define(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" = ") + rhs; }


    template<class CStrA, class CStrB>
    constexpr auto bake_optional(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_optional(const CStr& str) const { return CStr::make("[ ") + str + CStr::make(" ]"); }


    template<class CStrA, class CStrB>
    constexpr auto bake_group(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_group(const CStr& str) const { return CStr::make("(") + str + CStr::make(")") ; }

    template<class CStrA, class CStrB>
    constexpr auto bake_comment(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_comment(const CStr& str) const { return CStr::make("(* ") + str + CStr::make(" *)") ; }

    template<class CStrA, class CStrB>
    constexpr auto bake_special_seq(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make(" ") + rhs; }

    template<class CStr>
    constexpr auto bake_special_seq(const CStr& str) const { return CStr::make("? ") + str + CStr::make(" ?") ; }


    [[nodiscard]] constexpr auto bake_end() const { return ConstStr(" ;"); }


    template<class CStrA, class CStrB>
    constexpr auto bake_rules_def(const CStrA& lhs, const CStrB& rhs) const { return lhs + CStrA::make("\n") + rhs; }

    template<class CStr>
    constexpr auto bake_rules_def(const CStr& str) const { return str; }
};


class ExtEBNFRules : public EBNFRules
{
public:
    // Rules options
    // =============
    [[nodiscard]] constexpr static bool feature_repeat_exact() { return true; }
    [[nodiscard]] constexpr static bool feature_repeat_ge() { return true; }
    [[nodiscard]] constexpr static bool feature_repeat_range() { return true; }

    template<class CStrInt, class CStr>
    constexpr auto bake_repeat_exact(const CStrInt& m, const CStr& str) const { return str + CStr::make("{") + m + CStr::make("}"); }

    template<class CStrInt, class CStr>
    constexpr auto bake_repeat_ge(const CStrInt& m, const CStr& str) const { return str + CStr::make("{") + m + CStr::make(",}"); }

    template<class CStrIntA, class CStrIntB, class CStr>
    constexpr auto bake_repeat_range(const CStrIntA& m, const CStrIntB& n, const CStr& str) const { return str + CStr::make("{") + m + CStr::make(",") + n + CStr::make("}"); }
};


#endif //SUPERCFG_GBNF_H
