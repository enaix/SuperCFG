//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_BASE_H
#define SUPERCFG_BASE_H

#include <cstddef>
#include <tuple>
#include "cfg/containers.h"


template<class CStr>
class TreeNode
{
public:
    CStr name;
    // Add custom data
    TreeNode<CStr>* parent;
    std::vector<TreeNode<CStr>> nodes;

    TreeNode() : name("\0") {}

    TreeNode(const CStr& name, TreeNode<CStr>* parent = nullptr) : name(name), parent(parent) {}

    TreeNode<CStr>& add(const TreeNode<CStr>& node) { nodes.push_back(std::move(node)); return nodes.back; }
};



 /**
  * @brief Base nonterminal class
  * @tparam CStr Const string class
  */
template<class CStr>
class NTerm
{
public:
    CStr name;
    // TODO add type
    constexpr explicit NTerm(const CStr& n) : name(n) {}
    //static constexpr bool is_operator() { return false; };
    using _is_operator = std::false_type;

    constexpr NTerm() : name("\0") {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return rules.bake_nonterminal(this->name); }

    template<class Arg, class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return bake(rules); }

    constexpr auto flatten() const { return *this; } // flatten() operation on a term always returns term

    TreeNode<CStr>& create_node(TreeNode<CStr>& parent) const { return parent.add(TreeNode(name)); }

    TreeNode<CStr> create_node() const { return TreeNode(name); }
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
    using _is_operator = std::false_type;

    constexpr Term() : name("\0") {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return rules.bake_terminal(this->name); }

    template<class Arg, class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return bake(rules); }

    constexpr auto flatten() const { return *this; } // flatten() operation on a term always returns term
};


 /**
  * @brief Enum containing ebnf multi-operand operations
  */
enum class OpType
{
    // Basic operators (EBNF)
    Concat,
    Alter,
    Define,
    Optional, // 0 or 1 times
    Repeat, // 0 or inf times
    Group,
    Comment,
    SpecialSeq,
    Except,
    End,
    RulesDef, // Base element

    // Extended operators
    RepeatExact, // Repeat exactly M times
    RepeatGE, // Repeat greater or equal than times
    RepeatRange, // Repeat from M to N times

    None // Last element for internal purposes
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
  * @brief Helper function that returns whether an object is an operator
  */
template<class TSymbol>
constexpr inline bool is_operator(const TSymbol& s) { return TSymbol::_is_operator::value; }

 /**
  * @brief Helper function that returns an operator enum
  */
template<class TSymbol>
constexpr inline OpType get_operator(const TSymbol& s) { return TSymbol::_get_operator::value; }

 /**
  * @brief Helper function that returns whether an object is an terminal
  */
template<class TSymbol>
constexpr inline bool is_term(const TSymbol& s)
{
    if constexpr (is_operator(s)) return false;
    else return std::is_same_v<std::remove_cvref_t<decltype(s)>, Term<decltype(s.name)>>;
}

 /**
  * @brief Helper function that returns whether an object is an nonterminal
  */
template<class TSymbol>
constexpr inline bool is_nterm(const TSymbol& s)
{
    if constexpr (is_operator(s)) return false;
    else return std::is_same_v<std::remove_cvref_t<decltype(s)>, NTerm<decltype(s.name)>>;
}

// Repeat* helper operators

template<class TSymbol>
constexpr inline std::size_t get_repeat_times(const TSymbol& s) { return TSymbol::_times::value; }

template<class TSymbol>
constexpr inline std::size_t get_range_from(const TSymbol& s) { return TSymbol::_from::value; }

template<class TSymbol>
constexpr inline std::size_t get_range_to(const TSymbol& s) { return TSymbol::_to::value; }

// Tuple helper operators

template<class TSymbol>
struct get_first { typedef std::tuple_element_t<0, typename TSymbol::term_types_tuple> type; };

template<class TSymbol>
struct get_second { typedef std::tuple_element_t<1, typename TSymbol::term_types_tuple> type; };

template<class TSymbol> using get_first_t = get_first<TSymbol>::type;
template<class TSymbol> using get_second_t = get_second<TSymbol>::type;


 /**
  * @brief Base operator class
  * @tparam Operator Operator enum type
  * @tparam TSymbols Symbols contained in the operator
  */
template<OpType Operator, class... TSymbols>
class BaseOp
{
public:
    std::tuple<TSymbols...> terms;

    constexpr explicit BaseOp(const TSymbols&... t) : terms(t...) { validate(); }

    constexpr explicit BaseOp(auto validator, const TSymbols&... t) : terms(t...) { validator(); }

    using _is_operator = std::true_type;
    using _get_operator = op_type_t<Operator>;

    using term_types_tuple = std::tuple<std::remove_cvref_t<TSymbols>...>;
    using term_ptr_tuple = std::tuple<const std::remove_cvref_t<TSymbols>* const...>;

    [[nodiscard]] static constexpr std::size_t size() { return sizeof...(TSymbols); }

    template<class integral_const>
    [[nodiscard]] constexpr const auto operator[] (const integral_const N) const { return std::get<integral_const::value>(terms); }

    /**
     * @brief Iterate over each element in terms tuple
     * @param func Lambda closure that processes each element
     */
    constexpr void each(auto func) const { each_symbol<0>(func); }

    /**
     * @brief Recursive baking function that returns CStr. Manages precedence and calls baking preprocessing
     * @tparam max_precedence_t Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFRules Base rules class
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto bake(const BNFRules& rules) const
    {
        return process_precedence<max_precedence_t>(rules, [&]<class next>(const auto&... args) { return preprocess_bake<next, BNFRules>(args...); } );
    }

    /**
     * @brief Top-level recursive baking function that returns CStr
     * @tparam BNFRules Base rules class
     */
    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const
    {
        return preprocess_bake<op_type_t<Operator>>(rules);
    }

    /**
     * @brief Flatten operators A(A(...A(B))) to A(B, A(B, A(B,...))). Stops if A(B,C,...) is encountered or an operator is different
     */
    constexpr auto flatten() const
    {
        static_assert(Operator == OpType::Concat || Operator == OpType::Alter, "Operation does not support flatten()");
        static_assert(sizeof...(TSymbols) == 1, "Cannot flatten operator of more than 1 symbol");
        return do_flatten(*this);
    }

    /**
     * @brief A helper method for constructing a tuple of const pointers
     */
    constexpr term_ptr_tuple get_ptr_tuple() const { return construct_ptr_tuple<0>(); }

    /**
     * @brief Preprocess arguments for baking functions. Should be called only in process_precedence
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFRules Base rules class
     */
    template<class max_precedence, class BNFRules>
    constexpr auto preprocess_bake(const BNFRules& rules) const
    {
        // Definition operator rules
        if constexpr (Operator == OpType::Define) return do_bake_define<max_precedence>(rules);
        else if constexpr (Operator == OpType::Except) return do_bake_except<max_precedence>(rules);
        else if constexpr (Operator == OpType::End) return exec_bake_rule<max_precedence>(rules);
        else // Other operators
        {
            if constexpr (sizeof...(TSymbols) == 1) // Singular symbol check
                return exec_bake_rule<max_precedence>(rules, std::get<0>(terms));
            else return exec_bake_rule<max_precedence>(rules, do_bake<1, max_precedence>(rules, std::get<0>(terms)));
        }
    }

protected:
    /**
     * @brief Recursibely bake each argument in terms tuple
     */
    template<std::size_t depth, class max_precedence_t, class BNFRules, class TBuf>
    constexpr auto do_bake(const BNFRules& rules, TBuf last) const
    {
        auto res = exec_bake_rule<max_precedence_t>(rules, last, std::get<depth>(terms));
        if constexpr (depth + 1 < sizeof...(TSymbols))
            return do_bake<depth + 1, max_precedence_t>(rules, res);
        else return res;
    }

    /**
     * @brief Execute rules baking function
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @param symbols List of symbols which are passed to the baking function
     */
    template<class max_precedence_t, class BNFRules, class... TSymbolPack>
    constexpr auto exec_bake_rule(const BNFRules& rules, const TSymbolPack&... symbols) const
    {
        if constexpr (Operator == OpType::Concat) return rules.bake_concat(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::Alter) return rules.bake_alter(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::Define) return rules.bake_define(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::Optional) return rules.bake_optional(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::Repeat) return rules.bake_repeat(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::Group) return rules.bake_group(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::Comment) return rules.bake_comment(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::SpecialSeq) return rules.bake_special_seq(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::Except) return rules.bake_except(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::End) return rules.bake_end();
        if constexpr (Operator == OpType::RulesDef) return rules.bake_rules_def(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::RepeatExact) return rules.bake_repeat_exact(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::RepeatGE) return rules.bake_repeat_ge(symbols.template bake<max_precedence_t>(rules)...);
        if constexpr (Operator == OpType::RepeatRange) return rules.bake_repeat_range(symbols.template bake<max_precedence_t>(rules)...);
    }

    /**
     * @brief Sanity check for the passed arguments for the base class
     */
    constexpr void validate() const
    {
        static_assert(int(Operator) <= int(OpType::RulesDef), "Invalid operator type for class BaseOP");
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
        // TODO add stricter check on the number of symbols
    }

    /**
     * @brief Sanity check for each symbol
     */
    template<std::size_t depth>
    constexpr void validate_each_param() const
    {
        // Check non top level elements which contain operators
        // This long expression is needed in order to use :: on a tuple element type in constexpr
        if constexpr (Operator != OpType::RulesDef && std::remove_cvref_t<std::tuple_element_t<depth, decltype(terms)>>::_is_operator::value)// is_operator(std::get<depth>(terms)))
        {
            static_assert(std::remove_cvref_t<std::tuple_element_t<depth, decltype(terms)>>::_get_operator::value != OpType::Define, "Definitions are only allowed in top level elements");
            static_assert(std::remove_cvref_t<std::tuple_element_t<depth, decltype(terms)>>::_get_operator::value != OpType::RulesDef, "RulesDef elements are only allowed in top-level elements");
        }
        // Check top level elements for Term/NTerm
        //else static_assert(!std::tuple_element_t<depth, decltype(terms)>::is_operator::value, "RulesDef elements may only contain operators");
        if constexpr (depth + 1 < sizeof...(TSymbols)) validate_each_param<depth + 1>();
    }

    /**
     * @brief Bake definition operator
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto do_bake_define(const BNFRules& rules) const
    {
        BaseOp<OpType::End> end;
        return exec_bake_rule<max_precedence_t>(rules, std::get<0>(terms), std::get<1>(terms)) + end.bake<max_precedence_t>(rules);
    }

    /**
     * @brief Bake exception operator
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto do_bake_except(const BNFRules& rules) const
    {
        return exec_bake_rule<max_precedence_t>(rules, std::get<0>(terms), std::get<1>(terms));
    }

    template<class TSymbol>
    constexpr auto do_flatten(const TSymbol& symbol) const
    {
        // Check if the value is not an operator, the operator is different or it has >1 element
        if constexpr (!std::remove_cvref_t<TSymbol>::_is_operator::value) return symbol;
        else if constexpr (std::remove_cvref_t<TSymbol>::_get_operator::value != Operator
                                            || std::remove_cvref_t<TSymbol>::size() != 1) return symbol;
        else
        {
            auto res = do_flatten(std::get<0>(symbol.terms));
            return BaseOp<Operator, std::remove_cvref_t<TSymbol>, decltype(res)>(symbol, res);
        }
    }

    template<std::size_t i, class TSymbol>
    constexpr void each_symbol(auto func) const
    {
        func(std::get<i>(terms));
        if constexpr (i + 1 < sizeof...(TSymbols)) each_symbol<i + 1>(func);
    }

    template<std::size_t i>
    constexpr auto construct_ptr_tuple() const
    {
        const auto elem = std::get<i>(terms);
        auto elem_ptr = static_cast<const std::remove_cvref_t<decltype(elem)>* const>(&elem); // Convert elem ref to ptr
        if constexpr (i + 1 < sizeof...(TSymbols)) return std::tuple_cat(std::make_tuple(elem_ptr), construct_ptr_tuple<i+1>());
        else return std::make_tuple(elem_ptr);
    }

    /**
     * @brief Process precedence order and call the corresponding bake operation
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFRules Base rules class
     * @param bake_func Lambda closure template which calls preprocess_bake function
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto process_precedence(const BNFRules& rules, auto bake_func) const
    {
        // If our precedence is lower than previous, we add grouping operator and recursively parse
        if constexpr (BNFRules::precedence().has(Operator) && BNFRules::precedence().has(max_precedence_t::value) && BNFRules::precedence().less(Operator, max_precedence_t::value)) return BaseOp<OpType::Group, decltype(*this)>(*this).template bake<op_type_t<OpType::None>>(rules);
        // bake_func is a lambda, so we need to explicitly call operator()
        else return bake_func.template operator()<op_type_t<BNFRules::precedence().max(max_precedence_t::value, Operator)>>(rules);
    }
};


// Basic operators definition
// ==========================

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
template<class... TSymbols> constexpr auto RulesDef(const TSymbols&... symbols) { return BaseOp<OpType::RulesDef, TSymbols...>(symbols...); }


 /**
  * @brief Base extended operator class for RepeatExact and RepeatGT
  * @tparam Operator Operator enum type
  * @tparam Times How many times to repeat (M)
  * @tparam TSymbols Symbols contained in the operator
  */
template<OpType Operator, std::size_t Times, class... TSymbols>
class BaseExtRepeat : public BaseOp<Operator, TSymbols...>
{
public:
    using typename BaseOp<Operator, TSymbols...>::_is_operator;
    using typename BaseOp<Operator, TSymbols...>::_get_operator;
    using _times = IntegralWrapper<Times>;

    constexpr explicit BaseExtRepeat(const TSymbols&... t) : BaseOp<Operator, TSymbols...>([&](){ this->validate(); }, t...) { }

    constexpr explicit BaseExtRepeat(auto validator, const TSymbols&... t) : BaseOp<Operator, TSymbols...>(validator, t...) { }

    /**
     * @brief Recursive baking function that returns CStr. Manages precedence and calls baking preprocessing
     * @tparam max_precedence_t Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFRules Base rules class
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto bake(const BNFRules& rules) const
    {
        this->template process_precedence<max_precedence_t>(rules, [&]<class next>(const auto&... args) { return preprocess_bake<next, BNFRules>(args...); });
    }

    /**
     * @brief Top-level recursive baking function that returns CStr
     * @tparam BNFRules Base rules class
     */
    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return preprocess_bake<op_type_t<Operator>, BNFRules>(rules); }

    /**
     * @brief Preprocess arguments for baking functions. Should be called only in process_precedence
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFRules Base rules class
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto preprocess_bake(const BNFRules& rules) const
    {
        auto symbol = to_bnf_flavor(rules);
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(symbol)>, std::remove_cvref_t<decltype(*this)>>)
            return this->template exec_bake_rule<max_precedence_t>(rules, IntegralWrapper<Times>(), std::get<0>(this->terms));
        else return symbol.template bake<max_precedence_t>(rules);
    }

protected:
    /**
     * @brief Cast extended repeat operator to compatible bnf operators
     */
    template<class BNFRules>
    constexpr auto to_bnf_flavor(const BNFRules& rules) const
    {
        if constexpr (Operator == OpType::RepeatExact)
        {
            if constexpr (BNFRules::feature_repeat_exact()) return *this;
            else return unwrap_repeat_exact<Times>(rules, std::get<0>(this->terms)).flatten();
        }
        if constexpr (Operator == OpType::RepeatGE)
        {
            if constexpr (BNFRules::feature_repeat_ge()) return *this;
            else return Concat(unwrap_repeat_exact<Times>(rules, std::get<0>(this->terms)).flatten(), Repeat(std::get<0>(this->terms)));
        }
    }

    /**
     * @brief Recursively unwrap complex operation into N Concat operations
     * @tparam repeat How many times to repeat
     */
    template<std::size_t repeat, class BNFRules, class TSymbol>
    constexpr auto unwrap_repeat_exact(const BNFRules& rules, const TSymbol& symbol) const
    {
        if constexpr (repeat == 1) return symbol;
        else return Concat(unwrap_repeat_exact<repeat - 1>(rules, symbol));
    }

    /**
     * @brief unwrap_repeat_exact wrapper
     * @tparam integral_const How many times to repeat (IntegralConst<...>)
     */
    template<class integral_const, class BNFRules, class TSymbol>
    constexpr auto unwrap_repeat_exact(const integral_const& repeat, const BNFRules& rules, const TSymbol& symbol) const { return unwrap_repeat_exact<integral_const::value>(rules, symbol); }

    /**
     * @brief Recursively unwrap complex operation into N Optional(Concat(...)) operations
     * @tparam repeat How many times to repeat
     */
    template<std::size_t repeat, class BNFRules, class TSymbol>
    constexpr auto unwrap_repeat_le(const BNFRules& rules, const TSymbol& symbol) const
    {
        if constexpr (repeat == 1) return Optional(symbol);
        else return Optional(Concat(symbol, unwrap_repeat_le<repeat - 1>(rules, symbol)));
    }

    /**
     * @brief unwrap_repeat_le wrapper
     * @tparam integral_const How many times to repeat (IntegralConst<...>)
     */
    template<class integral_const, class BNFRules, class TSymbol>
    constexpr auto unwrap_repeat_le(const integral_const& repeat, const BNFRules& rules, const TSymbol& symbol) const { return unwrap_repeat_le<integral_const::value>(rules, symbol); }

    constexpr void validate() const
    {
        static_assert(int(Operator) >= int(OpType::RepeatExact) && int(Operator) <= int(OpType::RepeatGE), "Invalid operator type for class BaseExtRepeat");
        static_assert(sizeof...(TSymbols) == 1, "Extended repeat may only take singular symbol");
        static_assert(Times >= 1, "Invalid number of repeat times");
    }
};


 /**
  * @brief Base extended operator class for RepeatRange
  * @tparam Operator Operator enum type
  * @tparam From Range start (incl)
  * @tparam To Range end (incl)
  * @tparam TSymbols Symbols contained in the operator
  */
template<OpType Operator, std::size_t From, std::size_t To, class... TSymbols>
class BaseExtRepeatRange : public BaseExtRepeat<Operator, From, TSymbols...>
{
public:
    using typename BaseOp<Operator, TSymbols...>::_is_operator;
    using typename BaseOp<Operator, TSymbols...>::_get_operator;
    using _from = IntegralWrapper<From>;
    using _to = IntegralWrapper<To>;

protected:
    using BaseExtRepeat<Operator, From, TSymbols...>::unwrap_repeat_exact;
    using BaseExtRepeat<Operator, From, TSymbols...>::unwrap_repeat_le;

public:
    constexpr explicit BaseExtRepeatRange(const TSymbols&... t) : BaseExtRepeat<Operator, From, TSymbols...>([&](){ validate(); }, t...) { }

    /**
     * @brief Recursive baking function that returns CStr. Manages precedence and calls baking preprocessing
     * @tparam max_precedence_t Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFRules Base rules class
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto bake(const BNFRules& rules) const
    {
        this->template process_precedence<max_precedence_t>(rules, [&]<class next>(const auto&... args) { return preprocess_bake<next, BNFRules>(rules); });
    }

    /**
     * @brief Top-level recursive baking function that returns CStr
     * @tparam BNFRules Base rules class
     */
    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return preprocess_bake<op_type_t<Operator>, BNFRules>(rules); }

    /**
     * @brief Preprocess arguments for baking functions. Should be called only in process_precedence
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFRules Base rules class
     */
    template<class max_precedence_t, class BNFRules>
    constexpr auto preprocess_bake(const BNFRules& rules) const
    {
        auto symbol = to_bnf_flavor(rules);
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(symbol)>, std::remove_cvref_t<decltype(*this)>>)
            return this->template exec_bake_rule<max_precedence_t>(rules, IntegralWrapper<From>(), IntegralWrapper<To>(), std::get<0>(this->terms));
        else return symbol.template bake<max_precedence_t>(rules);
    }

protected:
    template<class BNFRules>
    constexpr auto to_bnf_flavor(const BNFRules& rules) const
    {
        if constexpr (Operator == OpType::RepeatRange)
        {
            if constexpr (BNFRules::feature_repeat_range()) return *this;
            else if constexpr (From == 0) return unwrap_repeat_le(IntegralWrapper<To>(), rules, std::get<0>(this->terms));
            else return Concat(unwrap_repeat_exact(IntegralWrapper<From>(), rules, std::get<0>(this->terms)).flatten(),
                               unwrap_repeat_le(IntegralWrapper<To - From>(), rules, std::get<0>(this->terms)));
        }
    }

    constexpr void validate() const
    {
        static_assert(Operator == OpType::RepeatRange, "Invalid operator type for class BaseExtRepeatRange");
        static_assert(sizeof...(TSymbols) == 1, "Extended repeat may only take singular symbol");
        static_assert(From >= 0 && From < To, "Invalid repeat range");
    }
};


// Extended operators definition
// =============================

template<std::size_t M, class... TSymbols> constexpr auto RepeatExact(const TSymbols&... symbols) { return BaseExtRepeat<OpType::RepeatExact, M, TSymbols...>(symbols...); }
template<std::size_t M, class... TSymbols> constexpr auto RepeatGE(const TSymbols&... symbols) { return BaseExtRepeat<OpType::RepeatGE, M, TSymbols...>(symbols...); }
template<std::size_t M, std::size_t N, class... TSymbols> constexpr auto RepeatRange(const TSymbols&... symbols) { return BaseExtRepeatRange<OpType::RepeatRange, M, N, TSymbols...>(symbols...); }


#endif //SUPERCFG_BASE_H
