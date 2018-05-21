//
//  Copyright (C) 2008, 2009, 2016 Tim Blechmann, based on code by Cory Nelson
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CONTAINER_ATOMIC_MARKED_PTR_HPP
#define STK_CONTAINER_ATOMIC_MARKED_PTR_HPP

#include <geometrix/utility/assert.hpp>
#include <limits>
#include <cstdint>
#include <atomic>

#include <boost/predef.h>

namespace stk {

template <typename T>
class atomic_markable_ptr
{
public:
    using mark_type = bool;

private:
    using storage_type = std::uint64_t;
    using atomic_ptr = std::atomic<storage_type>;

    static T* extract_ptr(storage_type const& i)
    {
        return (T*)(((i) & ~(storage_type)1));
    }

    static mark_type extract_mark(storage_type const& i)
    {
        return ((i) & 1);
    }

    static storage_type combine(T* ptr, mark_type mark)
    {
        GEOMETRIX_ASSERT(((storage_type)ptr & 1) == 0);
        return ((((storage_type)ptr) & ~(storage_type)1) | (mark));
    }

public:

    atomic_markable_ptr() = default;
    explicit atomic_markable_ptr(T* p, mark_type t = 0)
        : m_ptr(combine(p, t))
    {}

    atomic_markable_ptr(atomic_markable_ptr const&) = delete;
    atomic_markable_ptr& operator=(atomic_markable_ptr const&) = delete;
	atomic_markable_ptr(atomic_markable_ptr&& o)
		: m_ptr(std::move(o.m_ptr)) 
	{}

	atomic_markable_ptr& operator=(atomic_markable_ptr&& o)
	{
		m_ptr = std::move(o.m_ptr);
		return *this;
	}


    std::tuple<T*, mark_type> get() const
    {
        auto pPtr = m_ptr.load();
        return std::make_tuple(extract_ptr(pPtr), extract_mark(pPtr));
    }

    void set(T* p, mark_type t)
    {
        m_ptr = combine(p, t);
    }

    bool operator==(atomic_markable_ptr const& p) const
    {
        return m_ptr == p.m_ptr;
    }

    bool operator!=(atomic_markable_ptr const& p) const
    {
        return !operator==(p);
    }

    T* get_ptr() const
    {
        auto pPtr = m_ptr.load();
        return extract_ptr(pPtr);
    }

    void set_ptr(T* p)
    {
        auto mark = get_mark();
        auto pPtr = combine(p, mark);
        m_ptr.store(pPtr);
    }

    mark_type get_mark() const
    {
        auto pPtr = m_ptr.load();
        return extract_mark(pPtr);
    }

    void set_mark(mark_type t)
    {
        auto p = get_ptr();
        auto pPtr = combine(p, t);
        m_ptr.store(pPtr);
    }

    T& operator*() const
    {
        auto pPtr = get_ptr();
        GEOMETRIX_ASSERT(pPtr);
        return *pPtr;
    }

    T* operator->() const
    {
        return get_ptr();
    }

    explicit operator bool() const
    {
        return get_ptr() != 0;
    }

    //! atomic interface
    bool is_lock_free() const { return m_ptr.is_lock_free(); }
    void store(T* p, mark_type m, std::memory_order order = std::memory_order_seq_cst) { m_ptr.store(combine(p, m), order); }

    std::tuple<T*, mark_type> load(std::memory_order order = std::memory_order_seq_cst) const BOOST_NOEXCEPT
    {
        auto pPtr = m_ptr.load(order);
        return std::make_tuple(extract_ptr(pPtr), extract_mark(pPtr));
    }

    std::tuple<T*, mark_type> exchange(T* desiredPtr, mark_type desiredStamp, std::memory_order order = std::memory_order_seq_cst) BOOST_NOEXCEPT
    {
        auto pPtr = combine(desiredPtr, desiredStamp);
        auto pResult = m_ptr.exchange(pPtr, order);
        return std::make_tuple(extract_ptr(pResult), extract_mark(pResult));
    }

    bool compare_exchange_weak(T*& expectedPtr, mark_type& expectedStamp, T* desiredPtr, mark_type desiredStamp, std::memory_order order = std::memory_order_seq_cst) BOOST_NOEXCEPT
    {
        auto pPtrExpected = combine(expectedPtr, expectedStamp);
        auto pPtrDesired = combine(desiredPtr, desiredStamp);
        if( m_ptr.compare_exchange_weak(pPtrExpected, pPtrDesired, order))
            return true;
        expectedPtr = extract_ptr(pPtrExpected);
        expectedStamp = extract_mark(pPtrExpected);
        return false;
    }

    bool compare_exchange_strong(T*& expectedPtr, mark_type& expectedStamp, T* desiredPtr, mark_type desiredStamp, std::memory_order order = std::memory_order_seq_cst) BOOST_NOEXCEPT
    {
        auto pPtrExpected = combine(expectedPtr, expectedStamp);
        auto pPtrDesired = combine(desiredPtr, desiredStamp);
        if( m_ptr.compare_exchange_strong(pPtrExpected, pPtrDesired, order))
            return true;
        expectedPtr = extract_ptr(pPtrExpected);
        expectedStamp = extract_mark(pPtrExpected);
        return false;
    }

    storage_type load_raw(std::memory_order order = std::memory_order_seq_cst) const { return m_ptr.load(order); }
    void store_raw(storage_type s, std::memory_order order = std::memory_order_seq_cst) { m_ptr.store(s, order); }

private:

    atomic_ptr m_ptr { 0ULL };

};

}//! namespace stk;

#endif //! STK_CONTAINER_ATOMIC_MARKED_PTR_HPP

