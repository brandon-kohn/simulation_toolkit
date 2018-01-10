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

#include <stk/container/lock_free_concurrent_skip_list.hpp>
#include <functional>
#include <deque>
#include <type_traits>

#include <boost/version.hpp>

#if BOOST_VERSION < 106501
#error "Boost version 1.65.1+ is required to compile the stk::thread::thread_specific."
#endif

namespace stk { namespace thread {

	template <typename T>
	class thread_specific;

	namespace detail {
		template <typename Fn>
		inline void on_thread_exit(Fn&& fn)
		{
			thread_local struct raii_tl
			{
				std::deque<std::function<void()>> callbacks;

				~raii_tl()
				{
					for (auto& callback : callbacks)
						callback();
				}
			} exiter;

			exiter.callbacks.emplace_front(std::forward<Fn>(fn));
		}

    }//! namespace detail;

template <typename T>
class thread_specific
{
	using data_ptr = T*;
	using const_data_ptr = typename std::add_const<data_ptr>::type;
	struct instance_map : std::map<thread_specific<T> const*, T>
	{
		instance_map() = default;

		~instance_map()
		{
			for(auto const& i : *this)
				i.first->m_maps.erase(this);
		}
	};

	static instance_map& hive();

public:

	using reference = typename std::add_lvalue_reference<T>::type;
	using const_reference = typename std::add_const<reference>::type;
		    
    thread_specific()
        : m_initializer([](){ return T();})
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
			return &iter->second;
		
		return nullptr;
	}
	    
    data_ptr get_or_add_item () const
    {
		auto& m = hive();
        auto iter = m.lower_bound(this);
        if(iter != m.end() && iter->first == this)
			return &iter->second;
	
		m_maps.insert(&m);
		auto data = m_initializer();
        iter = m.insert(iter, std::make_pair(this, std::forward<T>(data)));
        GEOMETRIX_ASSERT(iter != m.end());
        return &iter->second;
    }

	void destroy()
	{
		auto& m = hive();
        auto key = this;
        m.erase(key);
        std::for_each(m_maps.begin(), m_maps.end(), [key](instance_map* pMap)
        { 
            pMap->erase(key);
		});
		
		GEOMETRIX_ASSERT(m.find(this) == m.end());
	}

    std::function<T()> m_initializer;
	mutable lock_free_concurrent_set<instance_map*> m_maps;

};

}}//! stk::thread.

//! This should be placed in a cpp file to avoid issues with singletons and the ODR.
//! There should be only one such definition for each type T specified.
#define STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(T)					\
namespace stk { namespace thread {									\
inline thread_specific<T>::instance_map& thread_specific<T>::hive()	\
{																	\
	static thread_local instance_map instance = instance_map();		\
	instance_map& m = instance;										\
	return m;														\
}																	\
}}																	\
/***/

#endif //! STK_THREAD_THREAD_SPECIFIC_HPP
