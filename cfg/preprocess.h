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
#include <utility>

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


class true_t : public std::true_type
{
public:
    constexpr true_t(const true_t& other) {}

    constexpr true_t() = default;
};

class false_t : public std::false_type
{
public:
    constexpr false_t(const false_t& other) {}

    constexpr false_t() = default;
};


/**
 * @brief A set of N types in a constant vector
 * @tparam Type Runtime nterm type class
 */
template<class Type>
class TypeSet
{
public:
    ConstVec<Type> types;

    // Initialize a singleton
    constexpr explicit TypeSet(const Type& type) : types(type) {}

    // Initialize with an array of types
    template<std::size_t N>
    constexpr explicit TypeSet(const std::array<Type, N>& src) : types(to_homogeneous_tuple(src)) {} // May be slower than direct init from a tuple

    // Initialize with an array of types
    template<class... Elems>
    constexpr explicit TypeSet(const std::tuple<Elems...>& src) : types(src) {}

    // Copy ctor
    constexpr TypeSet(const TypeSet<Type>& rhs) : types(rhs.types) {}

    // Move assignment
    TypeSet<Type>& operator=(const TypeSet<Type>& rhs) = default;

    // Is this types container a singleton
    static constexpr bool is_singleton() { return false; }

    [[nodiscard]] constexpr std::size_t size() const { return types.size(); }

    constexpr const Type& operator[](std::size_t i) const { return types[i]; }

    Type& operator[](std::size_t i) { return types[i]; }

    constexpr const Type& front() const { return types[0]; }

    Type& front() { return types[0]; }

    ConstVec<Type>& vec() { return types; }

    constexpr const ConstVec<Type>& vec() const { return types; }

    // Operations
    friend std::ostream& operator<<(std::ostream& os, const TypeSet<Type>& type)
    {
        os << "{" << type.types << "}";
        return os;
    }
};


/**
 * @brief Legacy type container. Is a proxy for Type class
 */
template<class Type>
class TypeSingleton : public Type
{
public:
    // Include ctors
    using Type::Type;

    // Implicit copy ctor
    constexpr TypeSingleton(const Type& rhs) : Type(rhs) {}

    // Move assignment
    TypeSingleton<Type>& operator=(const TypeSingleton<Type>& rhs) = default;

    static constexpr bool is_singleton() { return true; }

    constexpr const auto& front() const { return *this; }

    auto& front() { return *this; }

    constexpr std::size_t size() const { return 1; }

    constexpr const Type& operator[](std::size_t i) const { return *this; }

    Type& operator[](std::size_t i) { return *this; }
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
    std::variant<true_t, false_t> symbol_type; // true if it's a token, false if a nterm

    // Term
    constexpr explicit GrammarSymbol(const VStr& value, const Type& type) : value(value), type(type), symbol_type(true_t()) {}

    // NTerm
    constexpr explicit GrammarSymbol(const Type& type) : value(), type(type), symbol_type(false_t()) {}

    // Copy constructor
    constexpr GrammarSymbol(const GrammarSymbol& rhs) : value(rhs.value), type(rhs.type), symbol_type(rhs.symbol_type) {}

    // Move assignment
    GrammarSymbol& operator=(const GrammarSymbol& rhs) = default;

    constexpr auto visit(auto process_token, auto process_nterm) const
    {
        // TODO replace all cvref_t with decay_t
        return std::visit([&](auto&& s_type){
            if constexpr (std::is_same_v<std::decay_t<decltype(s_type)>, std::true_type>)
                return process_token();
            else
                return process_nterm();
            }, symbol_type);
    }

    [[nodiscard]] constexpr bool is_token() const
    {
        return std::visit([&](auto s_type){ return decltype(s_type)::value; }, symbol_type);
    }

    void with_types(const auto& symbols_ht, auto func) const
    {
        if (is_token())
        {
            // Return either a Term or a Range
            symbols_ht.get_term(value, func);
        } else {
            symbols_ht.get_nterm(type.front(), func);
        }
    }
};


/**
 * @brief Container of tokens (terminals), handles the mapping between string and its related type
 * @tparam VStr Token string container
 * @tparam TokenType Related nonterminal type (name) container
 */
template<class VStr, class TokenType>
class TermsStorage
{
public:
    std::vector<Token<VStr, TokenType>> storage;

    using hashtable = std::unordered_map<VStr, TokenType>; //, typename VStr::hash>; // Should be injected in std

    template<class RulesSymbol>
    constexpr explicit TermsStorage(const RulesSymbol& rules_def)
    {
        iterate(rules_def);
        //for (std::size_t i = N; i < TERMS_MAX; i++) { storage[i] = Token<VStr, TokenType>(); }
    }

    hashtable compile_hashmap() const
    {
        // TODO create optimal implementation for the hash function
        hashtable map;
        for (std::size_t i = 0; i < storage.size(); i++)
        {
            map.insert({storage[i].value, storage[i].type});
        }
        return map;
    }

    [[nodiscard]] constexpr bool validate() const
    {
        // Naive implementation in O*log(n)
        /*for (std::size_t i = 0; i < N; i++)
        {
            for (std::size_t j = i + 1; j < N; j++)
            {
                if (VStr::is_substr(storage[i].value, storage[j].value)) return false;
            }
        }*/
        return true;
    }

protected:
    template<class TSymbol>
    constexpr void iterate(const TSymbol& s)
    {
        s.each([&](const auto& symbol){
            if constexpr (is_operator<TSymbol>())
            {
                iterate(symbol, symbol); // Will always return bigger index
            }
        });
    }

    template<class TSymbol, class TDef>
    constexpr void iterate(const TSymbol& s, const TDef& def)
    {
        s.each([&](const auto& symbol){
            if constexpr (is_operator<decltype(symbol)>())
            {
                iterate(symbol, def); // Will always return bigger index
            } else {
                if constexpr (is_term<decltype(symbol)>())
                {
                    // Found a terminal symbol
                    storage.push_back(Token<VStr, TokenType>(VStr(symbol.name), TokenType(std::get<0>(def.terms).type())));
                }
                static_assert(!is_terms_range<decltype(symbol)>(), "Legacy lexer does not support TermsRange");
            }
        });
        //return ind; // Maximum index at this iteration
    }
};


/**
 * @brief Cache that contains the list of all rules and the mapping between rule and terms that it contains. Used for the TermsTypeMap calculation
 */
template<class TDefsTuple, class TermsTuple, class TermsUnion>
class TermsTreeCache
{
public:
    TDefsTuple defs;
    TermsTuple terms;
    TermsUnion all_terms;

    constexpr explicit TermsTreeCache(const TDefsTuple& defs_t, const TermsTuple& terms_t, const TermsUnion& all_t) : defs(defs_t), terms(terms_t), all_terms(all_t) {}

    template<class TSymbol>
    constexpr auto get(const TSymbol& symbol) const
    {
        return do_get<0>(symbol);
    }

protected:
    template<std::size_t depth, class TSymbol>
    constexpr auto do_get(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TDefsTuple>, "NTerm type not found");
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::tuple_element_t<0, typename std::tuple_element_t<depth, TDefsTuple>::term_types_tuple>>)
            return std::get<depth>(terms);
        else return do_get<depth + 1, TSymbol>(symbol);
    }
};


/**
 * @brief Calculate the full type for a token, including the rules where each token is present
 * @tparam VStr Token string container
 * @tparam TokenType Container for multiple token types
 */
template<class VStr, class TokenType, class TermsTuple, class TermsDefsTuple>
class TermsTypeMap
{
public:
    TermsTuple terms;
    TermsDefsTuple nterms;
    //std::vector<Token<VStr, TokenType>> storage;
    std::unordered_map<VStr, TypeSet<TokenType>> storage;

    constexpr explicit TermsTypeMap(const TermsTuple& defs_t, const TermsDefsTuple& terms_t) : terms(defs_t), nterms(terms_t) {}

    void populate_ht() { do_populate_ht<0>(); }

    void populate_ht_with_dup() { do_populate_ht_with_dup<0>(); }

    template<class TSymbol>
    constexpr auto get(const TSymbol& symbol) const
    {
        return do_get<0>(symbol);
    }

    constexpr auto get_rt(const VStr& token_str) const
    {
        // Returns a ConstVec of types
        return storage[token_str];
    }

    constexpr auto get_it(const VStr& token_str) const
    {
        // Returns an iterator to ConstVec of types
        return storage.find(token_str);
    }

    constexpr auto end() const { return storage.end(); }

protected:
    template<std::size_t i>
    void do_populate_ht()
    {
        const auto& symbol = std::get<i>(terms);
        const auto value = TypeSet<TokenType>(tuple_morph([]<std::size_t k>(const auto& src){ return VStr(std::get<k>(src).type()); }, std::get<i>(nterms)));

        if constexpr (is_term<decltype(symbol)>())
        {
            storage.insert({VStr(std::get<i>(terms).type()), value});
        } else {
            // Terms range
            symbol.each_range([&](const auto s){
                storage.insert({VStr(s), value});
            });
        }
        if constexpr (i + 1 < std::tuple_size_v<std::decay_t<TermsTuple>>)
            do_populate_ht<i+1>();
    }

    template<std::size_t i>
    void do_populate_ht_with_dup()
    {
        const auto& symbol = std::get<i>(terms);

        // Morph tuple into runtime TypeSet
        const auto value = TypeSet<TokenType>(tuple_morph([]<std::size_t k>(const auto& src){ return VStr(std::get<k>(src).type()); }, std::get<i>(nterms)));

        if constexpr (is_term<decltype(symbol)>())
        {
            const auto& type = VStr(std::get<i>(terms).type());
            auto it = storage.find(type);
            if (it != storage.end())
            {
                // Intersection
                it->second.types = vec_union(it->second.types, value.types);
            } else storage.insert({type, value});
        } else {
            // Terms range
            symbol.each_range([&](const auto s){
                const auto& type = VStr(s);
                auto it = storage.find(type);
                if (it != storage.end())
                {
                    // Intersection
                    it->second.types = vec_union(it->second.types, value.types);
                } else storage.insert({type, value});
                storage.insert({type, value});
            });
        }
        if constexpr (i + 1 < std::tuple_size_v<std::decay_t<TermsTuple>>)
            do_populate_ht_with_dup<i+1>();
    }

    template<std::size_t depth, class TSymbol>
    constexpr auto do_get(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TermsTuple>, "NTerm type not found");
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::tuple_element_t<0, typename std::tuple_element_t<depth, TermsTuple>::term_types_tuple>>)
        {
            return std::get<depth>(nterms);
        }
        else return do_get<depth + 1, TSymbol>(symbol);
    }
};


/**
 * @brief Nonterminal type to definition mapping
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
class TermsMap
{
public:
    TermsTypes storage;

    constexpr explicit TermsMap(const TermsTypes& types) : storage(types) {}

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
            return std::get<1>(std::get<i>(storage));
        else
        {
            if constexpr (i >= std::tuple_size_v<TermsTypes>)
                static_assert(i < std::tuple_size_v<TermsTypes>, "No such term found");
            return do_get<i+1>(term);
        }
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
    using TDefsTuple = typename RulesSymbol::term_types_tuple;
    using TDefsPtrTuple = typename RulesSymbol::term_ptr_tuple;
    using TupleLen = IntegralWrapper<std::tuple_size_v<TDefsTuple>>;
    // Morph tuple of definitions operators into a tuple of NTerms
    using NTermsTuple = decltype(type_morph_t<std::tuple>(
            []<std::size_t index>(){ return typename get_first<std::tuple_element_t<index, TDefsTuple>>::type(); },
            TupleLen()));

    NTermsTuple nterms;
    TDefsPtrTuple defs;

    constexpr explicit NTermsConstHashTable(const RulesSymbol& rules) :
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
        static_assert(depth < std::tuple_size_v<TDefsTuple>, "NTerm type not found");
        if constexpr (std::is_same_v<std::remove_cvref_t<TSymbol>, std::tuple_element_t<depth, NTermsTuple>>)
        {
            return std::get<depth>(defs);
        }
        else return do_get<depth + 1, TSymbol>(symbol);
    }
};


/**
 * @brief Single-pass tokenizer class. Does not support multiple tokens of the same type
 * @tparam VStr Variable string class
 * @tparam TokenType Nonterminal type (name) container
 */
template<class VStr, class TokenType>
class LexerLegacy
{
protected:
    TermsStorage<VStr, TokenType> storage;
    using hashtable = typename TermsStorage<VStr, TokenType>::hashtable;

public:
    using TokenSetClass = TypeSingleton<TokenType>;
    hashtable ht;

    [[nodiscard]] static constexpr bool is_legacy() { return true; }

    template<class RulesSymbol>
    constexpr explicit LexerLegacy(const RulesSymbol& root) : storage(root), ht(storage.compile_hashmap()) { assert(storage.validate() && "Duplicate terminals found, cannot build tokens storage"); }

    template<class VText>
    std::vector<Token<VStr, TypeSingleton<TokenType>>> run(const VText& text, bool& ok) const
    {
        std::vector<Token<VStr, TypeSingleton<TokenType>>> tokens;
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
                tokens.push_back(Token<VStr, TypeSingleton<TokenType>>(it->first, TypeSingleton<TokenType>(it->second)));
                pos = i + 1;
            }
        }

//        assert(pos == text.size() && "Tokenization error: found unrecognized tokens");
        ok = (pos == text.size());
        return tokens;
    }

    void prettyprint() const { } // Nothing to print
};


/**
 * @brief Single-pass tokenizer class with complete tokens typing support
 * @tparam VStr Variable string class
 * @tparam TokenType Nonterminal type (name) container
 */
template<class VStr, class TokenType, class TermsTMap>
class Lexer
{
public:
    TermsTMap terms_map;

    using TokenSetClass = TypeSet<TokenType>;
    [[nodiscard]] static constexpr bool is_legacy() { return false; }

    constexpr explicit Lexer(const TermsTMap& terms) : terms_map(terms) {}

    template<class VText>
    std::vector<Token<VStr, TypeSet<TokenType>>> run(const VText& text, bool& ok) const
    {
        std::vector<Token<VStr, TypeSet<TokenType>>> tokens;
        std::size_t pos = 0;
        for (std::size_t i = 0; i < text.size(); i++)
        {
            VStr tok = VStr::from_slice(text, pos, i + 1);
            const auto it = terms_map.get_it(tok);

            if (it != terms_map.end())
            {
                // Terminal found
                // std::size_t n = tokens.size();
                // We shouldn't actually merge tokens
                // if (n > 0 && tokens[n - 1].type == it->second) tokens[n - 1].value += it->first;
                // else tokens.push_back(Token<VStr, TokenType>(it->first, it->second));
                tokens.push_back(Token<VStr, TypeSet<TokenType>>(it->first, it->second));
                pos = i + 1;
            }
        }

        //        assert(pos == text.size() && "Tokenization error: found unrecognized tokens");
        ok = (pos == text.size());
        //print_tokens(tokens);
        return tokens;
    }

    void print_tokens(const std::vector<Token<VStr, TypeSet<TokenType>>>& tokens) const
    {
        for (const auto& tok : tokens)
            std::cout << "{" << tok.value << ", " << tok.type << "} ";
        std::cout << std::endl;
    }

    void prettyprint() const { do_print<0>(); }
protected:
    template<std::size_t depth>
    void do_print() const
    {
        const auto& term = std::get<depth>(terms_map.terms);
        const auto& types = std::get<depth>(terms_map.nterms);

        if constexpr (is_terms_range<std::decay_t<decltype(term)>>())
            std::cout << term.semantic_type() << " -> ";
        else
            std::cout << term.type() << " -> ";
        print_symbols_tuple(types) << std::endl;

        if constexpr (depth + 1 < std::tuple_size_v<std::decay_t<decltype(terms_map.terms)>>)
            do_print<depth+1>();
    }
};



template<class TRulesDefOp>
class NTermHTItem
{
public:
    const std::remove_cvref_t<get_second<TRulesDefOp>>* const ptr;
    //constexpr NTermHTItem(const TRulesDef* const addr) : ptr(addr) {}

    constexpr NTermHTItem() : ptr(nullptr) {}

    constexpr NTermHTItem(const TRulesDefOp& op) : ptr(&std::get<1>(op.terms)) {}
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
    using TupleLen = IntegralWrapper<std::tuple_size_v<TDefsTuple>>;

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
        tuple_each(terms, [&](std::size_t i, const auto& term){
            // Iterate over each element in lexical range
            if constexpr (is_terms_range<std::decay_t<decltype(term)>>())
            {
                term.each_range([&](auto symbol){
                    const auto t = TokenType(symbol);
                    // if (!terms_map.contains(t))
                    terms_map.insert(t, term); // We don't care what to insert
                });
            } else {
                const auto t = TokenType(term.type());
                terms_map.insert(t, term);
            }
        });

        tuple_each(nterms, [&](std::size_t i, const auto& nterm){
            nterms_map.insert(TokenType(nterm.type()), nterm);
        });
    }

    auto get_term(const TokenType& type, auto func) const
    {
        return terms_map.get(type, func);
    }

    auto get_nterm(const TokenType& type, auto func) const
    {
        return nterms_map.get(type, func);
    }
};


/**
 * @brief A mapping between NTerm and other nonterminals definitions where it's present (NTerm -> tuple<NTerm...>)
 * @tparam TDefsTuple A tuple of top-level definitions
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

    template<class TokenType>
    auto generate_hashtable() const
    {
        std::unordered_map<TokenType, std::vector<TokenType>> ht;
        tuple_each(defs, [&](std::size_t i, const auto& elem){
            // Morph related elements into homogeneous vector
            ht.insert({elem.type(), type_morph<std::vector>([&]<std::size_t j>(const auto& t){ return std::get<j>(t).type(); },
                IntegralWrapper<std::tuple_size_v<std::decay_t<decltype(std::get<i>(tree))>>>(), std::get<i>(tree))});
        });
        return ht;
    }

    template<class VStr>
    void prettyprint() const
    {
        do_print<0, VStr>();
    }

protected:
    template<std::size_t depth, class TSymbol>
    constexpr auto do_get(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TDefsTuple>, "NTerm type not found");
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::tuple_element_t<0, typename std::tuple_element_t<depth, TDefsTuple>::term_types_tuple>>)
        {
            return std::get<depth>(tree);
        }
        else return do_get<depth + 1, TSymbol>(symbol);
    }

    template<std::size_t depth, class VStr>
    void do_print() const
    {
        const auto& nt = std::get<depth>(defs);
        const auto& tree_rhs = std::get<depth>(tree);
        std::cout << VStr(std::get<0>(nt.terms).type()) << " -> ";
        print_symbols_tuple(tree_rhs) << std::endl;

        if constexpr (depth + 1 < std::tuple_size_v<decltype(defs)>)
            do_print<depth+1, VStr>();
    }
};


/**
 * @brief A class that checks if a matching element can be reduced in current parsing context. The check is performed for 1 step ahead. At the initial step,
 */
template<class TMatches, class RulesPosPairs, class TRules, class FullRRTree, bool do_prettyprint>
class ReducibilityChecker1
{
public:
    TMatches matches;
    RulesPosPairs pos;
    TRules rules;
    FullRRTree rr_all;
    std::array<std::size_t, std::tuple_size_v<TRules>> context;
    std::size_t last_ctx_pos;

    constexpr ReducibilityChecker1(const TMatches& m, const RulesPosPairs& p, const TRules& r, const FullRRTree& rr_all) : matches(m), pos(p), rules(r), rr_all(rr_all) {}

    /**
     * @brief Reset the context of the array. Should be performed at the start of parsing
     */
    void reset_ctx() { context.fill(0); last_ctx_pos = std::numeric_limits<std::size_t>::max(); }

    /**
     * @brief Check if a symbol can be reduced in the current context.
     * @param match Potential match candidate
     * @param stack_size Current size of the stack
     * @param terms2nterms Terms to definitions mapping
     * @param descend A function which takes a stack position and a definition and performs a recursive descend. It must return the number of matching elements. Note that it should descend over a stack with the original match applied
     */
    template<class TMatch, class NTermsMap>
    bool can_reduce(const TMatch& match, std::size_t stack_size, const NTermsMap& terms2nterms, auto descend)
    {
        const auto& res = get(match);

        if constexpr (std::tuple_size_v<std::decay_t<decltype(res)>> == 0)
            return true; // Nothing to check

        if constexpr (do_prettyprint)
        {
            std::cout << "ctx : ";
            for (const std::size_t idx : context) std::cout << idx << " ";
            std::cout << std::endl;
        }

        return tuple_each_or_return(res, [&](std::size_t i, const auto& rule_pair){
            const auto& [rule, first_pos] = rule_pair;
            constexpr std::size_t ctx_pos = get_ctx_index<0, std::decay_t<decltype(rule)>>();
            static_assert(ctx_pos < std::tuple_size_v<TRules>, "RC(1) : could not find reverse rule");
            // Check if the context exists
            // Note: it cannot solve rules with common prefixes

            // Check for the context first
            if constexpr (std::tuple_size_v<std::decay_t<FullRRTree>> > 0)
            {
                const auto& idx = get_rr_all(match);
                for (const std::size_t pos : idx)
                {
                    if (pos == get_ctx_index<0, std::decay_t<decltype(match)>>()) // check if we compare the symbol against itself
                    {
                        if (context[pos] > 1) [[unlikely]]
                        {
                            if constexpr (do_prettyprint) std::cout << "cannot reduce: conflicting nested ctx " << pos << std::endl;
                            return false;
                        }
                    } else if (context[pos] > 0) {
                        if constexpr (do_prettyprint) std::cout << "cannot reduce: conflicting ctx " << pos << std::endl;
                        return false;
                    }
                }
            }

            auto perform_check_at_symbol_start = [&](){
                // Out of bounds check: the rule cannot fit in current stack
                if (first_pos() >= stack_size)
                    return false; // Continue

                std::size_t stack_i = stack_size - 1 - first_pos();
                std::size_t parsed = descend(stack_i, std::get<1>(terms2nterms.get(rule)->terms));
                if constexpr (do_prettyprint)
                    std::cout << ", i: " << stack_i << ", parsed: " << parsed << "/" << first_pos() << std::endl;
                if (parsed >= first_pos()) // We can theoretically reduce everything up to this symbol OR there is no match at all
                {
                    //context[ctx_pos]++; // Successful match
                    last_ctx_pos = ctx_pos;
                    return true;
                }
                return false;
            };

            if (context[ctx_pos] > 0)
            {
                // Context already exists, we should start from position
                // It can be disabled for lazy evaluation
                if constexpr (do_prettyprint)
                    std::cout << "  rc : " << rule.type() << " : currently in ctx" << std::endl;
                // If there is a match at symbol start, then we have descended inside and need to increment the ctx
                perform_check_at_symbol_start(); // ctx++
                return true;
            } else {
                // No context found, pick the first appearance of char
                if constexpr (do_prettyprint)
                    std::cout << "  rc : " << rule.type();
                return perform_check_at_symbol_start();
            }
        });

        return true; // We don't need to check these symbols
    }

    void apply_ctx()
    {
        if (last_ctx_pos != std::numeric_limits<std::size_t>::max())
        {
            context[last_ctx_pos]++;
            last_ctx_pos = std::numeric_limits<std::size_t>::max();
        }
    }

    /**
     * @brief Update the context for the reduced symbol. This function should be called on each successful reduce operation
     * @param symbol Chosen match candidate
     */
    template<class TSymbol>
    void apply_reduce(const TSymbol& symbol)
    {
        do_apply_reduce<0>(symbol);
    }

    template<class TSymbol>
    constexpr auto get(const TSymbol& symbol) const
    {
        return do_get<0>(symbol);
    }

    template<class TSymbol>
    constexpr auto get_rr_all(const TSymbol& symbol) const
    {
        return do_get_rr_all<0>(symbol);
    }

    template<class VStr>
    void prettyprint() const
    {
        do_print<0, VStr>();
    }

protected:
    template<std::size_t depth, class TSymbol>
    constexpr auto do_get(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TMatches>, "NTerm type not found");
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::decay_t<std::tuple_element_t<0, typename std::tuple_element_t<depth, TMatches>::term_types_tuple>>>)
        {
            return std::get<depth>(pos);
        } else {
            return do_get<depth + 1>(symbol);
        }
    }

    template<std::size_t depth, class TSymbol>
    constexpr auto do_get_rr_all(const TSymbol& symbol) const
    {
        static_assert(depth < std::tuple_size_v<TMatches>, "NTerm type not found");
        if constexpr (std::is_same_v<std::decay_t<TSymbol>, std::decay_t<std::tuple_element_t<0, typename std::tuple_element_t<depth, TMatches>::term_types_tuple>>>)
        {
            return std::get<depth>(rr_all);
        } else {
            return do_get_rr_all<depth + 1>(symbol);
        }
    }

    template<std::size_t depth, class TSymbol>
    void do_apply_reduce(const TSymbol& symbol)
    {
        if constexpr (std::is_same_v<std::decay_t<std::tuple_element_t<depth, TRules>>, std::decay_t<TSymbol>>)
            context[depth] = (context[depth] > 0 ? context[depth] - 1 : 0); // Reset context
        else
        {
            if constexpr (depth + 1 < std::tuple_size_v<TRules>)
                do_apply_reduce<depth+1>(symbol);
            // Else we don't care about this symbol
        }
    }

    template<std::size_t depth, class TSymbol>
    [[nodiscard]] static constexpr std::size_t get_ctx_index()
    {
        if constexpr (depth >= std::tuple_size_v<TRules>)
            return std::numeric_limits<std::size_t>::max(); // No such symbol
        else
        {
            if constexpr (std::is_same_v<std::decay_t<std::tuple_element_t<depth, TRules>>, std::decay_t<TSymbol>>)
                return depth;
            else
                return get_ctx_index<depth+1, TSymbol>();
        }
    }

    template<std::size_t depth, class VStr>
    void do_print() const
    {
        const auto& nt = std::get<depth>(matches);
        const auto& tree_rhs = std::get<depth>(pos);
        std::cout << VStr(std::get<0>(nt.terms).type()) << " -> ";
        tuple_each(tree_rhs, [&](std::size_t i, const auto& elem){
            const auto& [rule, first_pos] = elem;
            std::cout << "{" << VStr(rule.type()) << ", " << first_pos() << "}, ";
        });
        std::cout << std::endl;

        if constexpr (depth + 1 < std::tuple_size_v<decltype(matches)>)
            do_print<depth+1, VStr>();
    }
};


class NoReducibilityChecker {};


// Heuristics preprocessing helpers
enum class HeuristicFeatures : std::uint64_t
{
    ContextManagement = 0x1, // Build the FullRRTree
};


template<class UniqueRelatedRules, class FullRRTree>
class HeuristicPreprocessor
{
public:
    UniqueRelatedRules unique_rr;
    FullRRTree full_rr;

    constexpr HeuristicPreprocessor(const UniqueRelatedRules& u_rr, const FullRRTree& full_rr_tree) : unique_rr(u_rr), full_rr(full_rr_tree) {}
};

class NoPrettyPrinter
{
public:
    template<class VStr, class TokenTSet, class TokenType>
    void update_stack(const std::vector<GrammarSymbol<VStr, TokenTSet>>& stack, const std::vector<ConstVec<TokenType>>& related_types, const ConstVec<TokenType>& intersect, int idx)
    {
        // do nothing
    }

    template<class VStr, class TokenTSet, class TSymbol>
    void update_descend(const std::vector<GrammarSymbol<VStr, TokenTSet>>& stack, const TSymbol& rule, std::size_t idx, std::size_t candidate, std::size_t total, std::size_t parsed, bool found) {}

    template<class Tree>
    void update_ast(const Tree& tree) {}

    template<std::size_t N, class TMatches, class TFix, class CTODO, class GSymbol>
    void update_heur_ctx_at_next(const std::array<std::size_t, N>& context, const TMatches& nterms, const std::vector<TFix>& pre, const std::vector<TFix>& post, const CTODO prefix, const CTODO postfix, const std::vector<GSymbol>& stack) {}

    template<class TSymbol>
    void update_heur_ctx_at_check(const TSymbol& match, bool accepted) {}

    template<std::size_t N, class TMatches, class TFix, class CTODO, class GSymbol, class TSymbol>
    void update_heur_ctx_at_apply(const std::array<std::size_t, N>& context, const TMatches& nterms, const std::vector<TFix>& pre, const std::vector<TFix>& post, const CTODO prefix, const CTODO postfix, const std::vector<GSymbol>& stack, const TSymbol& match) {}

    void set_empty_descend() {}

    template<class RRTree, class RulesDef>
    void init_windows(const RRTree& rr_tree, const RulesDef& rules) {}

    template<class TMatches, class TRules, class AllTerms, class NTermsPosPairs, class TermsPosPairs, class FixLimits>
    void init_ctx_classes(const TMatches& rules, const TRules& all_rr, const AllTerms& all_t, const NTermsPosPairs& nt_pairs, const TermsPosPairs& t_pairs, const FixLimits& limits) {}

    bool process() { return true; }

    void guru_meditation(const char* msg, const char* file, int line) {}

    void debug_message(auto build_widget, const char* file, int line) {}

    static void init_signal_handler() {}
};


#endif //SUPERCFG_PREPROCESS_H
