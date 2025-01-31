//
// Created by Flynn on 20.01.2025.
//

#ifndef SUPERCFG_CONTAINERS_H
#define SUPERCFG_CONTAINERS_H

#include <cstddef>
#include <algorithm>
#include <cstring>


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


 /**
  * @brief Constexpr string class. Not advised to use in runtime
  * @tparam SIZE Deduced string length, including '\0'
  */
template<std::size_t SIZE>
class ConstStr
{
public:
    char str[SIZE];
    static constexpr std::size_t size() { return SIZE; };
    [[nodiscard]] const char* c_str() const { return str; }

    constexpr explicit ConstStr(const char (&c)[SIZE]) { std::copy_n(c, SIZE, str); }
    // https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/

    // constexpr explicit ConstStr(std::size_t num) : str(itoc(num)) {}

    template<std::size_t N>
    static constexpr auto make(const char (&c)[N]) { return ConstStr<N>(c); } // Convenient wrapper for CStr constructor

    template<std::size_t N>
    constexpr auto concat(const ConstStr<N>& rhs) const
    {
        constexpr std::size_t new_size = SIZE + N - 1; // Account for '\0'
        char c[new_size];
        std::copy_n(this->str, this->size() - 1, c);
        std::copy_n(rhs.str, rhs.size(), c + this->size() - 1);
        return ConstStr<new_size>::make(c);
    }

    template<std::size_t N>
    constexpr bool operator==(const ConstStr<N>& rhs) const
    {
        static_assert(N == SIZE, "Cannot compare strings with different length");
        return equal(this->str, rhs.str);
    }

    // Overloads
    template<std::size_t N>
    constexpr auto operator+(const ConstStr<N>& rhs) const { return concat(rhs); }

    template<class... TArgs>
    constexpr const ConstStr<SIZE>& bake(auto... args) const { return *this; } // Baking operation on a string always returns string

    //template<class... Args>
};

template<std::size_t SIZE>
[[nodiscard]] constexpr auto cs(const char (&c)[SIZE]) { return ConstStr(c); }

 /**
  * @brief Int to ConstStr (itoa)
  * @tparam N Num to convert
  */
template<std::size_t N>
constexpr auto itoc()
{
    constexpr std::size_t D = digits<N>();
    char s[D + 1]; // store N digits + '\0'
    for (std::size_t d = 1, i = 0; i < D; d *= 10, i++)
    {
        auto digit = (std::int8_t)(N % (d * 10) / d);
        s[i] = digit + '0';
    }
    s[D] = '\0';
    return ConstStr(s);
}

 /**
  * @brief std::integral_constant to ConstStr
  */
template<class integral_const>
[[nodiscard]] constexpr auto int2str(const integral_const NUM) { return itoc<integral_const::value>(); }


 /**
  * integral_constant<size_t, ...> wrapper which converts N to ConstStr
  * @tparam N
  */
template<std::size_t N>
class IntegralWrapper : public std::integral_constant<size_t, N>
{
public:
    template<class... TArgs>
    constexpr auto bake(auto... args) const { return int2str(*this); }
};


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
    constexpr explicit EnumMap(bool increasing, const TArg&... args) : storage { SIZE_T_MAX }
    {
        if (increasing) init<0>(increasing, args...);
        else init<sizeof...(TArg) - 1>(increasing, args...);
    }

    constexpr bool has(T arg) const { return get(arg) != SIZE_T_MAX; }

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



#endif //SUPERCFG_CONTAINERS_H
