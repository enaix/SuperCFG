//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_BASE_H
#define SUPERCFG_BASE_H

#include <cstddef>
#include <tuple>
#include "cfg/containers.h"


 /**
  * @brief Base nonterminal class
  * @tparam CStr Const string class
  */
template<class CStr>
class NTerm
{
public:
    CStr name;
    constexpr explicit NTerm(const CStr& n) : name(n) {}

    constexpr NTerm() : name("\0") {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return rules.bake_nonterminal(this->name); }
};

 /**
  * @brief Base terminal class (literal)
  * @tparam CStr Const string class
  */
template<class CStr>
class Term
{
public:
    CStr name;
    constexpr explicit Term(const CStr& n) : name(n) {}

    constexpr Term() : name("\0") {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return rules.bake_terminal(this->name); }
};

 /**
  * @brief Enum containing ebnf binary operations
  */
enum BinOpType
{
    Concat,
    Alter,
    Except
};

 /**
  * @brief Base operator with 2 arguments
  * @tparam TSymbolA Left symbol
  * @tparam TSymbolB Right symbol
  */
template<BinOpType Operator, class TSymbolA, class TSymbolB>
class BinaryOP
{
public:
    TSymbolA left;
    TSymbolB right;
    constexpr BinaryOP(const TSymbolA& l, const TSymbolB& r) : left(l), right(r) {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const
    {
        if constexpr (Operator == BinOpType::Concat) return rules.bake_concat(left.bake(rules), right.bake(rules));
        if constexpr (Operator == BinOpType::Alter) return rules.bake_alter(left.bake(rules), right.bake(rules));
        if constexpr (Operator == BinOpType::Except) return rules.bake_except(left.bake(rules), right.bake(rules));
        /*switch (Operator)
        {
            case BinOpType::Concat:
                return rules.bake_concat(left.bake(rules), right.bake(rules));
            case BinOpType::Alter:
                return rules.bake_alter(left.bake(rules), right.bake(rules));
            case BinOpType::Except:
                return rules.bake_except(left.bake(rules), right.bake(rules));
        }*/
    }
};


/*template<class TSymbolA, class TSymbolB>   using Concat = BinaryOP<BinOpType::Concat, TSymbolA, TSymbolB>;
template<class TSymbolA, class TSymbolB>   using Alter  = BinaryOP<BinOpType::Alter, TSymbolA, TSymbolB>;
template<class TSymbolA, class TSymbolB>   using Except = BinaryOP<BinOpType::Except, TSymbolA, TSymbolB>;*/

/**
 * @brief Enum containing ebnf multi-operand operations
 */
enum MultiOpType
{
    Optional,
    Repeat,
    Group,
    SpecialSeq
};

template<class Operator>
struct TypeWrapper { Operator type; };

/**
 * @brief Base operator with multiple args
 * @tparam TSymbols A pack of symbols
 */
template<MultiOpType Operator, class... TSymbols>
class MultiOp
{
public:
    std::tuple<TSymbols...> terms;

    constexpr explicit MultiOp(const TSymbols&... t) : terms(t...) {}

    constexpr void each_elem(auto& func)
    {
        for (size_t i = 0; i < sizeof...(TSymbols); i++)
        {
            func(std::get<i>(terms));
        }
    }

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const
    {
        return exec_bake_rule(rules, do_bake<1>(rules, std::get<0>(terms)));
    }

protected:
    template<std::size_t depth, class BNFRules, class TBuf>
    constexpr auto do_bake(const BNFRules& rules, TBuf last) const
    {
        auto res = exec_bake_rule(rules, last, std::get<depth>(terms));
        if constexpr (depth + 1 < sizeof...(TSymbols))
            return do_bake<depth + 1>(rules, res);
        return res;
    }

    template<class BNFRules, class... TSymbolPack>
    constexpr auto exec_bake_rule(const BNFRules& rules, const TSymbolPack&... symbols) const
    {
        if constexpr (Operator == MultiOpType::Optional) return rules.bake_optional(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Repeat) return rules.bake_repeat(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Group) return rules.bake_group(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::SpecialSeq) return rules.bake_special_seq(symbols.bake(rules)...);
        /*switch (Operator)
        {
            case MultiOpType::Optional:
                return rules.bake_optional(rules.bake(symbols)...);
            case MultiOpType::Repeat:
                return rules.bake_repeat(rules.bake(symbols)...);
            case MultiOpType::Group:
                return rules.bake_group(rules.bake(symbols)...);
            case MultiOpType::SpecialSeq:
                return rules.bake_special_seq(rules.bake(symbols)...);
        }*/
    }
};


template<BinOpType op, class TSymbolA, class TSymbolB>
constexpr auto make_op(const TSymbolA& a, const TSymbolB& b) { return BinaryOP<op, TSymbolA, TSymbolB>(a, b); }

template<MultiOpType op, class... TSymbols>
constexpr auto make_op(const TSymbols&... symbols) { return MultiOp<op, TSymbols...>(symbols...); }


/*template<class... TSymbols>   using Optional   = MultiOp<MultiOpType::Optional, TSymbols...>;
template<class... TSymbols>   using Repeat     = MultiOp<MultiOpType::Repeat, TSymbols...>;
template<class... TSymbols>   using Group      = MultiOp<MultiOpType::Group, TSymbols...>;
template<class... TSymbols>   using SpecialSeq = MultiOp<MultiOpType::SpecialSeq, TSymbols...>;*/

 /**
  * @brief Base ending (termination) operator
  */
/*class BaseEndOP
{
public:
    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) { return rules.bake_end(); }
};*/

/**
 * @brief Base operator with multiple args, fixed-size implementation
 * @tparam TTerm Base nonterminal class
 * @tparam N Deduced number of arguments
 */
template<class TTerm>
class BaseMultiOPBuf
{
public:
    static constexpr std::size_t NTermsMax = 16;
    TTerm terms[NTermsMax];
    std::size_t N;

    template<class... NTerms>
    constexpr explicit BaseMultiOPBuf(const NTerms&... t) : N(sizeof...(t))
    {
        init(0, t...);
    }

    //constexpr explicit MultiOp(NTerm t) {}

protected:
    template<class... NTerms>
    constexpr void init(const std::size_t i, const TTerm& term, const NTerms&... t)
    {
        terms[i] = term;
        init(i+1, t...);
    }

    constexpr void init(const std::size_t i, const TTerm& term)
    {
        terms[i] = term;
        for (std::size_t j = i+1; j < NTermsMax; j++) { terms[j] = TTerm(); }
    }
};

#endif //SUPERCFG_BASE_H
