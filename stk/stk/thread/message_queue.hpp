//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/concurrentqueue.h>
#include <geometrix/utility/assert.hpp>
#include <type_traits>
#include <iterator>

namespace stk {

	//! A simple interface to the concurrent queue to affect a message_queue.
	template <typename Message>
	class message_queue
	{
		using queue_type = moodycamel::ConcurrentQueue<Message>;

	public:
		template <typename Value>
		bool send( Value&& value )
		{
			static_assert( std::is_convertible<Value, Message>::value, "message_queue<T>::send: Value is not convertible to Message type T." );
			auto b = m_q.enqueue( std::forward<Value>( value ) );
			GEOMETRIX_ASSERT( b );
			return b;
		}

		template <typename Range>
		bool send_range( Range&& rng )
		{
			auto it = std::begin( rng );
			auto count = std::size( rng );
			using value_type = typename std::iterator_traits<decltype( it )>::value_type;
			static_assert( std::is_convertible<value_type, Message>::value, "message_queue<T>::send: Range::value_type is not convertible to Message type T." );

			return m_q.enqueue_bulk( it, count );
		}

		template <typename Generator>
		bool send( Generator&& gen, std::size_t count )
		{
			static_assert( std::is_convertible<decltype(gen()), Message>::value, "message_queue<T>::send: Generator result type is not convertible to Message type T." );

			return m_q.generate_bulk( std::forward<Generator>(gen), count );
		}

		bool receive( Message& value )
		{
			return m_q.try_dequeue( value );
		}

		template <typename Consumer>
		bool receive( Consumer&& v )
		{
			return m_q.try_consume( std::forward<Consumer>( v ) );
		}

		template <typename Consumer>
		std::size_t receive_all( Consumer&& v )
		{
			return m_q.consume_bulk( v, m_q.size_approx() );
		}

		//! Only certain during quiescent periods (no producers or consumers active.)
		bool empty() const { return m_q.size_approx() == 0u; }

		//! Only certain during quiescent periods (no producers or consumers active.)
		void clear()
		{
			m_q.consume_bulk( +[]( Message&& ) {}, m_q.size_approx() );
		}

	private:
		queue_type m_q;
	};

} // namespace stk
