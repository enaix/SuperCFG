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
class BaseNTerm
{
public:
    CStr name;
    constexpr explicit BaseNTerm(const CStr& n) : name(n) {}

    constexpr BaseNTerm() : name("\0") {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return rules.bake_nonterminal(this->name); }
};

template<class CStr>
class BaseTerm
{
public:
    CStr name;
    constexpr explicit BaseTerm(const CStr& n) : name(n) {}

    constexpr BaseTerm() : name("\0") {}

    template<class BNFRules>
    constexpr auto bake(const BNFRules& rules) const { return rules.bake_terminal(this->name); }
};


 /**
  * @brief Base operator with 2 arguments
  * @tparam NTerm Base nonterminal class
  */
template<class NTerm>
class BaseBinaryOP
{
public:
    NTerm left;
    NTerm right;
    constexpr BaseBinaryOP(const NTerm& l, const NTerm& r) : left(l), right(r) {}
};

 /**
  * @brief Base operator with multiple args, fixed-size implementation
  * @tparam NTerm Base nonterminal class
  * @tparam N Deduced number of arguments
  */
template<class NTerm>
class BaseMultiOPBuf
{
public:
    static constexpr std::size_t NTermsMax = 16;
    NTerm terms[NTermsMax];
    std::size_t N;

    template<class... NTerms>
    constexpr explicit BaseMultiOPBuf(const NTerms&... t) : N(sizeof...(t))
    {
        init(0, t...);
    }

    //constexpr explicit BaseMultiOP(BaseNTerm t) {}

protected:
    template<class... NTerms>
    constexpr void init(const std::size_t i, const NTerm& term, const NTerms&... t)
    {
        terms[i] = term;
        init(i+1, t...);
    }

    constexpr void init(const std::size_t i, const NTerm& term)
    {
        terms[i] = term;
        for (std::size_t j = i+1; j < NTermsMax; j++) { terms[j] = NTerm(); }
    }
};


/**
 * @brief Base operator with multiple args
 * @tparam NTerm Base nonterminal class
 * @tparam N Deduced number of arguments
 */
template<class... NTerms>
class BaseMultiOP
{
public:
    std::tuple<NTerms...> terms;

    constexpr explicit BaseMultiOP(const NTerms&... t) : terms(t...) {}

    constexpr void each_elem(auto& func)
    {
        for (size_t i = 0; i < sizeof...(NTerms); i++)
        {
            func(std::get<i>(terms));
        }
    }

    //constexpr explicit BaseMultiOP(BaseNTerm t) {}
};



 /**
  * @brief Base ending (termination) operator
  */
class BaseEndOP
{

};

constexpr void bake()
{
//    constexpr BaseNTerm<char> a('a'), b('b');
    constexpr auto a("abc"), b("abcd");
    //constexpr auto op = BaseMultiOP<BaseNTerm<char>>(a, b);
    constexpr auto op = BaseMultiOP(a, b);
}

//template<template<class> class NTerm, class CStr>
//constexpr auto make_term(const CStr& name) => NTerm<CStr>()


#endif //SUPERCFG_BASE_H
