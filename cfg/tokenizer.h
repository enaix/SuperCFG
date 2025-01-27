//
// Created by Flynn on 27.01.2025.
//

#ifndef SUPERCFG_TOKENIZER_H
#define SUPERCFG_TOKENIZER_H

#include <vector>
#include <cstdint>
#include <cassert>
#include <unordered_map>

#include "cfg/base.h"


template<class VStr, class TokenType>
class Token
{
public:
    VStr value;
    TokenType type;

    constexpr Token(const VStr& v, const TokenType& t) : value(v), type(t) {}

    constexpr friend Token<VStr, TokenType> operator+ (const Token<VStr, TokenType>& lhs, const Token<VStr, TokenType>& rhs)
    {
        // We assume that type is identical
        return Token<VStr, TokenType>(lhs.value + rhs.value, lhs.type);
    }

    Token<VStr, TokenType>& operator+= (const Token<VStr, TokenType>& rhs) { value += rhs.value; return *this; }
};

template<std::size_t TERMS_MAX, class VStr, class TokenType>
class TermsStorage
{
public:
    Token<VStr, TokenType> storage[TERMS_MAX];
    std::size_t N;

    using hashtable = std::unordered_map<VStr, TokenType, typename VStr::hash>;

    template<class RulesSymbol>
    constexpr explicit TermsStorage(const RulesSymbol& rules_def)
    {
        N = iterate(rules_def, 0);
    }

protected:
    template<class TSymbol>
    constexpr std::size_t iterate(const TSymbol& s, std::size_t ind)
    {
        s.each([&](const auto& symbol){
            if constexpr (is_operator(symbol))
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
            if constexpr (is_operator(symbol))
            {
                ind = iterate(symbol, def, ind); // Will always return bigger index
            } else {
                if constexpr (is_term(symbol))
                {
                    // Found a terminal symbol
                    storage[ind] = Token<VStr, TokenType>(VStr(symbol.name), TokenType(def));
                    assert(ind + 1 < TERMS_MAX && "Maximum terminals limit reached");
                    ind++;
                }
            }
        });
        return ind; // Maximum index at this iteration
    }

    [[nodiscard]] constexpr bool validate() const
    {
        // Naive implementation in O*log(n)
        for (std::size_t i = 0; i < N; i++)
        {
            for (std::size_t j = i + 1; j < N; j++)
            {
                if (VStr::is_substr(storage[i].value == storage[j].value)) return false;
            }
        }
        return true;
    }

    hashtable compile_hashmap() const
    {
        // TODO create optimal implementation for the hash function
        hashtable map;
        for (std::size_t i = 0; i < N; i++)
        {
            map.insert(storage[i].value, storage[i].type);
        }
        return map;
    }
};


template<std::size_t TERMS_MAX, class VStr, class TokenType>
class Tokenizer
{
protected:
    TermsStorage<TERMS_MAX, VStr, TokenType> storage;
    using hashtable = TermsStorage<TERMS_MAX, VStr, TokenType>::hashtable;

public:
    template<class RulesSymbol>
    constexpr explicit Tokenizer(const RulesSymbol& root) : storage(root) { static_assert(storage.validate(), "Duplicate terminals found, cannot build tokens storage"); }

    hashtable init_hashtable() { return storage.compile_hashmap(); }

    template<class VText>
    std::vector<Token<VStr, TokenType>> parse(const hashtable& ht, const VText& text) const
    {
        std::vector<Token<VStr, TokenType>> tokens;
        std::size_t pos = 0;
        for (std::size_t i = 0; i + 1 < text.size(); i++)
        {
            VStr tok = VStr::from_slice(text, pos, i + 1);
            const auto it = ht.find(tok);

            if (it != ht.end())
            {
                // Terminal found
                std::size_t n = tokens.size();
                if (n > 0 && tokens[n - 1].type == it->second) tokens[n - 1] += it->first;
                else tokens.push_back(Token<VStr, TokenType>(it->first, it->second));
                pos = i + 1;
            }
        }

        assert(pos == text.size() && "Tokenization error: found unrecognized tokens");
        return tokens;
    }
};


#endif //SUPERCFG_TOKENIZER_H
