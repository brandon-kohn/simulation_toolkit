//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_THREAD_SPECIFIC_HPP
#define STK_THREAD_THREAD_SPECIFIC_HPP
#pragma once

#include <stk/container/locked_list.hpp>
#include <functional>
#include <deque>
#include <type_traits>

#include <boost/version.hpp>

#if BOOST_VERSION < 106501
#error "Boost version 1.65.1+ is required to compile the stk::thread::thread_specific."
#endif

namespace stk { namespace thread {
    namespace detail {
        template <typename Fn>
        inline void on_thread_exit(Fn&& fn)
        {
            thread_local struct raii_tl 
            {
                std::deque<std::function<void()>> callbacks;

                ~raii_tl()
                {
                    for (auto& callback: callbacks) 
                        callback();
                }
            } exiter;

            exiter.callbacks.emplace_front(std::forward<Fn>(fn));
        }
    }//! namespace detail;

template <typename T>
class thread_specific
{
	using data_ptr = std::shared_ptr<T>;
	using const_data_ptr = std::shared_ptr<typename std::add_const<T>::type>;
    using instance_map = std::map<thread_specific<T> const*, std::shared_ptr<data_ptr> >;
	static instance_map& hive();

public:

	using reference = typename std::add_lvalue_reference<T>::type;
	using const_reference = typename std::add_const<reference>::type;
		    
    thread_specific()
        : m_initializer([](){ return std::make_shared<T>();})
    {}

	~thread_specific()
	{
		destroy();
	}

	thread_specific(const thread_specific&) = delete;
	thread_specific& operator=(const thread_specific&) = delete;
	thread_specific(thread_specific&&) = delete;
	thread_specific& operator=(thread_specific&&) = delete;

    template <typename Initializer>
    thread_specific( Initializer&& init )
        : m_initializer( std::forward<Initializer>(init) )
    {}
       
    template <typename Value> 
    thread_specific& operator = ( Value&& t )
    {
		auto pData = get_or_add_item();
		GEOMETRIX_ASSERT(pData);
        *pData = std::forward<Value>(t);
        return *this;
    }
	    
	reference operator *()
	{
		return *get_or_add_item();
	}

	const_reference operator *() const
	{
		return *get_or_add_item();
	}
	
	T* operator ->()
	{
		return get_or_add_item().get();
	}

	T const* operator ->() const
	{
		return get_or_add_item().get();
	}

	bool has_value_on_calling_thread() const
	{
		return get_item() != nullptr;
	}

	explicit operator bool()
	{
		return has_value_on_calling_thread();
	}
	
private:

	data_ptr get_item() const
	{
		auto& m = hive();
		auto iter = m.lower_bound(this);
		if (iter != m.end() && iter->first == this)
			return std::atomic_load(iter->second.get());			
		
		return data_ptr();
	}
	    
    data_ptr get_or_add_item () const
    {
		auto& m = hive();
        auto iter = m.lower_bound(this);
        if(iter != m.end() && iter->first == this)
			return std::atomic_load(iter->second.get());
		
		auto pData = m_initializer();
		auto pShared = std::make_shared<data_ptr>(pData);
		m_dataChain.push_front(pShared);
        iter = m.insert(iter, std::make_pair(this, pShared));
        GEOMETRIX_ASSERT(iter != m.end());
        return pData;
    }

	void destroy()
	{
		m_dataChain.for_each([this](std::shared_ptr<data_ptr>& ptr)
		{
			std::atomic_store(ptr.get(), data_ptr());
		});
		auto& m = hive();
		
		GEOMETRIX_ASSERT(m.find(this) == m.end() || m.find(this)->second->get() == nullptr);
		m.erase(this);

		//!TODO: This leaves empties in the maps from the other threads ... 
	}

    std::function<std::shared_ptr<T>()> m_initializer;
	mutable locked_list<std::shared_ptr<data_ptr>> m_dataChain;

};

}}//! stk::thread.

//! This should be placed in a cpp file to avoid issues with singletons and the ODR.
//! There should be only one such definition for each type T specified.
#define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T)					\
namespace stk { namespace thread {									\
inline thread_specific<T>::instance_map& thread_specific<T>::hive()	\
{																	\
	static thread_local instance_map instance;						\
	instance_map& m = instance;										\
	return m;														\
}																	\
}}																	\
/***/

#endif //! STK_THREAD_THREAD_SPECIFIC_HPP
