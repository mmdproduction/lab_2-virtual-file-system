#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>

#include "../DynamicArray/DynamicArray.hpp"
#include "../Sequence.hpp"

template <typename T>
class ArraySequence : public Sequence<T> {
private:
    DynamicArray<T> storage_;
    std::size_t length_ = 0;
    
public:
    ArraySequence() = default;
    ~ArraySequence() override = default;

    std::size_t size() const override {
        return length_;
    }

    T& get(std::size_t index) override {
        if (index >= length_) {
            throw std::out_of_range("index out of range");
        }
        return storage_.get(index);
    }

    const T& get(std::size_t index) const override {
        if (index >= length_) {
            throw std::out_of_range("index out of range");
        }
        return storage_.get(index);
    }

    void append(const T& value) override {
        const T value_copy(value);

        if (length_ == storage_.size()) {
            const std::size_t capacity = storage_.size();

            if (capacity >
                std::numeric_limits<std::size_t>::max() / 2) {
                throw std::length_error("sequence is too large");
            }

            const std::size_t new_capacity =
                capacity == 0 ? 1 : capacity * 2;

            storage_.resize(new_capacity);
        }

        storage_.get(length_) = value_copy;
        ++length_;
    }

    void remove_at(std::size_t index) override {
        if (index >= length_) {
            throw std::out_of_range("index out of range");
        }

        DynamicArray<T> result(length_ - 1);
        std::size_t destination = 0;

        for (std::size_t i = 0; i < length_; ++i) {
            if (i != index) {
                result.get(destination) = storage_.get(i);
                ++destination;
            }
        }

        storage_ = result;
        --length_;
    }

    void clear() override {
        storage_.resize(0);
        length_ = 0;
    }

    void swap(ArraySequence<T>& other) noexcept {
        storage_.swap(other.storage_);
        std::swap(length_, other.length_);
    }
};