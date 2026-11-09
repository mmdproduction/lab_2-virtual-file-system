#pragma once

#include <cstddef>

template <typename TKey, typename TValue>
class IDictionary {
public:
    virtual ~IDictionary() = default;

    virtual std::size_t size() const = 0;

    virtual bool contains_key(const TKey& key) const = 0;

    virtual TValue& get(const TKey& key) = 0;
    virtual const TValue& get(const TKey& key) const = 0;

    virtual void add(const TKey& key, const TValue& value) = 0;
    virtual void remove(const TKey& key) = 0;

    virtual void clear() = 0;
};