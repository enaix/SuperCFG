//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_BASE_H
#define SUPERCFG_BASE_H

#include <cstddef>
#include <tuple>
#include <vector>
#include <limits>
#include "cfg/containers.h"


template<class VStr>
class TreeNode
{
public:
    VStr name;
    VStr value; // Token value
    // Add custom data
    TreeNode<VStr>* parent;
    std::vector<TreeNode<VStr>> nodes;

    TreeNode() : name(), value(), parent(nullptr), nodes() {}

    TreeNode(const TreeNode<VStr>& other) : name(other.name), value(other.value), parent(other.parent), nodes(other.nodes) {}

    template<class TStr>
    explicit TreeNode(const TStr& name, TreeNode<VStr>* parent = nullptr) : name(VStr(name)), value(), parent(parent) {}

    void add(const TreeNode<VStr>& node) { nodes.push_back(node); }

    void merge(const TreeNode<VStr>& node) { nodes.insert(nodes.end(), node.nodes.begin(), node.nodes.end()); }

    TreeNode<VStr>& last() { return nodes.back(); }

    template<class TStr>
    void add_value(const TStr& c) { value += c; }

    void traverse(auto func) const { do_traverse(func, 0); }

protected:
    void do_traverse(auto func, std::size_t depth) const
    {
        func(*this, depth);
        for (const auto& node : nodes) node.do_traverse(func, depth + 1);
    }
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
    constexpr explicit NTerm(const CStr& n) : name(n) {}
    //static constexpr bool is_operator() { return false; };
    using _is_operator = std::false_type;
    using _name_type = CStr;

    constexpr NTerm() : name() {}

    constexpr auto type() const { return name; }

    template<class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const { return rules.bake_nonterminal(this->name); }

    template<class Arg, class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const { return bake(rules); }

    constexpr auto flatten() const { return *this; } // flatten() operation on a term always returns term

    template<class Node>
    Node create_node(Node& parent) const { return Node(name, &parent); }

//    TreeNode<CStr> create_node() const { return TreeNode(name); }

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
    using _is_operator = std::false_type;
    using _name_type = CStr;

    constexpr explicit Term(const CStr& n) : name(n) {}
    //static constexpr bool is_operator() { return false; };

    constexpr Term() : name() {}

    constexpr auto type() const { return name; } // Terminal type may coincide with a nonterminal type

    template<class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const { return rules.bake_terminal(this->name); }

    template<class Arg, class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const { return bake(rules); }

    constexpr auto flatten() const { return *this; } // flatten() operation on a term always returns term
};


template<class CStrStart, class CStrEnd>
class TermsRange
{
public:
    CStrStart start;
    CStrEnd end;
    using _is_operator = std::false_type;

    using _name_type = CStrStart;
    using _name_type_start = CStrStart;
    using _name_type_end = CStrEnd;

    constexpr TermsRange(const CStrStart& start, const CStrEnd& end) : start(start), end(end)
    {
        static_assert(CStrStart::size() == 2 && CStrEnd::size() == 2, "Terms range should contain strings of length 1");
    }

    constexpr TermsRange() = default;

    // TODO implement recursive bake operations

    // Print semantic type of this elem. This cannot be treated as a literal type
    constexpr auto semantic_type() const { return cs<CStrStart, "[">() + start + cs<CStrStart, "-">() + end + cs<CStrStart, "]">(); }

    static constexpr auto get_start() { return CStrStart::template at<0>(); }

    static constexpr auto get_end() { return CStrEnd::template at<0>(); }

    constexpr void each_range(auto func) const
    {
        return lexical_range<typename CStrStart::char_t, CStrStart::template at<0>(), CStrEnd::template at<0>()>(func);
    }

    constexpr auto flatten() const { return *this; }
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
constexpr inline bool is_operator(const TSymbol& s) { return std::remove_cvref_t<TSymbol>::_is_operator::value; }

template<class TSymbol>
constexpr inline bool is_operator() { return std::decay_t<TSymbol>::_is_operator::value; }

 /**
  * @brief Helper function that returns an operator enum
  */
template<class TSymbol>
constexpr inline OpType get_operator(const TSymbol& s) { return std::remove_cvref_t<TSymbol>::_get_operator::value; }

template<class TSymbol>
constexpr inline OpType get_operator() { return std::remove_cvref_t<TSymbol>::_get_operator::value; }

 /**
  * @brief Helper function that returns whether an object is an terminal
  */
template<class TSymbol>
constexpr inline bool is_term()
{
    if constexpr (is_operator<TSymbol>()) return false;
    else return std::is_same_v<std::decay_t<TSymbol>, Term<typename std::decay_t<TSymbol>::_name_type>>;
}

template<class TSymbol>
constexpr inline bool is_term(const TSymbol& s) { return is_term<TSymbol>(); }


/**
 * @brief Helper function that returns whether an object is an nonterminal
 */
template<class TSymbol>
constexpr inline bool is_nterm()
{
    if constexpr (is_operator<TSymbol>()) return false;
    else return std::is_same_v<std::remove_cvref_t<TSymbol>, NTerm<typename std::remove_cvref_t<TSymbol>::_name_type>>;
}

template<class TSymbol>
constexpr inline bool is_nterm(const TSymbol& s) { return is_nterm<TSymbol>(); }


/**
 * @brief Helper function that returns whether an object is a terminals range
 */
template<class TSymbol>
constexpr inline bool is_terms_range()
{
    if constexpr (is_operator<TSymbol>() || is_term<TSymbol>() || is_nterm<TSymbol>()) return false;
    else return std::is_same_v<std::decay_t<TSymbol>, TermsRange<typename std::decay_t<TSymbol>::_name_type_start, typename std::decay_t<TSymbol>::_name_type_end>>;
}

template<class TSymbol>
constexpr inline bool is_terms_range(const TSymbol& s) { return is_terms_range<TSymbol>(); }


/**
 * @brief Helper function that returns whether an object is a single term or TermsRange
 */
template<class TSymbol>
constexpr inline bool terminal_type()
{
    return is_term<TSymbol>() || is_terms_range<TSymbol>();
}

template<class TSymbol>
constexpr inline bool terminal_type(const TSymbol& s) { return terminal_type<TSymbol>(); }


template<class TSymbol, class TChar>
constexpr bool in_terms_range(TChar c) { return in_lexical_range<TChar>(c, TSymbol::get_start(), TSymbol::get_end()); }


template<class TSymbolA, class TSymbolB>
struct terms_intersect
{
    static constexpr bool value()
    {
        if constexpr (is_term<TSymbolA>() && is_term<TSymbolB>())
            // Both are terms
            return std::is_same_v<TSymbolA, TSymbolB>;
        else if constexpr (is_term<TSymbolA>() && is_terms_range<TSymbolB>())
            // Term and TermsRange
            return check_intersect_ab<TSymbolB, TSymbolA>();
        else if constexpr (is_terms_range<TSymbolA>() && is_term<TSymbolB>())
            // Term and TermsRange
            return check_intersect_ab<TSymbolA, TSymbolB>();
        else if constexpr (is_terms_range<TSymbolA>() && is_terms_range<TSymbolB>())
            // Both are TermsRange
            return check_intersect_ranges<TSymbolA, TSymbolB>();
        else return false;
    }

protected:
    template<class TRange, class TSybmol>
    static constexpr bool check_intersect_ab()
    {
        // Iterate over a term and check each symbol
        return do_check_intersect_ab_step<0, TRange, TSybmol>();
    }

    template<std::size_t i, class TRange, class TSybmol>
    static constexpr bool do_check_intersect_ab_step()
    {
        if constexpr (TSybmol::_name_type::template at<i>() >= TRange::_name_type_start::template at<0>() && TSybmol::_name_type::template at<i>() <= TRange::_name_type_end::template at<0>())
            return true;
        else
        {
            if constexpr (i + 1 < TSybmol::_name_type::size())
                return do_check_intersect_ab_step<i+1, TRange, TSybmol>();
            else return false;
        }
    }

    template<class TRangeA, class TRangeB>
    static constexpr bool check_intersect_ranges()
    {
        // Iterate over a term and check each symbol
        constexpr bool a_start = TRangeA::_name_type_start::template at<0>();
        constexpr bool a_end = TRangeA::_name_type_end::template at<0>();
        constexpr bool b_start = TRangeB::_name_type_start::template at<0>();
        constexpr bool b_end = TRangeB::_name_type_end::template at<0>();

        // [   ( ]   )  or  [  (   )  ]
        return (b_start <= a_end && b_end >= a_start) || (a_start <= b_end || a_end >= b_start);
    }
};

template<class TSymbolA, class TSymbolB>
constexpr bool terms_intersect_v = terms_intersect<TSymbolA, TSymbolB>::value();


template<class TSymbol>
constexpr bool is_range_operator() { return std::decay_t<TSymbol>::_range_operator::value; }

template<class TSymbol>
constexpr bool is_numeric_operator() { return std::decay_t<TSymbol>::_numeric_operator::value; }


// Repeat* helper operators
template<class TSymbol>
constexpr inline std::size_t get_repeat_times()
{
    if constexpr (is_numeric_operator<TSymbol>()) return TSymbol::_times::value;
    else return std::numeric_limits<size_t>::max();
}

template<class TSymbol>
constexpr inline std::size_t get_range_from()
{
    if constexpr (is_range_operator<TSymbol>())
        return TSymbol::_from::value;
    else return std::numeric_limits<size_t>::max();;
}

template<class TSymbol>
constexpr inline std::size_t get_range_to()
{
    if constexpr (is_range_operator<TSymbol>())
        return TSymbol::_to::value;
    else return std::numeric_limits<size_t>::max();;
}


template<class TSymbol>
constexpr inline std::size_t get_repeat_times(const TSymbol& s) { return get_repeat_times<TSymbol>(); }

template<class TSymbol>
constexpr inline std::size_t get_range_from(const TSymbol& s) { return get_range_from<TSymbol>();; }

template<class TSymbol>
constexpr inline std::size_t get_range_to(const TSymbol& s) { return get_range_to<TSymbol>();; }

// Tuple helper operators

template<class TSymbol>
struct get_first { typedef std::tuple_element_t<0, typename std::remove_cvref_t<TSymbol>::term_types_tuple> type; };

template<class TSymbol>
struct get_second { typedef std::tuple_element_t<1, typename std::remove_cvref_t<TSymbol>::term_types_tuple> type; };

template<class TSymbol> using get_first_t = typename get_first<TSymbol>::type;
template<class TSymbol> using get_second_t = typename get_second<TSymbol>::type;

template<class Tuple>
std::ostream& print_symbols_tuple(const Tuple& tuple, bool is_op = false)
{
    tuple_each_tree(tuple, [&]<typename TSymbol>(std::size_t d, std::size_t i, const TSymbol& elem)
    {
        if constexpr (is_operator<std::decay_t<TSymbol>>())
        {
            std::cout << "(" << static_cast<int>(get_operator<std::decay_t<TSymbol>>()) << ")<";
            print_symbols_tuple(elem.terms, true);
            std::cout << "> ";
        }
        else
        {
            if constexpr (is_terms_range<TSymbol>())
                std::cout << elem.semantic_type() << ", ";
            else
                std::cout << elem.type() << ", ";
        }
    },
    [&](std::size_t d, std::size_t i, bool tuple_start){ std::cout << (tuple_start || is_op ? "" : "; "); });
    return std::cout;
}


template<typename Callable>
concept ReturnsBool = requires(Callable f)
{
    std::is_same_v<std::invoke_result<Callable, int>, bool>; // Check return type
};


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
    using _numeric_operator = std::false_type;
    using _range_operator = std::false_type;
    using _get_operator = op_type_t<Operator>;

    using term_types_tuple = std::tuple<std::remove_cvref_t<TSymbols>...>;
    using term_ptr_tuple = std::tuple<const std::remove_cvref_t<TSymbols>* const...>;

    [[nodiscard]] static constexpr std::size_t size() { return sizeof...(TSymbols); }

    template<class integral_const>
    [[nodiscard]] constexpr auto operator[] (const integral_const N) const { return std::get<integral_const::value>(terms); }

    /**
     * @brief Iterate over each element in terms tuple
     * @param func Lambda closure that processes each element
     */
    constexpr void each(auto func) const { each_symbol<0>(func); }

    /**
     * @brief Iterate over each element in terms tuple and pass index as lambda template parameter
     */
    constexpr void each_index(auto func) const { each_symbol_index<0>(func); }

    /**
     * @brief Iterate over each element in terms tuple until false is returned
     * @param func Lambda closure that processes each element and returns bool
     * @return false if it's returned from lambada, otherwise true
     */
    template<class Callable> requires ReturnsBool<Callable>
    constexpr bool each_or_exit(Callable func) const { return each_symbol_return<0>(func); }

    /**
     * @brief Iterate over each element in terms tuple and pass index as lambda template parameter, until false is returned
     */
    template<class Callable> requires ReturnsBool<Callable>
    constexpr bool each_index_or_exit(Callable func) const { return each_symbol_index_return<0>(func); }

    /**
     * @brief Recursive baking function that returns CStr. Manages precedence and calls baking preprocessing
     * @tparam max_precedence_t Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFBakery Base rules class
     */
    template<class max_precedence_t, class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const
    {
        return process_precedence<max_precedence_t>(rules, [&]<class next>(const auto&... args) { return preprocess_bake<next, BNFBakery>(args...); } );
    }

    /**
     * @brief Top-level recursive baking function that returns CStr
     * @tparam BNFBakery Base rules class
     */
    template<class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const
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
     * @tparam BNFBakery Base rules class
     */
    template<class max_precedence, class BNFBakery>
    constexpr auto preprocess_bake(const BNFBakery& rules) const
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
    template<std::size_t depth, class max_precedence_t, class BNFBakery, class TBuf>
    constexpr auto do_bake(const BNFBakery& rules, TBuf last) const
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
    template<class max_precedence_t, class BNFBakery, class... TSymbolPack>
    constexpr auto exec_bake_rule(const BNFBakery& rules, const TSymbolPack&... symbols) const
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
    template<class max_precedence_t, class BNFBakery>
    constexpr auto do_bake_define(const BNFBakery& rules) const
    {
        BaseOp<OpType::End> end;
        return exec_bake_rule<max_precedence_t>(rules, std::get<0>(terms), std::get<1>(terms)) + end.bake<max_precedence_t>(rules);
    }

    /**
     * @brief Bake exception operator
     */
    template<class max_precedence_t, class BNFBakery>
    constexpr auto do_bake_except(const BNFBakery& rules) const
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

    template<std::size_t i>
    constexpr void each_symbol(auto func) const
    {
        func(std::get<i>(terms));
        if constexpr (i + 1 < sizeof...(TSymbols)) each_symbol<i + 1>(func);
    }

    template<std::size_t i>
    constexpr void each_symbol_index(auto func) const
    {
        func.template operator()<i>(std::get<i>(terms));
        if constexpr (i + 1 < sizeof...(TSymbols)) each_symbol_index<i + 1>(func);
    }

    template<std::size_t i>
    constexpr bool each_symbol_return(auto func) const
    {
        if (!func(std::get<i>(terms))) return false;
        if constexpr (i + 1 < sizeof...(TSymbols)) return each_symbol_return<i + 1>(func);
        return true;
    }

    template<std::size_t i>
    constexpr bool each_symbol_index_return(auto func) const
    {
        if (!func.template operator()<i>(std::get<i>(terms))) return false;
        if constexpr (i + 1 < sizeof...(TSymbols)) return each_symbol_index_return<i + 1>(func);
        return true;
    }

    template<std::size_t i>
    constexpr auto construct_ptr_tuple() const
    {
        const auto& elem = std::get<i>(terms);
        auto elem_ptr = static_cast<const std::remove_cvref_t<decltype(elem)>* const>(&elem); // Convert elem ref to ptr
        if constexpr (i + 1 < sizeof...(TSymbols)) return std::tuple_cat(std::make_tuple(elem_ptr), construct_ptr_tuple<i+1>());
        else return std::make_tuple(elem_ptr);
    }

    /**
     * @brief Process precedence order and call the corresponding bake operation
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFBakery Base rules class
     * @param bake_func Lambda closure template which calls preprocess_bake function
     */
    template<class max_precedence_t, class BNFBakery>
    constexpr auto process_precedence(const BNFBakery& rules, auto bake_func) const
    {
        // If our precedence is lower than previous, we add grouping operator and recursively parse
        if constexpr (BNFBakery::precedence().has(Operator) && BNFBakery::precedence().has(max_precedence_t::value) && BNFBakery::precedence().less(Operator, max_precedence_t::value)) return BaseOp<OpType::Group, decltype(*this)>(*this).template bake<op_type_t<OpType::None>>(rules);
        // bake_func is a lambda, so we need to explicitly call operator()
        else return bake_func.template operator()<op_type_t<BNFBakery::precedence().max(max_precedence_t::value, Operator)>>(rules);
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
    using _numeric_operator = std::true_type;
    using _range_operator = std::false_type;
    using _times = IntegralWrapper<Times>;

    constexpr explicit BaseExtRepeat(const TSymbols&... t) : BaseOp<Operator, TSymbols...>([&](){ this->validate(); }, t...) { }

    constexpr explicit BaseExtRepeat(auto validator, const TSymbols&... t) : BaseOp<Operator, TSymbols...>(validator, t...) { }

    /**
     * @brief Recursive baking function that returns CStr. Manages precedence and calls baking preprocessing
     * @tparam max_precedence_t Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFBakery Base rules class
     */
    template<class max_precedence_t, class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const
    {
        this->template process_precedence<max_precedence_t>(rules, [&]<class next>(const auto&... args) { return preprocess_bake<next, BNFBakery>(args...); });
    }

    /**
     * @brief Top-level recursive baking function that returns CStr
     * @tparam BNFBakery Base rules class
     */
    template<class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const { return preprocess_bake<op_type_t<Operator>, BNFBakery>(rules); }

    /**
     * @brief Preprocess arguments for baking functions. Should be called only in process_precedence
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFBakery Base rules class
     */
    template<class max_precedence_t, class BNFBakery>
    constexpr auto preprocess_bake(const BNFBakery& rules) const
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
    template<class BNFBakery>
    constexpr auto to_bnf_flavor(const BNFBakery& rules) const
    {
        if constexpr (Operator == OpType::RepeatExact)
        {
            if constexpr (BNFBakery::feature_repeat_exact()) return *this;
            else return unwrap_repeat_exact<Times>(rules, std::get<0>(this->terms)).flatten();
        }
        if constexpr (Operator == OpType::RepeatGE)
        {
            if constexpr (BNFBakery::feature_repeat_ge()) return *this;
            else return Concat(unwrap_repeat_exact<Times>(rules, std::get<0>(this->terms)).flatten(), Repeat(std::get<0>(this->terms)));
        }
    }

    /**
     * @brief Recursively unwrap complex operation into N Concat operations
     * @tparam repeat How many times to repeat
     */
    template<std::size_t repeat, class BNFBakery, class TSymbol>
    constexpr auto unwrap_repeat_exact(const BNFBakery& rules, const TSymbol& symbol) const
    {
        if constexpr (repeat == 1) return symbol;
        else return Concat(unwrap_repeat_exact<repeat - 1>(rules, symbol));
    }

    /**
     * @brief unwrap_repeat_exact wrapper
     * @tparam integral_const How many times to repeat (IntegralConst<...>)
     */
    template<class integral_const, class BNFBakery, class TSymbol>
    constexpr auto unwrap_repeat_exact(const integral_const& repeat, const BNFBakery& rules, const TSymbol& symbol) const { return unwrap_repeat_exact<integral_const::value>(rules, symbol); }

    /**
     * @brief Recursively unwrap complex operation into N Optional(Concat(...)) operations
     * @tparam repeat How many times to repeat
     */
    template<std::size_t repeat, class BNFBakery, class TSymbol>
    constexpr auto unwrap_repeat_le(const BNFBakery& rules, const TSymbol& symbol) const
    {
        if constexpr (repeat == 1) return Optional(symbol);
        else return Optional(Concat(symbol, unwrap_repeat_le<repeat - 1>(rules, symbol)));
    }

    /**
     * @brief unwrap_repeat_le wrapper
     * @tparam integral_const How many times to repeat (IntegralConst<...>)
     */
    template<class integral_const, class BNFBakery, class TSymbol>
    constexpr auto unwrap_repeat_le(const integral_const& repeat, const BNFBakery& rules, const TSymbol& symbol) const { return unwrap_repeat_le<integral_const::value>(rules, symbol); }

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
    using _numeric_operator = std::false_type;
    using _range_operator = std::true_type;

protected:
    using BaseExtRepeat<Operator, From, TSymbols...>::unwrap_repeat_exact;
    using BaseExtRepeat<Operator, From, TSymbols...>::unwrap_repeat_le;

public:
    constexpr explicit BaseExtRepeatRange(const TSymbols&... t) : BaseExtRepeat<Operator, From, TSymbols...>([&](){ validate(); }, t...) { }

    /**
     * @brief Recursive baking function that returns CStr. Manages precedence and calls baking preprocessing
     * @tparam max_precedence_t Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFBakery Base rules class
     */
    template<class max_precedence_t, class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const
    {
        this->template process_precedence<max_precedence_t>(rules, [&]<class next>(const auto&... args) { return preprocess_bake<next, BNFBakery>(rules); });
    }

    /**
     * @brief Top-level recursive baking function that returns CStr
     * @tparam BNFBakery Base rules class
     */
    template<class BNFBakery>
    constexpr auto bake(const BNFBakery& rules) const { return preprocess_bake<op_type_t<Operator>, BNFBakery>(rules); }

    /**
     * @brief Preprocess arguments for baking functions. Should be called only in process_precedence
     * @tparam max_precedence Operator with the maximum preference in current scope (op_type_t<...>)
     * @tparam BNFBakery Base rules class
     */
    template<class max_precedence_t, class BNFBakery>
    constexpr auto preprocess_bake(const BNFBakery& rules) const
    {
        auto symbol = to_bnf_flavor(rules);
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(symbol)>, std::remove_cvref_t<decltype(*this)>>)
            return this->template exec_bake_rule<max_precedence_t>(rules, IntegralWrapper<From>(), IntegralWrapper<To>(), std::get<0>(this->terms));
        else return symbol.template bake<max_precedence_t>(rules);
    }

protected:
    template<class BNFBakery>
    constexpr auto to_bnf_flavor(const BNFBakery& rules) const
    {
        if constexpr (Operator == OpType::RepeatRange)
        {
            if constexpr (BNFBakery::feature_repeat_range()) return *this;
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
