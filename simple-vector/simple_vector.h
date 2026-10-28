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

    // ArrayPtr(size) создаёт элементы
    explicit SimpleVector(size_t size)
        : items_(size), size_(size), capacity_(size) {
    }

    SimpleVector(size_t size, const Type& value)
        : items_(size), size_(size), capacity_(size) {
        std::fill(begin(), end(), value);
    }

    // элементы initializer_list константные, поэтому они копируются
    SimpleVector(std::initializer_list<Type> init)
        : items_(init.size()), size_(init.size()), capacity_(init.size()) {
        std::copy(init.begin(), init.end(), begin());
    }

    explicit SimpleVector(ReserveProxyObj proxy)
        : items_(proxy.GetCapacity()), size_(0), capacity_(proxy.GetCapacity()) {
    }

    // вектор можно копировать
    SimpleVector(const SimpleVector& other)
        : items_(other.size_), size_(other.size_), capacity_(other.size_) {
        std::copy(other.begin(), other.end(), begin());
    }

    // Copy-and-swap: сначала делаем копию rhs, потом меняемся с ней местами
    SimpleVector& operator=(const SimpleVector& rhs) {
        if (this != &rhs) {
            SimpleVector rhs_copy(rhs);
            swap(rhs_copy);
        }
        return *this;
    }

    // Move-конструктор: забирает буфер у other вместо копирования элементов
    SimpleVector(SimpleVector&& other) noexcept
        : items_(std::move(other.items_))
        , size_(std::exchange(other.size_, 0))
        , capacity_(std::exchange(other.capacity_, 0)) {
    }

    // меняемся с rhs, старый буфер освободит деструктор rhs
    SimpleVector& operator=(SimpleVector&& rhs) noexcept {
        if (this != &rhs) {
            swap(rhs);
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

    // Resize: при нехватке места вызывает Reserve, новые элементы создаются по умолчанию
    void Resize(size_t new_size) {
        if (new_size > capacity_) {
            Reserve(std::max(new_size, capacity_ * 2));
        }
        if (new_size > size_) {
            std::generate(end(), begin() + new_size, [] { return Type{}; });
        }
        size_ = new_size;
    }

    // Reserve: увеличивает ёмкость, элементы переносятся в новый буфер через std::move
    void Reserve(size_t new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }
        ArrayPtr<Type> new_items(new_capacity);
        std::move(begin(), end(), new_items.Get());
        items_ = std::move(new_items);
        capacity_ = new_capacity;
    }

    // PushBack есть в двух версиях: для lvalue (const Type&) значение копируется,
    // для rvalue (Type&&) перемещается
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

    // Insert: копирование и перемещение значения
    Iterator Insert(ConstIterator pos, const Type& value) {
        return InsertImpl(pos, value);
    }

    Iterator Insert(ConstIterator pos, Type&& value) {
        return InsertImpl(pos, std::move(value));
    }

    // Erase: удаляет элемент в позиции pos, сдвигая правую часть влево через std::move
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
    // std::forward сохраняет вид ссылки: lvalue копируется, rvalue перемещается
    template <typename T>
    void EmplaceBackImpl(T&& value) {
        if (size_ == capacity_) {
            Reserve(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        items_[size_] = std::forward<T>(value);
        ++size_;
    }

    // InsertImpl: вставляет value перед pos и возвращает итератор на вставленный элемент
    template <typename T>
    Iterator InsertImpl(ConstIterator pos, T&& value) {
        assert(pos >= cbegin() && pos <= cend());
        const size_t index = static_cast<size_t>(pos - cbegin());

        if (size_ == capacity_) {
            Reserve(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        std::move_backward(begin() + index, end(), end() + 1);
        items_[index] = std::forward<T>(value);

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