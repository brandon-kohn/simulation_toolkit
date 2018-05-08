//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>
#include <jemalloc/jemalloc.h>

namespace stk {

    template <typename T>
    class jemallocator
    {
    public:
        typedef T* pointer;
        typedef const T* const_pointer;
        typedef T& reference;
        typedef const T& const_reference;
        typedef T value_type;
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;

        jemallocator() noexcept {}
        jemallocator(const jemallocator&) {}
        template <typename U> jemallocator(const jemallocator<U>&) noexcept {}
        ~jemallocator() {}

        pointer address(T& r) const
        {
            return &r;
        }

        const_pointer address(const T& s) const
        {
            return &s;
        }

        size_type max_size() const
        {
            return (static_cast<size_t>(0) - static_cast<size_t>(1)) / sizeof(T);
        }

        template <typename U>
        struct rebind
        {
            typedef jemallocator<U> other;
        };

        template <typename U> bool operator==(const jemallocator<U>&) const noexcept { return true; }
        template <typename U> bool operator!=(const jemallocator<U>&) const noexcept { return false; }

        void construct(T* const p, const T& t) const
        {
            void * const pv = static_cast<void *>(p);
            new (pv) T(t);
        }

        void destroy(T* const p) const
        {
            p->~T();
        }

        T* allocate(const size_t n) const
        {
            if (n == 0)
                return nullptr;

            if (n > max_size())
                throw std::length_error("jemallocator<T>::allocate() - Integer overflow.");

            void * const pv = je_malloc(n * sizeof(T));

            if (pv == nullptr)
                throw std::bad_alloc();

            return static_cast<T*>(pv);
        }

        void deallocate(T* const p, const size_t n) const noexcept
        {
            je_free(p);
        }

        template <typename U>
        pointer allocate(const size_t n, const U * /* const hint */) const
        {
            return allocate(n);
        }
    };
}//! namespace stk;

