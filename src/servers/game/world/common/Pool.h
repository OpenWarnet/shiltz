#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

// Keyed collection with O(1) lookup by key and dense, cache-friendly
// iteration over the values. Values live contiguously in a vector; a side
// index maps each key to its slot. Remove swaps the last value into the
// freed slot and pops, so iteration order is not stable.
//
// Pointers, references, and iterators into the pool are invalidated by Add
// (the vector may reallocate) and by Remove (a value may be moved into the
// freed slot) -- hold keys across those, not pointers. Don't Add or Remove
// while iterating.
template <typename Key, typename T, typename Hash = std::hash<Key>> class Pool
{
public:
    // False (and value is dropped) if key is already present.
    bool Add(const Key& key, T value)
    {
        if (m_index.contains(key))
            return false;

        m_index.emplace(key, m_values.size());
        m_keys.push_back(key);
        m_values.push_back(std::move(value));
        return true;
    }

    // Returns the removed value so the caller can move it elsewhere (e.g.
    // to another map's pool), or nullopt if key isn't present.
    std::optional<T> Remove(const Key& key)
    {
        auto it = m_index.find(key);
        if (it == m_index.end())
            return std::nullopt;

        const std::size_t slot = it->second;
        const std::size_t last = m_values.size() - 1;

        std::optional<T> removed(std::move(m_values[slot]));
        if (slot != last)
        {
            m_values[slot] = std::move(m_values[last]);
            m_keys[slot] = std::move(m_keys[last]);
            m_index.find(m_keys[slot])->second = slot;
        }

        m_values.pop_back();
        m_keys.pop_back();
        m_index.erase(it);
        return removed;
    }

    T* Get(const Key& key)
    {
        auto it = m_index.find(key);
        return it == m_index.end() ? nullptr : &m_values[it->second];
    }

    const T* Get(const Key& key) const
    {
        auto it = m_index.find(key);
        return it == m_index.end() ? nullptr : &m_values[it->second];
    }

    bool Contains(const Key& key) const { return m_index.contains(key); }

    std::size_t Size() const noexcept { return m_values.size(); }
    bool Empty() const noexcept { return m_values.empty(); }

    auto begin() noexcept { return m_values.begin(); }
    auto end() noexcept { return m_values.end(); }
    auto begin() const noexcept { return m_values.begin(); }
    auto end() const noexcept { return m_values.end(); }

private:
    std::vector<T> m_values;
    std::vector<Key> m_keys;
    std::unordered_map<Key, std::size_t, Hash> m_index;
};
