//
// Created by Flynn on 12.03.2025.
//

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <tuple>
#include <unordered_map>
#include <variant>

#include "cfg/helpers.h"


template<class Key, class ValuesTuple>
class TypesHashTable
{
public:
    using TypesNum = std::tuple_size<ValuesTuple>;

    // Morph values into std::variant type
    using ValuesVariant = decltype(type_morph_t<std::variant>(
            []<std::size_t index>(){ return std::tuple_element_t<index, ValuesTuple>(); },
            TypesNum()));

    std::unordered_map<Key, ValuesVariant> storage;

    TypesHashTable(auto fill_storage, const std::size_t N)
    {
        for (std::size_t i = 0; i < N; i++)
        {
            // Initialize storage
            fill_storage(i, storage);
        }
    }

    template<std::size_t N>
    constexpr explicit TypesHashTable(auto fill_storage, const IntegralWrapper<N> nums)
    {
        init(fill_storage, std::make_index_sequence<N>{});
    }

    auto get(const Key& key, auto func) const
    {
        return std::visit([func](const auto& elem){ return func(elem); }, storage[key]);
    }

protected:
    template<std::size_t... Ints>
    constexpr void init(auto fill_storage, const std::integer_sequence<std::size_t, Ints...>)
    {
        // Call fill_storage N times
        (fill_storage.template operator()<Ints>(storage),...);
    }
};


#endif //HASHTABLE_H
