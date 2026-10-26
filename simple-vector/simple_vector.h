#pragma once

#include "array_ptr.h"

#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <utility>

class ReserveProxyObj {
public:
    explicit ReserveProxyObj(size_t capacity_to_reserve)
        : capacity_(capacity_to_reserve) {
    }

    size_t GetCapacity() const noexcept {
        return capacity_;
    }

private:
    size_t capacity_;
};

inline ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}

template <typename Type>
class SimpleVector {
public:
    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;

    explicit SimpleVector(size_t size)
        : SimpleVector(size, Type{}) {
    }

    SimpleVector(size_t size, const Type& value)
        : items_(size), size_(size), capacity_(size) {
        std::fill(begin(), end(), value);
    }

    SimpleVector(std::initializer_list<Type> init)
        : items_(init.size()), size_(init.size()), capacity_(init.size()) {
        std::move(init.begin(), init.end(), begin());
    }

    explicit SimpleVector(ReserveProxyObj proxy)
        : items_(proxy.GetCapacity()), size_(0), capacity_(proxy.GetCapacity()) {
    }

    // operator= сначала делает копию rhs,
    // а потом меняет её местами с текущим вектором 
    SimpleVector(const SimpleVector& other)
        : items_(other.size_), size_(other.size_), capacity_(other.size_) {
        std::copy(other.begin(), other.end(), begin());
    }

    SimpleVector& operator=(const SimpleVector& rhs) {
        if (this != &rhs) {
            SimpleVector rhs_copy(rhs);
            swap(rhs_copy);
        }
        return *this;
    }

    // Move-конструктор: забирает буфер у other вместо копирования элементов.
    // После этого other пуст (size = 0, capacity = 0), но остаётся рабочим.
    SimpleVector(SimpleVector&& other) noexcept
        : items_(std::move(other.items_))
        , size_(std::exchange(other.size_, 0))
        , capacity_(std::exchange(other.capacity_, 0)) {
    }

    // Move-присваивание: забирает буфер у rhs, rhs остаётся пустым
    SimpleVector& operator=(SimpleVector&& rhs) noexcept {
        if (this != &rhs) {
            items_ = std::move(rhs.items_);
            size_ = std::exchange(rhs.size_, 0);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }

    size_t GetSize() const noexcept {
        return size_;
    }

    size_t GetCapacity() const noexcept {
        return capacity_;
    }

    bool IsEmpty() const noexcept {
        return size_ == 0;
    }

    Type& operator[](size_t index) noexcept {
        assert(index < size_);
        return items_[index];
    }

    const Type& operator[](size_t index) const noexcept {
        assert(index < size_);
        return items_[index];
    }

    Type& At(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index is out of range");
        }
        return items_[index];
    }

    const Type& At(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index is out of range");
        }
        return items_[index];
    }

    void Clear() noexcept {
        size_ = 0;
    }

// если места не хватает, элементы переносятся в новый буфер
    void Resize(size_t new_size) {
        if (new_size <= size_) {
            size_ = new_size;
            return;
        }
        if (new_size <= capacity_) {
            for (size_t i = size_; i < new_size; ++i) {
                items_[i] = Type{};
            }
            size_ = new_size;
            return;
        }

        const size_t new_capacity = std::max(new_size, capacity_ * 2);
        ArrayPtr<Type> new_items(new_capacity);

        std::move(begin(), end(), new_items.Get());
        for (size_t i = size_; i < new_size; ++i) {
            new_items[i] = Type{};
        }
        items_ = std::move(new_items);
        size_ = new_size;
        capacity_ = new_capacity;
    }

    // Reserve увеличивает ёмкость, переносит элементы через std::move
    void Reserve(size_t new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }
        ArrayPtr<Type> new_items(new_capacity);
        std::move(begin(), end(), new_items.Get());
        items_ = std::move(new_items);
        capacity_ = new_capacity;
    }

    // const Type& копирует значение, Type&& перемещает
    void PushBack(const Type& value) {
        EmplaceBackImpl(value);
    }

    void PushBack(Type&& value) {
        EmplaceBackImpl(std::move(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        --size_;
    }

    Iterator Insert(ConstIterator pos, const Type& value) {
        return InsertImpl(pos, value);
    }

    Iterator Insert(ConstIterator pos, Type&& value) {
        return InsertImpl(pos, std::move(value));
    }

    // Erase: удаляет элемент, сдвигая остальные влево через std::move
    Iterator Erase(ConstIterator pos) {
        assert(pos >= cbegin() && pos < cend());
        const size_t index = static_cast<size_t>(pos - cbegin());
        std::move(begin() + index + 1, end(), begin() + index);
        --size_;
        return begin() + index;
    }

    void swap(SimpleVector& other) noexcept {
        items_.swap(other.items_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    Iterator begin() noexcept {
        return items_.Get();
    }

    Iterator end() noexcept {
        return items_.Get() + size_;
    }

    ConstIterator begin() const noexcept {
        return items_.Get();
    }

    ConstIterator end() const noexcept {
        return items_.Get() + size_;
    }

    ConstIterator cbegin() const noexcept {
        return items_.Get();
    }

    ConstIterator cend() const noexcept {
        return items_.Get() + size_;
    }

private:
    // std::forward сохраняет вид ссылки: lvalue копируется, rvalue перемещается.
    template <typename T>
    void EmplaceBackImpl(T&& value) {
        if (size_ == capacity_) {
            Reserve(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        items_[size_] = std::forward<T>(value);
        ++size_;
    }

    // InsertImpl: вставляет value перед pos; при нехватке места создаёт новый буфер
    template <typename T>
    Iterator InsertImpl(ConstIterator pos, T&& value) {
        assert(pos >= cbegin() && pos <= cend());
        const size_t index = static_cast<size_t>(pos - cbegin());

        if (size_ == capacity_) {
            const size_t new_capacity = capacity_ == 0 ? 1 : capacity_ * 2;
            ArrayPtr<Type> new_items(new_capacity);
            std::move(begin(), begin() + index, new_items.Get());
            new_items[index] = std::forward<T>(value);
            std::move(begin() + index, end(), new_items.Get() + index + 1);
            items_ = std::move(new_items);
            capacity_ = new_capacity;
        } else {
            std::move_backward(begin() + index, end(), end() + 1);
            items_[index] = std::forward<T>(value);
        }

        ++size_;
        return begin() + index;
    }

    ArrayPtr<Type> items_;
    size_t size_ = 0;
    size_t capacity_ = 0;
};

template <typename Type>
bool operator==(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return lhs.GetSize() == rhs.GetSize() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename Type>
bool operator!=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs == rhs);
}

template <typename Type>
bool operator<(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename Type>
bool operator<=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(rhs < lhs);
}

template <typename Type>
bool operator>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return rhs < lhs;
}

template <typename Type>
bool operator>=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs < rhs);
}