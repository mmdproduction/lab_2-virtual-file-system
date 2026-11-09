#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

template <typename T>
class DynamicArray{
    private:
    std::size_t size_ = 0;
    T* data_ = nullptr;

    public:

    DynamicArray() = default;
    DynamicArray(const DynamicArray<T>& other)
        : size_(other.size_),
        data_(size_ == 0 ? nullptr : new T[size_]) {
        try {
            for (std::size_t i = 0; i < size_; ++i) {
                data_[i] = other.data_[i];
            }
        } catch (...) {
            delete[] data_;
            throw;
        }
    }

    DynamicArray<T>& operator=(const DynamicArray<T>& other){
        if(&other == this)
            return *this;
        DynamicArray<T> result(other);

        std::swap(size_, result.size_);
        std::swap(data_, result.data_);

        return *this;
    }

    ~DynamicArray() {
        delete[] data_;
    }

    explicit DynamicArray(std::size_t size)
    : size_(size),
      data_(size == 0 ? nullptr : new T[size]{}) {}

    std::size_t size() const {
    return size_;
    }

    T& get(std::size_t index){
        if(index >= size_)
            throw std::out_of_range("index out of range");
        return data_[index];
    }

    const T& get(std::size_t index) const{
        if(index >= size_)
            throw std::out_of_range("index out of range");
        return data_[index];
    }

    void resize(std::size_t new_size) {
    if (new_size == size_) {
        return;
    }

    DynamicArray<T> result(new_size);

    const std::size_t copy_size =
        size_ < new_size ? size_ : new_size;

    for (std::size_t i = 0; i < copy_size; ++i) {
        result.data_[i] = data_[i];
    }

    std::swap(size_, result.size_);
    std::swap(data_, result.data_);
}
};