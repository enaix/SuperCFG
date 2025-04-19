//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_CONTAINERS_H
#define SUPERCFG_CONTAINERS_H

#include <cstddef>
#include <algorithm>
#include <array>
#include <tuple>
#include <cstring>
#include <limits>
#include <cstdint>
#include <memory>

#include "cfg/common.h"
#include "cfg/helpers.h"


// Not so optimized code for runtime even with -O3
template<std::size_t N>
constexpr bool equal(const char (&lhs)[N], const char (&rhs)[N])
{
    for (std::size_t i = 0; i < N; i++) { if (lhs[i] != rhs[i]) return false; }
    return true;
}

// constexpr std::size_t get_full_chunks(std::size_t chunk_bytes, std::size_t N) { return (N / chunk_bytes) + int(N % chunk_bytes != 0); }

constexpr std::size_t digits(std::size_t N)
{
    if (N / 10 == 0) return 1;
    else return 1 + digits(N / 10);
}

template<std::size_t N>
constexpr std::size_t digits()
{
    if (N / 10 == 0) return 1;
    else return 1 + digits<N / 10>();
}


template<std::size_t N, std::size_t M>
constexpr void concat_str(const char (&lhs)[N], const char (&rhs)[M], const char* dest)
{
    std::copy_n(lhs, N - 1, dest);
    std::copy_n(rhs, M, dest + N - 1);
}


template<class T, T arg>
struct TemplateArgWrapper {};


template<std::size_t SIZE>
class ConstStrContainer
{
public:
    using underlying_t = char[SIZE];
    using char_t = char;
    // TODO add templated char type

    char str[SIZE];

    constexpr ConstStrContainer() : str {} {}

    constexpr ConstStrContainer(const char (&c)[SIZE]) { std::copy_n(c, SIZE, str); }

    constexpr explicit ConstStrContainer(const char *c) { std::copy_n(c, SIZE, str); }

    template<std::size_t N, std::size_t M>
    constexpr explicit ConstStrContainer(const char (&lhs)[N], const char (&rhs)[M])
    {
        static_assert(N+M-1 == SIZE, "Sizes do not match");
        std::copy_n(lhs, N - 1, str);
        std::copy_n(rhs, M, str + N - 1);
    }

    template<std::size_t N, ConstStrContainer<N> src, std::size_t START, std::size_t LEN>
    constexpr explicit ConstStrContainer(const TemplateArgWrapper<ConstStrContainer<N>, src>, const std::integral_constant<std::size_t, START>, const std::integral_constant<std::size_t, LEN>)
    {
        static_assert(START+LEN <= N, "Substring cannot be larger than a source string");

        std::copy_n(src.str + START, LEN, str);
        /*if constexpr (SIZE > LEN)
        {
            constexpr char zeros[SIZE - LEN] = ""; // zero-init
            std::copy_n(zeros, SIZE - LEN, str + START + LEN);
        }*/
        str[SIZE-1] = '\0';
    }

    template<class... TArg>
    constexpr explicit ConstStrContainer(const std::tuple<TArg...>& c)
    {
        tuple_each(c, [&](std::size_t i, const auto& e){ str[i] = e; });
    }

    [[nodiscard]] constexpr const char* c_str() const { return str; }

    static constexpr std::size_t size() { return SIZE; }

    template<std::size_t i>
    [[nodiscard]] constexpr char at() const { return str[i]; }
};


template<ConstStrContainer STR>
struct ConstStrWrapper
{
    static constexpr auto value() { return STR; }
};


namespace cfg_helpers
{
    template<ConstStrContainer STR, size_t START, size_t LEN>
    constexpr auto make_slice(auto builder)
    {
        constexpr std::integral_constant<std::size_t, START> start;
        constexpr std::integral_constant<std::size_t, LEN> len;
        constexpr ConstStrContainer<LEN+1> container(TemplateArgWrapper<decltype(STR), STR>{}, start, len);
        return builder.template operator()<decltype(container), container>();
    }

    template<std::size_t i, class TChar, TChar start, TChar end>
    constexpr void do_lexical_range(auto func)
    {
        func(static_cast<TChar>(static_cast<std::size_t>(start) + i));
        if constexpr (i + 1 <= static_cast<std::size_t>(end))
            return do_lexical_range<i+1, TChar, start, end>(func);
    }
}


 /**
  * @brief Constexpr string class. Not advised to use in runtime
  * @tparam STR Deduced string container, including '\0'
  */
template<ConstStrContainer STR>
class ConstStr
{
public:
    static constexpr std::size_t size() { return STR.size(); }
    [[nodiscard]] constexpr const char* c_str() const { return STR.c_str(); }

    using char_t = typename decltype(STR)::char_t;
    constexpr explicit ConstStr() = default;

    //template<std::size_t SIZE>
    //constexpr explicit ConstStr(const char (&c)[SIZE]) { }
    // https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/

    // constexpr explicit ConstStr(std::size_t num) : str(itoc(num)) {}

    constexpr auto container() const { return STR; }

    template<ConstStrContainer C>
    static constexpr auto make() { return ConstStr<C.str>(); } // Convenient wrapper for CStr constructor

    template<ConstStrContainer RHS>
    constexpr auto concat(const ConstStr<RHS>& rhs) const
    {
        constexpr std::size_t new_size = size() + ConstStr<RHS>::size() - 1; // Account for '\0'
        constexpr ConstStrContainer<new_size> c(STR.str, RHS.str);
        return ConstStr<c>();
    }

    template<std::size_t START, std::size_t LEN>
    static constexpr auto slice()
    {
        static_assert(START + LEN <= size(), "Substring cannot be larger than a source string");

        return cfg_helpers::make_slice<STR, START, LEN>([&]<class T, T Cnt>(){ return ConstStr<Cnt>(); });
    }

    template<std::size_t i>
    static constexpr auto at() { return STR.template at<i>(); }

    template<ConstStrContainer RHS>
    constexpr bool operator==(const ConstStr<RHS>& rhs) const
    {
        static_assert(ConstStr<RHS>::size() == size(), "Cannot compare strings with different length");
        return equal<size()>(STR.str, rhs.container().str);
    }

    // Overloads
    template<ConstStrContainer RHS>
    constexpr auto operator+(const ConstStr<RHS>& rhs) const { return concat(rhs); }

    template<class... TArgs>
    constexpr const auto& bake(auto... args) const { return *this; } // Baking operation on a string always returns string

    //template<ConstStrContainer LHS>
    friend std::ostream& operator<<(std::ostream& os, const ConstStr<STR>& lhs) { return os << lhs.c_str(); }
};

template<ConstStrContainer STR>
[[nodiscard]] constexpr auto cs() { return ConstStr<STR.str>(); }

template<class CStr, ConstStrContainer STR>
[[nodiscard]] constexpr auto cs() { return CStr::template make<STR>(); }

 /**
  * @brief Int to ConstStr (itoa)
  * @tparam N Num to convert
  */
template<std::size_t N>
constexpr auto itoc()
{
    constexpr std::size_t D = digits<N>();
    //char s[D + 1]; // store N digits + '\0'
    std::array<char, D+1> s;
    for (std::size_t d = 1, i = 0; i < D; d *= 10, i++)
    {
        auto digit = (std::int8_t)(N % (d * 10) / d);
        s[i] = digit + '0';
    }
    s[D] = '\0';
    //constexpr auto c = ConstStrContainer(s);
    //return ConstStr<c>();
    return to_homogeneous_tuple(s);
}

 /**
  * @brief std::integral_constant to ConstStr
  */
template<std::size_t N>
[[nodiscard]] constexpr auto int2str(const std::integral_constant<std::size_t, N> n)
{
    constexpr auto chars = itoc<N>();
    constexpr auto c = ConstStrContainer<std::tuple_size_v<decltype(chars)>>(chars);
    return ConstStr<c>();
}


template<class TChar, TChar start, TChar end>
constexpr void lexical_range(auto func)
{
    return cfg_helpers::do_lexical_range<0, TChar, start, end>(func);
}

template<class TChar>
constexpr bool in_lexical_range(TChar c, TChar start, TChar end)
{
    return (start <= c) && (c <= end);
}


 /**
  * @brief Map enum to its index in increasing order
  * @tparam Max Maximum (last) value of enum
  * @tparam TArg List of enum arguments of the same type
  */
template<class T, T Max>
class EnumMap
{
public:
    std::size_t storage[static_cast<std::size_t>(Max) + 1];

    template<class... TArg>
    constexpr explicit EnumMap(bool increasing, const TArg&... args) : storage { std::numeric_limits<std::size_t>::max() }
    {
        if (increasing) init<0>(increasing, args...);
        else init<sizeof...(TArg) - 1>(increasing, args...);
    }

    constexpr bool has(T arg) const { return get(arg) != std::numeric_limits<std::size_t>::max(); }

    constexpr size_t get(T arg) const { return storage[static_cast<std::size_t>(arg)]; }

    constexpr T max(T lhs, T rhs) const { return (get(lhs) > get(rhs) ? lhs : rhs); }

    constexpr bool less(T lhs, T rhs) const { return get(lhs) < get(rhs); }

protected:
    template<std::size_t i, class Arg, class... Args>
    constexpr void init(bool increasing, const Arg& arg, const Args&... args)
    {
        storage[static_cast<std::size_t>(arg)] = i;
        if (increasing) init<i+1>(increasing, args...);
        else init<i-1>(increasing, args...);
    }

    template<std::size_t i, class Arg>
    constexpr void init(bool increasing, const Arg& arg)
    {
        storage[static_cast<std::size_t>(arg)] = i;
    }
};


/**
 * @brief Constant-size vector with lazy init
 * @tparam T Element type
 */
template<class T>
class ConstVec
{
protected:
    std::unique_ptr<T[]> _st;
    std::size_t _n{};
    std::size_t _cap{};

public:
    // Lazy init
    constexpr ConstVec() : _st(), _n(0), _cap(0) {}

    // Copy ctor
    constexpr ConstVec(const ConstVec<T>& rhs) : _st(new T[rhs.size()]), _n(rhs.size()), _cap(rhs.size()) { deepcopy(rhs); }

    // Initialize a singleton
    constexpr explicit ConstVec(const T& elem) : _st(new T[1]), _n(1), _cap(1) { _st[0] = elem; }

    // Initialize from values
    //constexpr explicit ConstVec(const T...& elems) : _st(std::make_unique<T>(elems...)), _n(sizeof...(T)), _cap(sizeof...(T)) {}

    // Initialize from homogeneous tuple
    template<class... Elems>
    constexpr explicit ConstVec(const std::tuple<Elems...>& elems) : _st(init_from_tuple(elems, std::make_index_sequence<sizeof...(Elems)>{})), _n(sizeof...(Elems)), _cap(sizeof...(Elems))
    { static_assert(are_same_v<T, Elems...>, "Element types are non-homogeneous"); }


    /**
     * @brief Initialize from singleton
     * @param elem Element to insert
     */
    void init(const T& elem)
    {
        _st.reset(new T[1]);
        _st[0] = elem;
        _cap = 1;
        _n = 1;
    }

    /**
     * @brief Initialize from a tuple of size N
     * @tparam Elems Deduced element types
     * @param elems Tuple to initialize from
     */
    template<class... Elems>
    void init(const std::tuple<Elems...>& elems)
    {
        static_assert(are_same_v<T, Elems...>, "Element types are non-homogeneous");
        _st.reset(new T[sizeof...(Elems)]);
        _cap = sizeof...(Elems);
        _n = sizeof...(Elems);

        tuple_each(elems, [&](std::size_t i, const auto& elem){ _st[i] = elem; }); // Sub-optimal
    }

    void init(const ConstVec<T>& rhs)
    {
        _st.reset(new T[rhs.size()]);
        deepcopy(rhs);
        _cap = rhs.size();
        _n = rhs.size();
    }

    [[nodiscard]] std::size_t size() const noexcept { return _n; }

    [[nodiscard]] std::size_t cap() const noexcept { return _cap; }

    T& operator[](std::size_t i) { return _st[i]; }

    constexpr const T& operator[](std::size_t i) const { return _st[i]; }

    /**
     * @brief Replace array with a single element
     * @param elem
     */
    void replace_with(const T& elem)
    {
        _n = 1;
        _st[0] = elem;
    }

    /**
     * @brief Manually set the size of the array. Doesn't perform any checks on capacity!
     * @param n New array size
     */
    void set_size(std::size_t n) { _n = n; }

    void erase() { _n = 0; }

    ConstVec<T>& operator=(const ConstVec& rhs)
    {
        init(rhs);
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const ConstVec<T>& lhs)
    {
        for (std::size_t i = 0; i < lhs.size(); i++)
            os << lhs[i] << " ";
        return os;
    }

protected:
    template<class SrcTuple, std::size_t... N>
    constexpr auto init_from_tuple(const SrcTuple& src, const std::index_sequence<N...>)
    {
        // return std::make_unique<T>(T(std::get<N>(src))...);
        return new T[std::tuple_size_v<std::decay_t<SrcTuple>>]{ T(std::get<N>(src))... };
    }

    void deepcopy(const ConstVec<T>& rhs)
    {
        // Perform a deepcopy of the rhs array up to rhs.size elements
        std::copy_n(rhs._st.get(), rhs.size(), _st.get());
    }
};



#endif //SUPERCFG_CONTAINERS_H
