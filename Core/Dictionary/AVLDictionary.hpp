#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>

#include "AVLTree.h"
#include "IDictionary.hpp"

template <typename TKey, typename TValue,
          class Compare = std::less<TKey>>
class AvlDictionary : public IDictionary<TKey, TValue> {
    struct Entry {
        TKey key;
        TValue value;
    };

    struct EntryCompare {
        Compare compare;

        bool operator()(const Entry& first,
                        const Entry& second) const {
            return compare(first.key, second.key);
        }
    };

public:
    AvlDictionary() = default;

    explicit AvlDictionary(const Compare& compare)
        : tree_(EntryCompare{compare}) {}

    ~AvlDictionary() override = default;

    std::size_t size() const noexcept override {
        return tree_.size();
    }

    bool contains_key(const TKey& key) const override {
        return tree_.contains(Entry{key, TValue{}});
    }

    TValue& get(const TKey& key) override {
        Entry* entry = tree_.find(Entry{key, TValue{}});

        if (!entry) {
            throw std::out_of_range("key not found");
        }

        return entry->value;
    }

    const TValue& get(const TKey& key) const override {
        const Entry* entry = tree_.find(Entry{key, TValue{}});

        if (!entry) {
            throw std::out_of_range("key not found");
        }

        return entry->value;
    }

    void add(const TKey& key, const TValue& value) override {
        if (!tree_.insert(Entry{key, value})) {
            throw std::invalid_argument("key already exists");
        }
    }

    void remove(const TKey& key) override {
        if (!tree_.erase(Entry{key, TValue{}})) {
            throw std::out_of_range("key not found");
        }
    }

    void clear() noexcept override {
        tree_.clear();
    }

private:
    AVLTree<Entry, EntryCompare> tree_;
};