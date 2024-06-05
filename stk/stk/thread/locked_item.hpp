//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/rw_race_detector.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <mutex>

namespace stk {

	template <typename T, typename Mutex = stk::thread::tiny_atomic_spin_lock<>>
	class locked_item
	{
	public:

		template <typename... Args>
		locked_item(Args&& ... args)
			: m_item( std::forward<Args>(args)... )
		{}

		template<typename Fn>
		void exec(Fn&& fn)
		{
			auto lk = std::lock_guard<Mutex>{ m_mtx };
			STK_EXCL_WRITER_DETECT_RACE( locked_item_race_detector );
			fn( m_item );
		}
		
		template<typename Fn>
		void exec_unsafe(Fn&& fn)
		{
			STK_EXCL_READERS_DETECT_RACE( locked_item_race_detector );
			fn( m_item );
		}

	private:

		T m_item;
		Mutex m_mtx;
		STK_RW_RACE_DETECTOR( locked_item_race_detector );
	
	};
};//! namespace stk;

