//
// Created by Flynn on 03.02.2025.
//

#ifndef SUPERCFG_STR_H
#define SUPERCFG_STR_H

#include <string>

#include "cfg/base.h"


template<class TChar>
class StdStr : public std::basic_string<TChar>
{
public:
    StdStr() = default;

    template<std::size_t N>
    explicit StdStr(const char (&a)[N]) : std::basic_string<TChar>(a, N) {}

    template<std::size_t N>
    explicit StdStr(const ConstStr<N>& c) : std::basic_string<TChar>(c.c_str(), N) {}

    /**
     * @brief Construct a new StdStr from a slice of a string
     * @param src String to slice
     * @param start Starting index of range
     * @param end The index of the string end (character after the last symbol in range)
     */
    static StdStr<TChar> from_slice(const StdStr<TChar>& src, std::size_t start, std::size_t end)
    {
        return StdStr<TChar>(src.c_str() + start, end - start);
    }
};


template<class TChar>
struct std::hash<StdStr<TChar>>
{
    std::size_t operator()(const StdStr<TChar>& s) const noexcept
    {
        return std::hash<std::basic_string<TChar>>{}(s);
    }
};

template<class Char, std::size_t BYTES, std::size_t CHUNK, std::size_t GROWTH>
class VarStr
{
public:
    Char str[BYTES];  // Statically allocate N bytes
    std::size_t N;
    Char* chunks;

    constexpr VarStr() : N(0), chunks(nullptr), str("\0") {}

    template<size_t SIZE>
    constexpr explicit VarStr(const char (&c)[SIZE]) : N(SIZE - 1)
    {
        if constexpr (SIZE > BYTES)
        {
            alloc_empty(N - 1); // -'\0'
            memcpy(str, c, BYTES); // +'\0'
            memcpy(chunks, c + BYTES, bytes_on_chunk(SIZE));
        } else {
            memcpy(str, c, SIZE);
            chunks = nullptr;
        }
        // Need to add an empty string chunk for quick chunk operations
    }

    template<size_t SIZE>
    constexpr explicit VarStr(const ConstStr<SIZE>& c) : N(SIZE - 1)
    {
        if constexpr (SIZE > BYTES)
        {
            alloc_empty(N - 1); // -'\0'
            memcpy(str, c.c_str(), BYTES); // +'\0'
            memcpy(chunks, c.c_str() + BYTES, bytes_on_chunk(SIZE));
        } else {
            memcpy(str, c.c_str(), SIZE);
            chunks = nullptr;
        }
        // ditto
    }

    //constexpr VarStr<Char, BYTES, CHUNK, GROWTH>& operator+=() {}

protected:
    void allocate(size_t n)
    {
        chunks = realloc(chunks, chunk_num()); // Allocate chunks
    }

    void alloc_empty(size_t n)
    {
        chunks = malloc(bytes_on_chunk(n));
    }

    // Adds n + '\0' symbol
    [[nodiscard]] constexpr std::size_t bytes_on_chunk(std::size_t n) const { return n - BYTES + 1; }

    [[nodiscard]] constexpr std::size_t chunk_part() const { return N - BYTES; }

    [[nodiscard]] constexpr std::size_t chunk_num()
    {
        // +'\0'
        if constexpr (GROWTH == 1) return (chunk_part() / CHUNK) + int(chunk_part() % CHUNK != 0);
        else
        {
            // G = 1, C = 8: {8, 16, 32, 48}
            // G = 2, C = 8: {8, 32, 64, 128} -> {1, 4, 8, 16}
            return ((chunk_part() / CHUNK) + int(chunk_part() % CHUNK != 0)) >> (GROWTH + 1);
        }
    }
    // TODO finish this class
};


#endif //SUPERCFG_STR_H
