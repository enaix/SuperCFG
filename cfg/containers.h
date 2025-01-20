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


template<std::size_t SIZE>
class ConstStr
{
public:
    char str[SIZE];
    static constexpr std::size_t size() { return SIZE; };
    [[nodiscard]] const char* c_str() const { return str; }

    constexpr explicit ConstStr(const char (&c)[SIZE]) { std::copy_n(c, SIZE, str); }
    // https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/

    template<std::size_t N>
    static constexpr auto make(const char (&c)[N]) { return ConstStr<N>(c); } // Convenient wrapper for CStr constructor

    template<std::size_t N>
    constexpr auto concat(const ConstStr<N>& rhs)
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
    constexpr auto operator+(const ConstStr<N>& rhs) { return concat(rhs); }
};


#endif //SUPERCFG_CONTAINERS_H
