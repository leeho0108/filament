/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UTILS_VECTOR_H
#define UTILS_VECTOR_H

#include <utils/Allocator.h>
#include <utils/compiler.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace utils {

/**
 * Inserts the specified item in the vector at its sorted position.
 */
template <typename T>
static inline void insert_sorted(std::vector<T>& v, T item) {
    auto pos = std::lower_bound(v.begin(), v.end(), item);
    v.insert(pos, std::move(item));
}

/**
 * Inserts the specified item in the vector at its sorted position.
 * The item type must implement the < operator. If the specified
 * item is already present in the vector, this method returns without
 * inserting the item again.
 *
 * @return True if the item was inserted at is sorted position, false
 *         if the item already exists in the vector.
 */
template <typename T>
static inline bool insert_sorted_unique(std::vector<T>& v, T item) {
    if (UTILS_LIKELY(v.size() == 0 || v.back() < item)) {
        v.push_back(item);
        return true;
    }

    auto pos = std::lower_bound(v.begin(), v.end(), item);
    if (UTILS_LIKELY(pos == v.end() || item < *pos)) {
        v.insert(pos, std::move(item));
        return true;
    }

    return false;
}

//  -----------------------------------------------------------------------------------------------

template<typename ALLOCATOR>
class TrivialVectorBase {
protected:
    using size_type = uint32_t;
    using allocator_type = ALLOCATOR;

    char* mBegin = nullptr;
    size_type mItemCount = 0;
    size_type mCapacity = 0;
    ALLOCATOR mAllocator{};

    TrivialVectorBase() noexcept = default;

    explicit TrivialVectorBase(const allocator_type& allocator) noexcept : mAllocator(allocator) { }

    TrivialVectorBase(size_type itemSize, size_type count, const allocator_type& allocator) noexcept
            : mItemCount(count), mCapacity(count), mAllocator(allocator) {
        mBegin = (char*)mAllocator.alloc(mCapacity * itemSize);
    }

    TrivialVectorBase(size_type itemSize, size_type count) noexcept
            : TrivialVectorBase(itemSize, count, allocator_type()) {
    }

    TrivialVectorBase(TrivialVectorBase const& rhs) = default;

    TrivialVectorBase(TrivialVectorBase&& rhs) noexcept
            : mAllocator(std::move(rhs.mAllocator)) {
        using std::swap;
        swap(mBegin, rhs.mBegin);
        swap(mItemCount, rhs.mItemCount);
        swap(mCapacity, rhs.mCapacity);
    }

    ~TrivialVectorBase() noexcept {
        mAllocator.free(mBegin);
    }

    TrivialVectorBase& operator=(TrivialVectorBase const& rhs) = default;

    TrivialVectorBase& operator=(TrivialVectorBase&& rhs) noexcept {
        if (this != &rhs) {
            this->swap(rhs);
        }
    }

    inline void* assert_capacity_for_size(size_type c, size_type itemSize) {
        if (UTILS_UNLIKELY(mCapacity < c)) {
            assert_capacity_slow(c, itemSize);
        }
        size_type offset = mItemCount * itemSize;
        mItemCount = c;
        return mBegin + offset;
    }

    UTILS_NOINLINE
    void assert_capacity_slow(size_type c, size_type itemSize) {
        c = (c * 3 + 1) / 2;
        set_capacity(c, itemSize);
    }

    UTILS_NOINLINE
    void set_capacity(size_type n, size_type itemSize) {
        if (UTILS_UNLIKELY(n == mCapacity)) {
            return;
        }
        char* addr = (char*)mAllocator.alloc(n * itemSize);
        memcpy(addr, mBegin, ((mItemCount < n) ? mItemCount : n) * itemSize);
        mAllocator.free(mBegin);
        mBegin = addr;
        mCapacity = n;
    }

    UTILS_NOINLINE
    void swap(TrivialVectorBase& other) {
        using std::swap;
        swap(mBegin, other.mBegin);
        swap(mItemCount, other.mItemCount);
        swap(mCapacity, other.mCapacity);
        swap(mAllocator, other.mAllocator);
    }

    char* begin() noexcept { return mBegin; }
    char* end(size_type itemSize) noexcept { return mBegin + mItemCount * itemSize; }
    char const* begin() const noexcept { return const_cast<TrivialVectorBase*>(this)->begin(); }
    char const* end(size_type itemSize) const noexcept { return const_cast<TrivialVectorBase*>(this)->end(itemSize); }
};

template<typename T, typename A = HeapAllocator,
        std::enable_if_t<std::is_trivially_copyable_v<T>, bool> = true>
class vector : private TrivialVectorBase<A> {
    using base = TrivialVectorBase<A>;
    inline T* assert_capacity_for_size(typename base::size_type c) {
        return static_cast<T*>(this->base::assert_capacity_for_size(c, sizeof(T)));
    }
public:
    using allocator_type = typename base::allocator_type;
    using value_type = T;
    using reference = T&;
    using const_reference = T const&;
    using size_type = typename base::size_type;
    using difference_type = int32_t;
    using pointer = T*;
    using const_pointer = T const*;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    using const_reference_or_value = std::conditional_t<
            std::is_fundamental_v<T> || std::is_pointer_v<T>, value_type, const_reference>;

    vector() noexcept = default;

    vector(vector const& rhs) = default;

    vector(vector&& rhs) = default;

    explicit vector(const allocator_type& allocator) noexcept : base(allocator) { }

    explicit vector(
            size_type count,
            const_reference_or_value proto = value_type{},
            const allocator_type& allocator = allocator_type{}) noexcept
                    : base(sizeof(T), count, allocator) {
        std::generate_n(begin(), count, [proto] { return proto; });
    }

    vector(size_type count, const allocator_type& allocator) noexcept
            : vector(count, value_type{}, allocator) { }

    vector& operator=(vector const& rhs) = default;

    vector& operator=(vector&& rhs) = default;

    allocator_type get_allocator() const noexcept {
        return this->mAllocator;
    }

    // --------------------------------------------------------------------------------------------

    iterator begin() noexcept { return reinterpret_cast<iterator>(base::begin()); }

    iterator end() noexcept { return reinterpret_cast<iterator>(base::end(sizeof(T))); }

    const_iterator begin() const noexcept { return reinterpret_cast<const_iterator>(base::begin()); }

    const_iterator end() const noexcept { return reinterpret_cast<const_iterator>(base::end(sizeof(T))); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

    const_iterator cbegin() const noexcept { return begin(); }

    const_iterator cend() const noexcept { return end(); }

    const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    const_reverse_iterator crend() const noexcept { return rend(); }

    // --------------------------------------------------------------------------------------------

    size_type size() const noexcept { return this->mItemCount; }

    size_type capacity() const noexcept { return this->mCapacity; }

    bool empty() const noexcept { return this->mItemCount == 0; }

    // --------------------------------------------------------------------------------------------

    reference operator[](size_type n) noexcept { return *(begin() + n); }

    const_reference operator[](size_type n) const noexcept { return *(begin() + n); }

    reference front() noexcept { return *begin(); }

    const_reference front() const noexcept { return *begin(); }

    reference back() noexcept { return *(end() - 1); }

    const_reference back() const noexcept { return *(end() - 1); }

    value_type* data() noexcept { return begin(); }

    const value_type* data() const noexcept { return begin(); }

    // --------------------------------------------------------------------------------------------

    UTILS_NOINLINE
    void push_back(const_reference_or_value v) {
        *assert_capacity_for_size(size() + 1) = v;
    }

    template<typename ... ARGS>
    reference emplace_back(ARGS&& ... args) {
        auto pos = assert_capacity_for_size(size() + 1);
        new(pos) value_type(std::forward<ARGS>(args)...);
        return *pos;
    }

    void pop_back() {
        this->mItemCount--;
    }

    iterator insert(const_iterator position, const_reference_or_value v) {
        auto pos = assert_capacity_for_size(size() + 1);
        std::move_backward(position, pos, pos + 1);
        *position = v;
    }

    iterator erase(const_iterator position) {
        std::move(position + 1, end(), position);
        this->mItemCount--;
    }

    iterator erase(const_iterator first, const_iterator last) {
        std::move(last, end(), first);
        this->mItemCount -= last - first;
    }

    void clear() noexcept {
        this->mItemCount = 0;
    }

    void resize(size_type count) {
        resize(count, value_type{});
    }

    void resize(size_type count, const_reference_or_value v) {
        auto itemCount = size();
        assert_capacity_for_size(count);
        if (count > itemCount) {
            std::generate(begin() + itemCount, begin() + count, [v] { return v; });
        }
    }

    void swap(vector& other) {
        base::swap(other);
    }

    void reserve(size_type n) {
        this->set_capacity(n, sizeof(T));
    }

    void shrink_to_fit() noexcept {
        this->set_capacity(size(), sizeof(T));
    }
};

} // end utils namespace

#endif //UTILS_VECTOR_H
