#pragma once

#include <cstddef>

template <typename T>
class Sequence {
public:
    virtual ~Sequence() = default;

    virtual std::size_t size() const = 0;

    virtual T& get(std::size_t index) = 0;
    virtual const T& get(std::size_t index) const = 0;

    virtual void append(const T& value) = 0;
    virtual void remove_at(std::size_t index) = 0;
    virtual void clear() = 0;
};