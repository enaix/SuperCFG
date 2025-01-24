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
    //static constexpr bool is_operator() { return false; };
    using is_operator = std::false_type;

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
    //static constexpr bool is_operator() { return false; };
    using is_operator = std::false_type;

    constexpr Term() : name("\0") {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return rules.bake_terminal(this->name); }
};


/**
 * @brief Enum containing ebnf multi-operand operations
 */
enum class OpType
{
    Concat,
    Alter,
    Define,
    Optional,
    Repeat,
    Group,
    Comment,
    SpecialSeq,
    Except,
    End,
    Root
};

 /**
  * @brief Operator type wrapper
  */
template<OpType T> struct op_type_t
{
    static constexpr OpType value = T;
    constexpr auto operator()() const { return value; }
};


/**
 * @brief Base operator class
 * @tparam OpType Operator type
 * @tparam TSymbols Symbols contained in the operator
 */
template<OpType Operator, class... TSymbols>
class BaseOp
{
public:
    std::tuple<TSymbols...> terms;

    constexpr explicit BaseOp(const TSymbols&... t) : terms(t...) { validate(); }

    using is_operator = std::true_type;
    using get_operator = op_type_t<Operator>;

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const
    {
        // Definition operator rules
        if constexpr (Operator == OpType::Define) return do_bake_define(rules);
        else if constexpr (Operator == OpType::Except) return do_bake_except(rules);
        else if constexpr (Operator == OpType::End) return exec_bake_rule(rules);
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
        if constexpr (Operator == OpType::Concat) return rules.bake_concat(symbols.bake(rules)...);
        if constexpr (Operator == OpType::Alter) return rules.bake_alter(symbols.bake(rules)...);
        if constexpr (Operator == OpType::Define) return rules.bake_define(symbols.bake(rules)...);
        if constexpr (Operator == OpType::Optional) return rules.bake_optional(symbols.bake(rules)...);
        if constexpr (Operator == OpType::Repeat) return rules.bake_repeat(symbols.bake(rules)...);
        if constexpr (Operator == OpType::Group) return rules.bake_group(symbols.bake(rules)...);
        if constexpr (Operator == OpType::Comment) return rules.bake_comment(symbols.bake(rules)...);
        if constexpr (Operator == OpType::SpecialSeq) return rules.bake_special_seq(symbols.bake(rules)...);
        if constexpr (Operator == OpType::Except) return rules.bake_except(symbols.bake(rules)...);
        if constexpr (Operator == OpType::End) return rules.bake_end();
        if constexpr (Operator == OpType::Root) return rules.bake_root_elem(symbols.bake(rules)...);
    }

    /**
     * @brief Sanity check for the passed arguments
     */
    constexpr void validate() const
    {
        if constexpr (Operator == OpType::Define)
        {
            static_assert(sizeof...(TSymbols) == 2 || sizeof...(TSymbols) == 3, "Definition should only take 2 or 3 operators");
            static_assert(std::is_same_v<std::remove_cvref_t<decltype(std::get<0>(terms))>, NTerm<decltype(std::get<0>(terms).name)>>, "Definition must have a non-terminal on lhs");
            if constexpr (sizeof...(TSymbols) == 3)
                static_assert(std::tuple_element_t<2, decltype(terms)>::get_operator::value == OpType::End, "Definition might only have a termination operator at the end");
        }
        if constexpr (Operator == OpType::Except) static_assert(sizeof...(TSymbols) == 2, "Exception may only contain 2 elements");
        if constexpr (Operator == OpType::End) static_assert(sizeof...(TSymbols) == 0, "End operator must not contain elements");
        else validate_each_param<0>(); // Do not validate params for the end operator
    }

    /**
     * @brief Sanity check for each symbol
     */
    template<std::size_t depth>
    constexpr void validate_each_param() const
    {
        // Check non-root elements which contain operators
        if constexpr (Operator != OpType::Root && std::tuple_element_t<depth, decltype(terms)>::is_operator::value)
        {
            static_assert(std::tuple_element_t<depth, decltype(terms)>::get_operator::value != OpType::Define, "Definitions are only allowed in root elements");
            static_assert(std::tuple_element_t<depth, decltype(terms)>::get_operator::value != OpType::Root, "Root elements are only allowed in top-level elements");
        }
        // Check root elements for Term/NTerm
        //else static_assert(!std::tuple_element_t<depth, decltype(terms)>::is_operator::value, "Root elements may only contain operators");
        if constexpr (depth + 1 < sizeof...(TSymbols)) validate_each_param<depth + 1>();
    }

    /**
     * @brief Bake definition operator
     */
    template<class BNFRules>
    constexpr auto do_bake_define(const BNFRules& rules) const
    {
        BaseOp<OpType::End> end;
        return exec_bake_rule(rules, std::get<0>(terms), std::get<1>(terms)) + end.bake(rules);
    }

    /**
     * @brief Bake exception operator
     */
    template<class BNFRules>
    constexpr auto do_bake_except(const BNFRules& rules) const
    {
        return exec_bake_rule(rules, std::get<0>(terms), std::get<1>(terms));
    }
};


// Operators definition
template<class... TSymbols> constexpr auto Concat(const TSymbols&... symbols) { return BaseOp<OpType::Concat, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Alter(const TSymbols&... symbols) { return BaseOp<OpType::Alter, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Define(const TSymbols&... symbols) { return BaseOp<OpType::Define, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Optional(const TSymbols&... symbols) { return BaseOp<OpType::Optional, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Repeat(const TSymbols&... symbols) { return BaseOp<OpType::Repeat, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Group(const TSymbols&... symbols) { return BaseOp<OpType::Group, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Comment(const TSymbols&... symbols) { return BaseOp<OpType::Comment, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto SpecialSeq(const TSymbols&... symbols) { return BaseOp<OpType::SpecialSeq, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Except(const TSymbols&... symbols) { return BaseOp<OpType::Except, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto End(const TSymbols&... symbols) { return BaseOp<OpType::End, TSymbols...>(symbols...); }
template<class... TSymbols> constexpr auto Root(const TSymbols&... symbols) { return BaseOp<OpType::Root, TSymbols...>(symbols...); }


#endif //SUPERCFG_BASE_H
