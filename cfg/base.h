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
enum class BinOpType
{
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
        if constexpr (Operator == BinOpType::Except) return rules.bake_except(left.bake(rules), right.bake(rules));
    }
};


/**
 * @brief Base ending (termination) operator
 */
class EndOP
{
public:
    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) { return rules.bake_end(); }
};


/**
 * @brief Enum containing ebnf multi-operand operations
 */
enum class MultiOpType
{
    Concat,
    Alter,
    Define,
    Optional,
    Repeat,
    Group,
    Comment,
    SpecialSeq,
    Root
};


/**
 * @brief Base operator with multiple args
 * @tparam TSymbols A pack of symbols
 */
template<MultiOpType Operator, class... TSymbols>
class MultiOp
{
public:
    std::tuple<TSymbols...> terms;

    constexpr explicit MultiOp(const TSymbols&... t) : terms(t...) { validate(); }

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
        // Definition operator rules
        if constexpr (Operator == MultiOpType::Define) return do_bake_define(rules);
        else // Other operators
        {
            if constexpr (sizeof...(TSymbols) == 1) // Singular symbol check
                return exec_bake_rule(rules, std::get<0>(terms));
            else return exec_bake_rule(rules, do_bake<1>(rules, std::get<0>(terms)));
        }
    }

protected:
    template<std::size_t depth, class BNFRules, class TBuf>
    constexpr auto do_bake(const BNFRules& rules, TBuf last) const
    {
        auto res = exec_bake_rule(rules, last, std::get<depth>(terms));
        if constexpr (depth + 1 < sizeof...(TSymbols))
            return do_bake<depth + 1>(rules, res);
        else return res;
    }

    /**
     * @brief Execute rules baking function
     * @param symbols List of symbols which are passed to the baking function
     */
    template<class BNFRules, class... TSymbolPack>
    constexpr auto exec_bake_rule(const BNFRules& rules, const TSymbolPack&... symbols) const
    {
        if constexpr (Operator == MultiOpType::Concat) return rules.bake_concat(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Alter) return rules.bake_alter(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Define) return rules.bake_define(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Optional) return rules.bake_optional(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Repeat) return rules.bake_repeat(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Group) return rules.bake_group(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Comment) return rules.bake_comment(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::SpecialSeq) return rules.bake_special_seq(symbols.bake(rules)...);
        if constexpr (Operator == MultiOpType::Root) return rules.bake_root_elem(symbols.bake(rules)...);
    }

    /**
     * @brief Sanity check for the passed arguments
     */
    constexpr void validate() const
    {
        if constexpr (Operator == MultiOpType::Define)
        {
            static_assert(sizeof...(TSymbols) == 2 || sizeof...(TSymbols) == 3, "Definition should only take 2 or 3 operators");
            static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::get<0>(terms))>, NTerm<decltype(std::get<0>(terms).name)>>, "Definition must have a non-terminal on lhs");
            if constexpr (sizeof...(TSymbols) == 3)
                static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::get<2>(terms))>, EndOP>, "Definition might only have a termination operator at the end");
        }
    }

    /**
     * @brief Bake definition operator
     */
    template<class BNFRules>
    constexpr auto do_bake_define(const BNFRules& rules) const
    {
        EndOP end;
        return exec_bake_rule(rules, std::get<0>(terms), std::get<1>(terms)) + end.bake(rules);
    }
};


template<BinOpType op, class TSymbolA, class TSymbolB>
constexpr auto make_op(const TSymbolA& a, const TSymbolB& b) { return BinaryOP<op, TSymbolA, TSymbolB>(a, b); }

template<MultiOpType op, class... TSymbols>
constexpr auto make_op(const TSymbols&... symbols) { return MultiOp<op, TSymbols...>(symbols...); }

// Operators definition
template<class TSymbolA, class TSymbolB> constexpr auto Except(const TSymbolA& a, const TSymbolB& b) { return BinaryOP<BinOpType::Except, TSymbolA, TSymbolB>(a, b); }

template<class... TSymbols> constexpr auto Concat(const TSymbols&... symbols) { return MultiOp<MultiOpType::Concat, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Alter(const TSymbols&... symbols) { return MultiOp<MultiOpType::Alter, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Define(const TSymbols&... symbols) { return MultiOp<MultiOpType::Define, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Optional(const TSymbols&... symbols) { return MultiOp<MultiOpType::Optional, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Repeat(const TSymbols&... symbols) { return MultiOp<MultiOpType::Repeat, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Group(const TSymbols&... symbols) { return MultiOp<MultiOpType::Group, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Comment(const TSymbols&... symbols) { return MultiOp<MultiOpType::Comment, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto SpecialSeq(const TSymbols&... symbols) { return MultiOp<MultiOpType::SpecialSeq, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Root(const TSymbols&... symbols) { return MultiOp<MultiOpType::Root, TSymbols...>(symbols...); }


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
