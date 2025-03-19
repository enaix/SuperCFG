//
// Created by Flynn on 12.03.2025.
//

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <tuple>
#include <unordered_map>
#include <variant>

#include "cfg/helpers.h"
#include "elemtree/element.h"


template<class Key, class ValuesTuple>
class TypesHashTable
{
public:
    using TypesNum = std::tuple_size<ValuesTuple>;

    // Morph values into std::variant type
    using ValuesVariant = variadic_morph_t<ValuesTuple>;

    std::unordered_map<Key, ValuesVariant> storage;

    TypesHashTable(auto fill_storage, const std::size_t N)
    {
        for (std::size_t i = 0; i < N; i++)
        {
            // Initialize storage
            fill_storage(i, storage);
        }
    }

    constexpr TypesHashTable() = default;

    auto get(const Key& key, auto func) const
    {
        return std::visit([func](const auto& elem){ return func(elem); }, storage[key]);
    }

    template<class Val>
    void insert(const Key& key, const Val& value) const
    {
        static_assert(tuple_contains_v<Val, ValuesTuple>, "Tuple does not contain such type");
        Val v = value;
        storage.insert({key, ValuesVariant(v)});
    }
};


#endif //HASHTABLE_H
