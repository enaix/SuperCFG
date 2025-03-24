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
    constexpr explicit GrammarSymbol(const Token<VStr, Type>& token) : value(token.value), type(token.type), symbol_type(true_t()) {}

    // NTerm
    constexpr explicit GrammarSymbol(const Type& type) : value(), type(type), symbol_type(false_t()) {}

    // Copy constructor
    constexpr GrammarSymbol(const GrammarSymbol& rhs) : value(rhs.value), type(rhs.type), symbol_type(rhs.symbol_type) {}

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
};



#endif //SUPERCFG_PREPROCESS_H
