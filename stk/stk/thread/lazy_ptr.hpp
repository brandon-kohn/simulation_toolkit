//
//! Copyright � 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/detail/lazy_ptr.hpp>

namespace stk {
		
	enum class basic_lazy_ptr_state
	{
		null = 0,
		valid = 1,
		building = 2,
		failed = 3
	};

	template <typename T, typename Deleter = std::default_delete<T>, typename ExceptHandler = default_except_handler>
	class basic_lazy_ptr;

	template <typename T, typename Deleter, typename ExceptHandler>
	class basic_lazy_ptr : private lazy_policy_base_helper<Deleter, ExceptHandler>::type
	{
		using base_t = typename lazy_policy_base_helper<Deleter, ExceptHandler>::type;

	public:
		using pointer_type = T*;

	private:
		using atomic_ptr_t = std::atomic<pointer_type>;
		using deleter_t = Deleter;
		using except_handler_t = ExceptHandler;

		auto          get_deleter() -> decltype( std::declval<base_t>().get_policy_1() ) { return base_t::get_policy_1(); }
		auto          get_except_handler() -> decltype( std::declval<base_t>().get_policy_2() ) { return base_t::get_policy_2(); }
		atomic_ptr_t& get_atomic() const { return m_state; }

		static T* get_build_code()
		{
			return reinterpret_cast<T*>( static_cast<std::size_t>( 0xBC ) );
		}

		static T* get_fail_code()
		{
			return reinterpret_cast<T*>( static_cast<std::size_t>( 0xFC ) );
		}

	public:

		basic_lazy_ptr( Deleter const& d = Deleter{}, ExceptHandler const& e = ExceptHandler{} )
			: base_t{ d, e }
			, m_state{}
		{}

		//! no move or copy.
		basic_lazy_ptr( const basic_lazy_ptr& ) = delete;
		basic_lazy_ptr& operator=( const basic_lazy_ptr& ) = delete;
		basic_lazy_ptr( basic_lazy_ptr&& ) = delete;
		basic_lazy_ptr& operator=( basic_lazy_ptr&& ) = delete;

		//! if base type, cannot delete from base ptr.
		~basic_lazy_ptr()
		{
			auto* ptr = get_atomic().load( std::memory_order_relaxed );
			if( ptr && ptr != get_build_code() && ptr != get_fail_code() )
			{
				get_deleter()( ptr );
			}
			else if( ptr == get_build_code() )
			{
				wait_for_build();
				ptr = get_atomic().load( std::memory_order_relaxed );
				GEOMETRIX_ASSERT( ptr != get_build_code() ); //! This should not be destructing when being built. Should wait?
				if( ptr != get_fail_code() )
				{
					get_deleter()( ptr );
				}
			}
		}

		T* try_get() const
		{
			auto* ptr = get_atomic().load( std::memory_order_relaxed );
			if( BOOST_LIKELY( ptr && ptr != get_build_code() && ptr != get_fail_code() ) )
				return ptr;
			return nullptr;
		}

		template <typename Init>
		T* get( Init&& init ) const
		{
			return get_or_build( std::forward<Init>( init ) );
		}

		basic_lazy_ptr_state get_state() const
		{
			auto ptr = get_atomic().load( std::memory_order_relaxed );
			if( ptr )
			{
				if( ptr != get_build_code() )
				{
					if( ptr != get_fail_code() )
						return basic_lazy_ptr_state::valid;
					return basic_lazy_ptr_state::failed;
				}
				return basic_lazy_ptr_state::building;
			}
			return basic_lazy_ptr_state::null;
		}

		//! Not thread-safe with build state.
		//! If returns a fail-state, could be used to try again?
		pointer_type release() noexcept
		{
			auto* ptr = get_atomic().exchange( nullptr );
			// Never return build or fail codes
			if( ptr != get_build_code() && ptr != get_fail_code() )
				return ptr;
			return nullptr;
		}

	protected:
		
		//! Returns either the built item or a nullptr is there is a fail code.
		template <typename Init>
		T* get_or_build( Init&& init ) const
		{
			T* expected = get_atomic().load( std::memory_order_relaxed );
			if( BOOST_LIKELY( expected && expected != get_build_code() && expected != get_fail_code() ) )
				return expected;

			if( expected == nullptr )
			{
				if( get_atomic().compare_exchange_strong( expected, get_build_code() ) )
				{
					T* ptr = nullptr;
					try
					{
						ptr = init();
					}
					catch( ... )
					{
						get_atomic().store( get_fail_code() );
						throw;
					}
					get_atomic().store( ptr );
					get_atomic().notify_all();
					return ptr;
				}
			}

			//! It's either being built, failed, or valid now.
			GEOMETRIX_ASSERT( expected != nullptr );

			return wait_for_build();
		}

		T* wait_for_build() const
		{
			T* ptr = nullptr;
			int           spin_count = 0;
			constexpr int max_spin = 16;
			while( ( ptr = get_atomic().load( std::memory_order_relaxed ) ) == get_build_code() )
			{
				if( spin_count++ < max_spin )
				{
					std::this_thread::yield();
				}
				else
				{
					get_atomic().wait( get_build_code() );
				}
			}

			GEOMETRIX_ASSERT( ptr != get_build_code() );
			return ptr != get_fail_code() ? ptr : nullptr;
		}

		mutable atomic_ptr_t m_state;
	};

	template <typename T, typename Deleter = std::default_delete<T>, typename ExceptHandler = default_except_handler>
	class lazy_ptr : private basic_lazy_ptr<T, Deleter, ExceptHandler>
	{
		using base_type = basic_lazy_ptr<T, Deleter, ExceptHandler>;

	public:
		template <typename Initializer>
		lazy_ptr( Initializer&& init, const Deleter& d = Deleter{}, const ExceptHandler& e = ExceptHandler{} )
			: base_type( d, e )
			, m_init( std::forward<Initializer>( init ) )
		{}

		T* operator->()
		{
			auto* ptr = base_type::get_or_build( m_init );
			return ptr;
		}

		const T* operator->() const
		{
			auto* ptr = base_type::get_or_build( m_init );
			return ptr;
		}

		T& operator*()
		{
			auto* ptr = base_type::get_or_build( m_init );
			GEOMETRIX_ASSERT( ptr != nullptr );
			return *ptr;
		}

		const T& operator*() const
		{
			auto* ptr = base_type::get_or_build( m_init );
			GEOMETRIX_ASSERT( ptr != nullptr );
			return ptr;
		}

		T* get() const { return base_type::get_or_build( m_init ); }
		T* try_get() const { return base_type::try_get(); }
		T* release() { return base_type::release(); }

	private:
		std::function<T*()> m_init;
	};

	//! In case where initializer is an empty class, shrink this.
	template <typename T, T* (*Init)(), typename Deleter = std::default_delete<T>, typename ExceptHandler = default_except_handler>
	class lazy_lean_ptr : private basic_lazy_ptr<T, Deleter, ExceptHandler>
	{
		using lazy_base = basic_lazy_ptr<T, Deleter, ExceptHandler>;

	public:
		lazy_lean_ptr( const Deleter& d = Deleter{}, const ExceptHandler& e = ExceptHandler{} )
			: lazy_base(d, e) 
		{}

		T* operator->()
		{
			auto* ptr = lazy_base::get_or_build( Init );
			return ptr;
		}

		const T* operator->() const
		{
			auto* ptr = lazy_base::get_or_build( Init );
			return ptr;
		}

		T& operator*()
		{
			auto* ptr = lazy_base::get_or_build( Init );
			GEOMETRIX_ASSERT( ptr != nullptr );
			return *ptr;
		}

		const T& operator*() const
		{
			auto* ptr = lazy_base::get_or_build( Init );
			GEOMETRIX_ASSERT( ptr != nullptr );
			return ptr;
		}

		T* get() const { return lazy_base::get_or_build( Init ); }
		T* try_get() const { return lazy_base::try_get(); }
		T* release() { return lazy_base::release(); }
	};

}//! namespace stk;

