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
        {
            return std::get<depth>(terms);
        }
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

    constexpr explicit TermsTypeMap(const TermsTuple& defs_t, const TermsDefsTuple& terms_t) : terms(defs_t), nterms(terms_t)
    {
        populate_ht<0>();
    }

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
    constexpr void populate_ht()
    {
        const auto& symbol = std::get<i>(terms);
        const auto value = TypeSet<TokenType>(tuple_morph([]<std::size_t k>(const auto& src){ return VStr(std::get<k>(src).type()); }, std::get<i>(nterms)));

        if constexpr (is_term<decltype(symbol)>())
            storage.insert({VStr(std::get<i>(terms).type()), value});
        else
        {
            // Terms range
            symbol.each_range([&](const auto s){ storage.insert({VStr(s), value}); });
        }
        if constexpr (i + 1 < std::tuple_size_v<std::decay_t<TermsTuple>>)
            populate_ht<i+1>();
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
protected:
    TermsTMap terms_map;

public:
    using TokenSetClass = TypeSet<TokenType>;

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
        return tokens;
    }

    void prettyprint() const { do_print<0>(); }
protected:
    template<std::size_t depth>
    void do_print() const
    {
        const auto& term = std::get<depth>(terms_map.terms);
        const auto& types = std::get<depth>(terms_map.nterms);
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
            terms_map.insert(TokenType(term.type()), term);
        });

        tuple_each(nterms, [&](std::size_t i, const auto& nterm){
            nterms_map.insert(TokenType(nterm.type()), nterm);
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



#endif //SUPERCFG_PREPROCESS_H
