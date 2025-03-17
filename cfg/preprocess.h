//
// Created by Flynn on 10.02.2025.
//

#ifndef SUPERCFG_PREPROCESS_H
#define SUPERCFG_PREPROCESS_H

#include <vector>
#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <queue>
#include <typeindex>
#include <variant>

#include "cfg/base.h"
#include "cfg/helpers.h"
#include "cfg/hashtable.h"


/**
 * @brief Token class
 * @tparam VStr Token string container
 * @tparam TokenType Related nonterminal type (name) container
 */
template<class VStr, class TokenType>
class Token
{
public:
    VStr value;
    TokenType type;

    constexpr Token(const VStr& v, const TokenType& t) : value(v), type(t) {}

    constexpr Token() = default;

    constexpr friend Token<VStr, TokenType> operator+ (const Token<VStr, TokenType>& lhs, const Token<VStr, TokenType>& rhs)
    {
        // We assume that type is identical
        return Token<VStr, TokenType>(lhs.value + rhs.value, lhs.type);
    }

    Token<VStr, TokenType>& operator+= (const Token<VStr, TokenType>& rhs) { value += rhs.value; return *this; }
};


/**
 * @brief Wrapper class that stores either a token or a nonterminal (type)
 * @tparam VStr
 * @tparam Type
 */
template<class VStr, class Type>
class GrammarSymbol
{
public:
    VStr value;
    Type type;
    const std::variant<std::true_type, std::false_type> symbol_type; // true if it's a token, false if a nterm

    constexpr GrammarSymbol(const Token<VStr, Type>& token) : value(token.value), type(token.type), symbol_type(std::true_type()) {}

    constexpr GrammarSymbol(const Type& type) : value(), type(type), symbol_type(std::false_type()) {}

    constexpr auto visit(auto process_token, auto process_nterm) const
    {
        // TODO replace all cvref_t with decay_t
        return std::visit([&](auto&& s_type){
            if constexpr (std::is_same_v<std::decay_t<decltype(s_type)>, std::true_type>)
                return process_token(value, type);
            else
                return process_nterm(type);
            }, symbol_type);
    }

    [[nodiscard]] constexpr bool is_token() const
    {
        return std::visit([&](auto s_type){ return decltype(s_type)::value; }, symbol_type);
    }
};


/**
 * @brief Container of tokens (terminals), handles the mapping between string and its related type
 * @tparam TERMS_MAX Container size
 * @tparam VStr Token string container
 * @tparam TokenType Related nonterminal type (name) container
 */
template<std::size_t TERMS_MAX, class VStr, class TokenType>
class TermsStorage
{
public:
    Token<VStr, TokenType> storage[TERMS_MAX];
    std::size_t N;

    using hashtable = std::unordered_map<VStr, TokenType>; //, typename VStr::hash>; // Should be injected in std

    template<class RulesSymbol>
    constexpr explicit TermsStorage(const RulesSymbol& rules_def)
    {
        N = iterate(rules_def, 0);
        //for (std::size_t i = N; i < TERMS_MAX; i++) { storage[i] = Token<VStr, TokenType>(); }
    }

    hashtable compile_hashmap() const
    {
        // TODO create optimal implementation for the hash function
        hashtable map;
        for (std::size_t i = 0; i < N; i++)
        {
            map.insert({storage[i].value, storage[i].type});
        }
        return map;
    }

    [[nodiscard]] constexpr bool validate() const
    {
        // Naive implementation in O*log(n)
        for (std::size_t i = 0; i < N; i++)
        {
            for (std::size_t j = i + 1; j < N; j++)
            {
                if (VStr::is_substr(storage[i].value, storage[j].value)) return false;
            }
        }
        return true;
    }

protected:
    template<class TSymbol>
    constexpr std::size_t iterate(const TSymbol& s, std::size_t ind)
    {
        s.each([&](const auto& symbol){
            if constexpr (is_operator<TSymbol>())
            {
                ind = iterate(symbol, symbol, ind); // Will always return bigger index
            }
        });
        return ind;
    }

    template<class TSymbol, class TDef>
    constexpr std::size_t iterate(const TSymbol& s, const TDef& def, std::size_t ind)
    {
        s.each([&](const auto& symbol){
            if constexpr (is_operator<decltype(symbol)>())
            {
                ind = iterate(symbol, def, ind); // Will always return bigger index
            } else {
                if constexpr (is_term<decltype(symbol)>())
                {
                    // Found a terminal symbol
                    storage[ind] = Token<VStr, TokenType>(VStr(symbol.name), TokenType(std::get<0>(def.terms).type()));
                    assert(ind + 1 < TERMS_MAX && "Maximum terminals limit reached");
                    ind++;
                }
            }
        });
        return ind; // Maximum index at this iteration
    }
};


/**
 * @brief Nonterminal type to definition mapping. Currently is suboptimal and searches in O(N)
 * @tparam TokenType Nonterminal type (name) container
 * @tparam RulesSymbol Top-level operator object
 */
template<class TokenType, class RulesSymbol>
class NTermsStorage
{
public:
    using TDefsTuple = typename RulesSymbol::term_ptr_tuple;
    TokenType types[std::tuple_size<TDefsTuple>()];
    TDefsTuple defs; // Pointers to defines

    constexpr explicit NTermsStorage(const RulesSymbol& rules) : defs(rules.get_ptr_tuple())
    {
        std::size_t i = 0;
        // Iterate over each definition
        rules.each([&](const auto& def){
            auto type = std::get<0>(def.terms).type;
            types[i] = type;
            i++;
        });
    }

    constexpr auto get(const TokenType& type) const { return do_get<0>(type); }

protected:
    template<std::size_t i>
    constexpr auto do_get(const TokenType& type) const
    {
        if (type == types[i]) return std::get<i>(defs);

        if constexpr (i + 1 < std::tuple_size<TDefsTuple>()) return do_get<i+1>(type);
        else return nullptr;
    }
};


template<class TermsTypes>
class TermsStorage
{
public:
    TermsTypes storage;

    constexpr explicit TermsStorage(const TermsTypes& types) : storage(types) {}

    template<class TermT>
    constexpr auto get(const TermT& term)
    {
        return do_get<0>(term);
    }

    template<class TermsTuple>
    constexpr auto get_all(const TermsTuple& terms)
    {
        return tuple_morph([&]<std::size_t i>(const auto& t){ return get(std::get<i>(t)); }, terms);
    }

protected:
    template<std::size_t i, class TermT>
    constexpr auto do_get(const TermT& term)
    {
        if constexpr (std::is_same_v<std::tuple_element_t<0, std::tuple_element_t<i, TermsTypes>>, TermT>)
            return std::get<1>(std::get<i>(term));
        else
        {
            if constexpr (i >= std::tuple_size_v<TermsTypes>)
                static_assert(i < std::tuple_size_v<TermsTypes>, "No such term found");
            return do_get<i+1>(term);
        }
    }
};


/**
 * @brief Terms to related NTerms mapping
 * @tparam RulesSymbol Rules operator
 */
class TermsStorageFactory
{
public:
    constexpr explicit TermsStorageFactory() = default;

    template<class RulesSymbol>
    constexpr auto build(const RulesSymbol& rules)
    {
        auto terms = tuple_flatten_layer(descend_each(rules.terms), std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(rules.terms)>>>{}, [&](const auto& def){
            return descend_each_def(std::get<0>(def), std::get<1>(def));
        });

        return TermsStorage(terms);
    }

protected:
    template<class NTerm, class TSymbol>
    constexpr auto descend_each_def(const NTerm& lhs, const TSymbol& symbol)
    {
        if constexpr (is_operator<TSymbol>())
        {
            return descend_each(symbol.terms, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(symbol.terms)>>>{}, [&](const auto& e){ return descend_each_def(lhs, e); });
        } else if constexpr (is_nterm<TSymbol>()) {
            return std::make_tuple<>();
        } else if constexpr (is_term<TSymbol>()) {
            return std::make_tuple(std::make_tuple(symbol, lhs));
        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
    }

    template<class SrcTuple, std::size_t... Ints>
    constexpr auto descend_each(const SrcTuple& op, const std::integer_sequence<std::size_t, Ints...>, auto func) const
    {
        return std::make_tuple(func(std::get<Ints>(op))...);
    }
};


/**
 * @brief Constant NTerm -> definition mapping
 * @tparam RulesSymbol
 */
template<class RulesSymbol>
class NTermsConstHashTable
{
public:
    using TDefsTuple = RulesSymbol::term_types_tuple;
    using TDefsPtrTuple = RulesSymbol::term_ptr_tuple;
    using TupleLen = IntegralWrapper<std::tuple_size_v<TDefsTuple>>;
    // Morph tuple of definitions operators into a tuple of NTerms
    using NTermsTuple = decltype(type_morph_t<std::tuple>(
            []<std::size_t index>(){ return typename get_first<std::tuple_element_t<index, TDefsTuple>>::type(); },
            TupleLen()));

    NTermsTuple nterms;
    TDefsPtrTuple defs;

    constexpr NTermsConstHashTable(const RulesSymbol& rules) :
            nterms(type_morph<std::tuple>(
                    [&]<std::size_t index>(const auto& src){ return std::get<0>(std::get<index>(src.terms).terms); }
                    , TupleLen(), rules)),
            defs(rules.get_ptr_tuple()) {}

    template<class TSymbol>
    constexpr auto get(const TSymbol& symbol) const
    {
        return do_get<0, TSymbol>(symbol);
    }

protected:
    template<std::size_t depth, class TSymbol>
    constexpr auto do_get(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TDefsTuple>(), "NTerm type not found");
        if constexpr (std::is_same_v<std::remove_cvref_t<TSymbol>, std::tuple_element_t<depth, NTermsTuple>>)
        {
            return std::get<depth>(defs);
        }
        else return do_get<depth + 1, TSymbol>(symbol);
    }
};


/**
 * @brief Single-pass tokenizer class
 * @tparam TERMS_MAX Maximum number of terminals
 * @tparam VStr Variable string class
 * @tparam TokenType Nonterminal type (name) container
 */
template<std::size_t TERMS_MAX, class VStr, class TokenType>
class Tokenizer
{
protected:
    TermsStorage<TERMS_MAX, VStr, TokenType> storage;
    using hashtable = typename TermsStorage<TERMS_MAX, VStr, TokenType>::hashtable;

public:
    template<class RulesSymbol>
    constexpr explicit Tokenizer(const RulesSymbol& root) : storage(root) { assert(storage.validate() && "Duplicate terminals found, cannot build tokens storage"); }

    hashtable init_hashtable() { return storage.compile_hashmap(); }

    template<class VText>
    std::vector<Token<VStr, TokenType>> run(const hashtable& ht, const VText& text, bool& ok) const
    {
        std::vector<Token<VStr, TokenType>> tokens;
        std::size_t pos = 0;
        for (std::size_t i = 0; i < text.size(); i++)
        {
            VStr tok = VStr::from_slice(text, pos, i + 1);
            const auto it = ht.find(tok);

            if (it != ht.end())
            {
                // Terminal found
                std::size_t n = tokens.size();
                // We shouldn't actually merge tokens
                // if (n > 0 && tokens[n - 1].type == it->second) tokens[n - 1].value += it->first;
                // else tokens.push_back(Token<VStr, TokenType>(it->first, it->second));
                tokens.push_back(Token<VStr, TokenType>(it->first, it->second));
                pos = i + 1;
            }
        }

//        assert(pos == text.size() && "Tokenization error: found unrecognized tokens");
        ok = (pos == text.size());
        return tokens;
    }
};


template<class TRulesDefOp>
class NTermHTItem
{
public:
    const std::remove_cvref_t<get_second<TRulesDefOp>>* const ptr;
    //constexpr NTermHTItem(const TRulesDef* const addr) : ptr(addr) {}

    constexpr NTermHTItem() : ptr(nullptr) {}

    constexpr NTermHTItem(const TRulesDefOp& op) : ptr(&std::get<1>(op)) {}
};


/**
 * @brief TokenType (str) to nterm definition mapping. Uses std::variant to store pointers
 */
template<class TokenType, class RulesSymbol>
class NTermsHashTable
{
public:
    using TDefsTuple = typename RulesSymbol::term_types_tuple;
    using TDefsPtrTuple = typename RulesSymbol::term_ptr_tuple;
    using TupleLen = IntegralWrapper<std::tuple_size_v<TDefsTuple>>();

    // Morph tuple into std::variant type
    using NTermVariant = decltype(type_morph_t<std::variant>(
            []<std::size_t index>(){ return NTermHTItem<std::tuple_element_t<index, TDefsTuple>()>(); },
            TupleLen()));

    std::unordered_map<TokenType, NTermVariant> storage;

    constexpr explicit NTermsHashTable(const RulesSymbol& rules)
    {
        std::size_t i = 0;
        // Iterate over each definition
        rules.each([&](const auto& def){
            NTermHTItem term(def);
            TokenType t(std::get<0>(def.terms).type());
            storage.insert(t, term);
        });
    }

    /**
     * @brief Get the nterm definition from a TokenType. Uses an unordered_map
     * @param type TokenType (key)
     * @param func Lambda that accepts the value
     */
    auto get(const TokenType& type, auto func) const
    {
        return std::visit([func](const auto& ht_item){ return func(ht_item.ptr); }, storage[type]);
    }
};


template<class TokenType, class Terms, class NTerms>
class SymbolsHashTable
{
public:
    using TermsTuple = Terms;
    using NTermsTuple = NTerms;
    using TermsVariant = typename TypesHashTable<TokenType, Terms>::ValuesVariant;
    using NTermsVariant = typename TypesHashTable<TokenType, NTerms>::ValuesVariant;

    TypesHashTable<TokenType, Terms> terms_map;
    TypesHashTable<TokenType, NTerms> nterms_map;

    constexpr SymbolsHashTable(const Terms& terms, const NTerms& nterms) : terms_map(), nterms_map()
    {
        tuple_each(terms, [&](const auto& term){
            terms_map.insert(term.type(), term);
        });

        tuple_each(nterms, [&](const auto& nterm){
            terms_map.insert(nterm.type(), nterm);
        });
    }

    auto get_term(const TokenType& type, auto func)
    {
        return terms_map.get(type, func);
    }

    auto get_nterm(const TokenType& type, auto func)
    {
        return nterms_map.get(type, func);
    }
};


class SymbolsHashTableFactory
{
public:
    constexpr SymbolsHashTableFactory() = default;

    template<class TokenType, class RulesSymbol>
    constexpr auto build(const RulesSymbol& rules) const
    {
        auto nterms = tuple_unique(find_nterms(rules));
        auto terms = find_terms(rules);

        return SymbolsHashTable<TokenType>(terms, nterms);
    }

protected:
    template<class TSymbol>
    constexpr auto find_nterms(const TSymbol& elem) const
    {
        static_assert(is_operator<TSymbol>(), "Symbol is not an operator");
        static_assert(get_operator<TSymbol>() == OpType::Define, "Operator is not a root symbol");
        return tuple_morph([&]<std::size_t i>(const auto& def){
            return std::get<0>(std::get<i>(def)); // Get the nterm definition (on the left)
        }, elem.terms);
    }

    template<class TSymbol>
    constexpr auto find_terms(const TSymbol& elem) const
    {
        if constexpr (is_operator<TSymbol>())
        {
            return descend_each(elem.terms, std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(elem.terms)>>>{}, [&](const auto& e){ return find_terms(e); });
        } else if constexpr (is_nterm<TSymbol>()) {
            return std::make_tuple<>();
        } else if constexpr (is_term<TSymbol>()) {
            return std::make_tuple(elem);
        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");
    }

    template<class SrcTuple, std::size_t... Ints>
    constexpr auto descend_each(const SrcTuple& op, const std::integer_sequence<std::size_t, Ints...>, auto func) const
    {
        return std::make_tuple(func(std::get<Ints>(op))...);
    }
};



/**
 * @brief A mapping between NTerm and other nonterminals definitions where it's present (NTerm -> tuple<NTerm...>)
 * @tparam TDefsTuple Definitions tuple (rules.get_ptr_tuple)
 * @tparam TRuleTree Tuple of NTerms tuples
 */
template<class TDefsTuple, class TRuleTree>
class ReverseRuleTree
{
public:
    TDefsTuple defs;
    TRuleTree tree;

    constexpr ReverseRuleTree(const TDefsTuple& defs, const TRuleTree& tree) : defs(defs), tree(tree) {}

    template<class TSymbol>
    constexpr auto get(const TSymbol& symbol) const
    {
        return do_get<0>(symbol);
    }

protected:
    template<std::size_t depth, class TSymbol>
    constexpr auto do_get(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TDefsTuple>(), "NTerm type not found");
        if constexpr (std::is_same_v<std::remove_cvref_t<TSymbol>, std::tuple_element_t<depth, TDefsTuple>>)
        {
            return std::get<depth>(tree);
        }
        else return do_get<depth + 1, TSymbol>(symbol);
    }
};



/**
 * @brief Construct a ReverseRuleTree (call build())
 */
class ReverseRuleTreeFactory
{
public:
    constexpr ReverseRuleTreeFactory() {}

    template<class RulesSymbol>
    constexpr auto build(const RulesSymbol& rules) const
    {
        typename RulesSymbol::term_ptr_tuple defs(rules.get_ptr_tuple());
        const auto nterms = reverse_rule_tree_for_symbol<0>(rules);

        return ReverseRuleTree(defs, nterms);
    }

protected:
    template<std::size_t depth, class RulesSymbol>
    constexpr auto reverse_rule_tree_for_symbol(const RulesSymbol& rules) const
    {
        const auto& nterm = std::get<0>(std::get<depth>(rules.terms)); // key (nterm)
        const auto nterms = iterate_over_rules<0>(rules, nterm); // value (related definitions)

        if constexpr (depth + 1 >= std::tuple_size<typename RulesSymbol::term_types_tuple>()) return std::make_tuple(nterms);
        else return std::tuple_cat(nterms, reverse_rule_tree_for_symbol<depth+1>(rules));
    }

    template<std::size_t depth, class RulesSymbol, class TSymbol>
    constexpr auto iterate_over_rules(const RulesSymbol& rules, const TSymbol& nterm) const
    {
        // Iterate over all rules
        const auto& rule_nterm = std::get<0>(std::get<depth>(rules.terms));
        const auto& rule_def = std::get<1>(std::get<depth>(rules.terms));

        // Check if we compare the type with its own definition
        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(rule_nterm)>, TSymbol>())
        {
            // We need to skip over this element
            if constexpr (depth + 1 >= std::tuple_size_v<typename RulesSymbol::term_types_tuple>) return std::tuple<>(); // TODO cover this case when last term is the same
            return iterate_over_rules<depth+1>(rules, nterm);
        } else {
            // Check if the element is in this definition
            const bool found = is_nterm_in_rule(rules, nterm);

            if constexpr (depth + 1 >= std::tuple_size_v<typename RulesSymbol::term_types_tuple>)
                return (found ? std::make_tuple(rule_nterm) : std::make_tuple<>());
            else
                return (found ? std::tuple_cat(rule_nterm, iterate_over_rules<depth+1>(rules, nterm)) : iterate_over_rules<depth+1>(rules, nterm));
        }
    }

    template<class TSymbol, class TNTerm>
    constexpr bool is_nterm_in_rule(const TSymbol& symbol, const TNTerm& nterm) const
    {
        // Descend over all operators and check if it's present in this rule definition
        if constexpr (is_nterm<TSymbol>())
        {
            return std::is_same_v<TSymbol, TNTerm>();
        }
        else if constexpr (is_operator<TSymbol>())
        {
            return process_op<0>(symbol);
        }
        else return false; // We need to gracefully handle this case
    }

    template<std::size_t depth, class TSymbol>
    constexpr bool process_op(const TSymbol& symbol) const
    {
        const bool found = is_nterm_in_rule(std::get<depth>(symbol.terms));
        if (found) return true;

        if constexpr (depth + 1 < std::tuple_size_v<std::decay_t<decltype(symbol.terms)>>)
        {
            return found;
        } else {
            return process_op<depth + 1>(symbol);
        }
    }
};



#endif //SUPERCFG_PREPROCESS_H
