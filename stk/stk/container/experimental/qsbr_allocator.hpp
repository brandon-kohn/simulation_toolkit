//
//! Copyright © 2018
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
#include <stk/container/experimental/simple_qsbr.hpp>

namespace stk {

    template <typename T, typename MemoryReclaimer = stk::thread::simple_qsbr, typename Alloc = std::allocator<T>>
    class qsbr_allocator : private Alloc
    {
        using base_t = Alloc;
        using reclaimer_t = MemoryReclaimer;

        template <typename U, typename M, typename A>
        friend class qsbr_allocator;

    public:

        using value_type = T;

       template <typename U> struct rebind
       {
           using ubase = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;
           typedef qsbr_allocator<U, MemoryReclaimer, ubase> other;
       };

        qsbr_allocator() = delete;
        qsbr_allocator(MemoryReclaimer& m) BOOST_NOEXCEPT
            : m_reclaimer(&m)
        {}
        qsbr_allocator(const qsbr_allocator& q)
            : base_t(q)
            , m_reclaimer(q.m_reclaimer)
        {}

        template <typename U>
        qsbr_allocator(const qsbr_allocator<U>& q) BOOST_NOEXCEPT
            : base_t(q)
            , m_reclaimer(q.m_reclaimer)
        {}

        ~qsbr_allocator() {}

        template <typename U> bool operator==(const qsbr_allocator<U>& u) const BOOST_NOEXCEPT { return u.m_reclaimer == m_reclaimer; }
        template <typename U> bool operator!=(const qsbr_allocator<U>& u) const BOOST_NOEXCEPT { return u.m_reclaimer != m_reclaimer; }

        template <typename ... Args>
        void construct(T* const p, Args&&... a)
        {
            std::allocator_traits<base_t>::construct(*this, p, std::forward<Args>(a)...);
        }

        void destroy(T* const p)
        {
            //! This is deferred into the reclaimer.
            //base_t::destroy(p);
        }

        T* allocate(const std::size_t n)
        {
            return std::allocator_traits<base_t>::allocate(*this, n);
        }

        void deallocate(T* const p, const std::size_t n) BOOST_NOEXCEPT
        {
            memory_reclamation_traits<reclaimer_t>::reclaim_from_allocator(*m_reclaimer, *(base_t*)this, p, n);
        }

    private:

        qsbr_allocator & operator=(const qsbr_allocator&) = delete;

        mutable reclaimer_t* m_reclaimer{nullptr};

    };
}//! namespace stk;
