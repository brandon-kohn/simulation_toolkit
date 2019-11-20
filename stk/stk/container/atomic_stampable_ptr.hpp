//
//  Copyright (C) 2008, 2009, 2016 Tim Blechmann, based on code by Cory Nelson
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CONTAINER_ATOMIC_STAMPABLE_PTR_HPP
#define STK_CONTAINER_ATOMIC_STAMPABLE_PTR_HPP

#include <limits>
#include <cstdint>
#include <atomic>

#include <boost/predef.h>
#ifndef BOOST_ARCH_X86_64
    #error "atomic_stampable_ptr only works on the 48bit address space used in X86_64 as of 2017."
#endif

namespace stk {

template <typename T>
class atomic_stampable_ptr
{
public:
    using stamp_type = std::uint16_t;

private:
    using storage_type = std::uint64_t;
    using atomic_ptr = std::atomic<storage_type>;

    static const int stamp_index = 3;
    static const storage_type ptr_mask = 0xffffffffffffUL; //(1L<<48L)-1;

    static T* extract_ptr(storage_type const& i)
    {
        return (T*)(i & ptr_mask);
    }

    union cast_unit
    {
        storage_type value;
        stamp_type stamp[4];
    };

    static stamp_type extract_stamp(storage_type const& i)
    {
        cast_unit cu;
        cu.value = i;
        return cu.stamp[stamp_index];
    }

    static storage_type combine(T* ptr, stamp_type stamp)
    {
        cast_unit ret;
        ret.value = storage_type(ptr);
        ret.stamp[stamp_index] = stamp;
        return ret.value;
    }

    BOOST_CONSTEXPR std::memory_order failure_order(std::memory_order order) const BOOST_NOEXCEPT
    {
        return order == std::memory_order_acq_rel ? std::memory_order_acquire : (order == std::memory_order_release ? std::memory_order_relaxed : order);
    }

public:

    atomic_stampable_ptr() = default;
    explicit atomic_stampable_ptr(T* p, stamp_type t = 0)
        : m_ptr(combine(p, t))
    {}

    atomic_stampable_ptr(atomic_stampable_ptr const&) = delete;
    atomic_stampable_ptr& operator=(atomic_stampable_ptr const&) = delete;

	atomic_stampable_ptr(atomic_stampable_ptr&& o)
		: m_ptr(std::move(o.m_ptr))
	{}

	atomic_stampable_ptr& operator=(atomic_stampable_ptr&& o)
	{
		m_ptr = std::move(o.m_ptr);
		return *this;
	}

    std::tuple<T*, stamp_type> get() const
    {
        auto pPtr = m_ptr.load();
        return std::make_tuple(extract_ptr(pPtr), extract_stamp(pPtr));
    }

    void set(T* p, stamp_type t)
    {
        m_ptr = combine(p, t);
    }

    bool operator==(atomic_stampable_ptr const& p) const
    {
        return m_ptr == p.m_ptr;
    }

    bool operator!=(atomic_stampable_ptr const& p) const
    {
        return !operator==(p);
    }

    T* get_ptr() const
    {
        auto pPtr = m_ptr.load(std::memory_order_acquire);
        return extract_ptr(pPtr);
    }

    void set_ptr(T* p)
    {
        auto stamp = get_stamp();
        auto pPtr = combine(p, stamp);
        m_ptr.store(pPtr, std::memory_order_release);
    }

    stamp_type get_stamp() const
    {
        auto pPtr = m_ptr.load(std::memory_order_acquire);
        return extract_stamp(pPtr);
    }

    stamp_type get_next_stamp() const
    {
        return (get_stamp() + 1u) & (std::numeric_limits<stamp_type>::max)();
    }

    void set_stamp(stamp_type t)
    {
        auto p = get_ptr();
        auto pPtr = combine(p, t);
        m_ptr.store(pPtr, std::memory_order_release);
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
    void store(T* p, stamp_type m, std::memory_order order = std::memory_order_seq_cst) { m_ptr.store(combine(p, m), order); }

    std::tuple<T*, stamp_type> load(std::memory_order order = std::memory_order_seq_cst) const BOOST_NOEXCEPT
    {
        auto pPtr = m_ptr.load(order);
        return std::make_tuple(extract_ptr(pPtr), extract_stamp(pPtr));
    }

    std::tuple<T*, stamp_type> exchange(T* desiredPtr, stamp_type desiredStamp, std::memory_order order = std::memory_order_seq_cst) BOOST_NOEXCEPT
    {
        auto pPtr = combine(desiredPtr, desiredStamp);
        auto pResult = m_ptr.exchange(pPtr, order);
        return std::make_tuple(extract_ptr(pResult), extract_stamp(pResult));
    }

    bool compare_exchange_weak(T*& expectedPtr, stamp_type& expectedStamp, T* desiredPtr, stamp_type desiredStamp, std::memory_order order = std::memory_order_seq_cst) BOOST_NOEXCEPT
    {
        auto pPtrExpected = combine(expectedPtr, expectedStamp);
        auto pPtrDesired = combine(desiredPtr, desiredStamp);
        if( m_ptr.compare_exchange_weak(pPtrExpected, pPtrDesired, order, failure_order(order)))
            return true;
        expectedPtr = extract_ptr(pPtrExpected);
        expectedStamp = extract_stamp(pPtrExpected);
        return false;
    }

    bool compare_exchange_strong(T*& expectedPtr, stamp_type& expectedStamp, T* desiredPtr, stamp_type desiredStamp, std::memory_order order = std::memory_order_seq_cst) BOOST_NOEXCEPT
    {
        auto pPtrExpected = combine(expectedPtr, expectedStamp);
        auto pPtrDesired = combine(desiredPtr, desiredStamp);
        if( m_ptr.compare_exchange_strong(pPtrExpected, pPtrDesired, order, failure_order(order)))
            return true;
        expectedPtr = extract_ptr(pPtrExpected);
        expectedStamp = extract_stamp(pPtrExpected);
        return false;
    }

    storage_type load_raw(std::memory_order order = std::memory_order_seq_cst) const { return m_ptr.load(order); }
    void store_raw(storage_type s, std::memory_order order = std::memory_order_seq_cst) { m_ptr.store(s, order); }

private:

    atomic_ptr m_ptr { 0ULL };

};

}//! namespace stk;

#endif //! STK_CONTAINER_ATOMIC_STAMPABLE_PTR_HPP

