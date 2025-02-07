//
// Created by Flynn on 27.01.2025.
//

#ifndef SUPERCFG_PARSER_H
#define SUPERCFG_PARSER_H

#include <vector>
#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <queue>
#include <typeindex>
#include <variant>

#include "cfg/base.h"
#include "cfg/helpers.h"


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
    using TDefsTuple = RulesSymbol::term_ptr_tuple;
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


 /**
  * @brief Constant NTerm -> definition mapping
  * @tparam TokenType
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
    using hashtable = TermsStorage<TERMS_MAX, VStr, TokenType>::hashtable;

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
                if (n > 0 && tokens[n - 1].type == it->second) tokens[n - 1].value += it->first;
                else tokens.push_back(Token<VStr, TokenType>(it->first, it->second));
                pos = i + 1;
            }
        }

//        assert(pos == text.size() && "Tokenization error: found unrecognized tokens");
        ok = (pos == text.size());
        return tokens;
    }
};


 /**
  * @brief Tokens parser class
  * @tparam VStr Variable string class
  * @tparam TokenType Nonterminal type (name) container
  * @tparam RulesSymbol Rules class
  * @tparam Tree Parser tree node class
  */
template<class VStr, class TokenType, class Tree, class RulesSymbol>
class Parser
{
protected:
    //NTermsStorage<TokenType, RulesSymbol> storage;
    NTermsConstHashTable<RulesSymbol> storage;
    using TokenV = Token<VStr, TokenType>;

public:
    constexpr explicit Parser(const RulesSymbol& rules) : storage(rules) {}

    /**
     * @brief Recursively parse tokens and build parse tree
     * @param node Empty tree to populate
     * @param root Rules starting point
     * @param tokens List of tokens from Tokenizer
     */
    template<class RootSymbol>
    bool run(Tree& node, const RootSymbol& root, const std::vector<TokenV>& tokens) const
    {
        std::size_t index = 0;
        return parse(root, node, index, tokens);
    }

protected:
    template<class TSymbol>
    bool parse(const TSymbol& symbol, Tree& node, std::size_t& index, const std::vector<TokenV>& tokens) const
    {
        // Iterate over each operator
        if constexpr (is_operator<TSymbol>())
        {
            if constexpr (get_operator<TSymbol>() == OpType::Concat)
            {
                std::size_t index_stack = index;
                // Iterate over each node, index is moved each time. No need to copy node on stack
                bool found = true;
                symbol.each([&](const auto& s){
                    if (!parse(s, node, index, tokens)) { index = index_stack; found = false; }
                });
                return found;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Alter)
            {
                // Check any of these nodes
                std::size_t i = index;
                bool found = false;
                symbol.each([&](const auto& s){
                    // Apply index and return
                    Tree node_stack = node;
                    if (parse(s, node_stack, i, tokens))
                    {
                        node = node_stack; // Apply changes
                        index = i;
                        found = true;
                    }
                });

                return found;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Optional)
            {
                // Try to match if we can, just return true anyway
                Tree node_stack = node;
                if (parse(std::get<0>(symbol.terms), node_stack, index, tokens)) node = node_stack;
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Repeat)
            {
                Tree node_stack = node;
                while (parse(std::get<0>(symbol.terms), node_stack, index, tokens))
                {
                    // Update stack when we succeed
                    node = node_stack;
                }
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Group)
            {
                return parse(std::get<0>(symbol.terms), node, index, tokens);
            }
            else if constexpr (get_operator<TSymbol>() == OpType::Except)
            {
                std::size_t i = index;
                Tree node_stack = node;
                if (parse(std::get<0>(symbol.terms), node_stack, index, tokens))
                {
                    // Check if the symbol is an exception
                    if (!parse(std::get<1>(symbol.terms), Tree(), i, tokens))
                    {
                        node = node_stack;
                        return true;
                    }
                }
                return false;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatExact)
            {
                std::size_t index_stack = index;
                for (std::size_t i = 0; i < get_repeat_times(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) { index = index_stack; return false; }
                }
                // We don't need to check the next operator, since it may start from this token
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatGE)
            {
                std::size_t index_stack = index;
                for (std::size_t i = 0; i < get_repeat_times(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) { index = index_stack; return false; }
                }
                Tree node_stack = node;
                while (parse(std::get<0>(symbol.terms), node_stack, index, tokens))
                {
                    // Update stack when we succeed
                    node = node_stack;
                }
                return true;
            }
            else if constexpr (get_operator<TSymbol>() == OpType::RepeatRange)
            {
                std::size_t index_stack = index;
                for (std::size_t i = 0; i < get_range_from(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) { index = index_stack; return false; }
                }
                Tree node_stack = node;
                for (std::size_t i = get_range_from(symbol); i < get_range_to(symbol); i++)
                {
                    if (!parse(std::get<0>(symbol.terms), node, index, tokens)) return true;
                    else node = node_stack; // Update symbol
                }
                // We don't need to check the next operator, since it may start from this token
                return true;
            }
            else
            {
                // Implement RepeatExact, GE, Range and handle exceptions
                static_assert(get_operator<TSymbol>() == OpType::Comment || get_operator<TSymbol>() == OpType::SpecialSeq, "Wrong operator type");
            }

        } else if constexpr (is_nterm<TSymbol>()) {
            // Get definition and get rules for the non-terminal
            const auto& s = std::get<1>(storage.get(symbol)->terms);
            // Create and pass a new leaf node
            return parse(s, symbol.template create_node<Tree>(node), index, tokens);

        } else if constexpr (is_term<TSymbol>()) {
            if (index >= tokens.size()) [[unlikely]] abort();

            if (tokens[index].value == symbol.name)
            {
                index++; // Move the pointer only if we succeed
                return true;
            }
            return false;
        } else static_assert(is_term<TSymbol>() || is_nterm<TSymbol>() || is_operator<TSymbol>(), "Wrong symbol type");

        return true;
    }
};

// Unneeded classes
// ================

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
    using TDefsTuple = RulesSymbol::term_types_tuple;
    using TDefsPtrTuple = RulesSymbol::term_ptr_tuple;
    using TupleLen = IntegralWrapper<std::tuple_size_v<TDefsTuple>>();

    // Morph tuple into std::variant type
    using NTermVariant = decltype(type_morph_t<std::variant>(
            []<std::size_t index>(){ return NTermHTItem<std::tuple_element_t<index, TDefsTuple>()>(); },
            TupleLen()));

    std::unordered_map<TokenType, NTermVariant> storage;

    constexpr NTermsHashTable(const RulesSymbol& rules)
    {
        std::size_t i = 0;
        // Iterate over each definition
        rules.each([&](const auto& def){
            NTermHTItem term(def);
            TokenType t(std::get<0>(def.terms).type());
            storage.insert(t, term);
        });
    }

    auto get(const TokenType& type) const
    {
        return std::visit([](const auto& ht_item){ return ht_item.ptr; }, storage[type]);
    }
};



#endif //SUPERCFG_PARSER_H
