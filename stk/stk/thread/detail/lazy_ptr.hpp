//
//! Copyright � 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <geometrix/utility/assert.hpp>
#include <memory>
#include <atomic>
#include <functional>
#include <exception>

namespace stk {

	template <typename T, typename EnableIf = void>
	struct default_create
	{
		using type = T;
		template <typename... Args>
		type operator()( Args&&... a ) const
		{
			return type( std::forward<Args>( a )... );
		}
	};

	struct default_except_handler
	{
		void operator()( std::exception_ptr p ) const
		{
			std::rethrow_exception( p );
		}
	};

	template <typename T1, typename T2>
	constexpr unsigned int encode_empty_bases()
	{
		unsigned int r = 0;
		if constexpr( std::is_empty<T1>::value == false )
			r += 10;
		if constexpr( std::is_empty<T2>::value == false )
			r += 1;
		return r;
	}

	template <typename T1, typename T2, unsigned int Code>
	struct empty_base_helper;

	//! Deleter
	template <typename T, typename EnableIf = void>
	struct deleter_policy
	{
		T&       get_deleter() { return m_deleter; }
		const T& get_deleter() const { return m_deleter; }

	private:
		T m_deleter;
	};
	
    template <typename T>
	struct deleter_policy<T, typename std::enable_if<std::is_empty<T>::value>::type>
	{
		T get_deleter() const { return T{}; }
	};
	
    template <typename T, typename EnableIf = void>
	struct creator_policy
	{
		T&       get_creator() { return m_creator; }
		const T& get_creator() const { return m_creator; }

	private:
		T m_creator;
	};
	
    template <typename T>
	struct creator_policy<T, typename std::enable_if<std::is_empty<T>::value>::type>
	{
		T get_creator() const { return T{}; }
	};
	
    template <typename T, typename EnableIf = void>
	struct except_handler_policy
	{
		T&       get_except_handler() { return m_except_handler; }
		const T& get_except_handler() const { return m_except_handler; }

	private:
		T m_except_handler;
	};
	
    template <typename T>
	struct except_handler_policy<T, typename std::enable_if<std::is_empty<T>::value>::type>
	{
		T get_except_handler() const { return T{}; }
	};

	template <typename T1, typename T2>
	struct empty_base_helper<T1, T2, 0>
	{
		empty_base_helper( const T1&, const T2& )
		{}
		T1 get_policy_1() const { return T1{}; }
		T2 get_policy_2() const { return T2{}; }
	};

	template <typename T1, typename T2>
	struct empty_base_helper<T1, T2, 1>
	{
		empty_base_helper() = default;
		empty_base_helper( const T1&, const T2& t2 )
			: m_2( t2 )
		{}

		T1        get_policy_1() const { return T1{}; }
		T2&       get_policy_2() { return m_2; }
		const T2& get_policy_2() const { return m_2; }
		T2        m_2;
	};

	template <typename T1, typename T2>
	struct empty_base_helper<T1, T2, 10>
	{
		empty_base_helper() = default;
		empty_base_helper( const T1& t1, const T2& )
			: m_1( t1 )
		{}

		T1&       get_policy_1() { return m_1; }
		const T1& get_policy_1() const { return m_1; }
		T2        get_policy_2() const { return T2{}; }
		T1        m_1;
	};

	template <typename T1, typename T2>
	struct empty_base_helper<T1, T2, 11>
	{
		empty_base_helper() = default;
		empty_base_helper( const T1& t1, const T2& t2 )
			: m_1( t1 )
			, m_2( t2 )
		{}

		T1&       get_policy_1() { return m_1; }
		const T1& get_policy_1() const { return m_1; }
		T2&       get_policy_2() { return m_2; }
		const T2& get_policy_2() const { return m_2; }
		T1        m_1;
		T2        m_2;
	};

	template <typename T1, typename T2>
	struct lazy_policy_base_helper
	{
		static constexpr unsigned int code = encode_empty_bases<T1, T2>();
		using type = empty_base_helper<T1, T2, code>;
	};

}//! namespace stk

