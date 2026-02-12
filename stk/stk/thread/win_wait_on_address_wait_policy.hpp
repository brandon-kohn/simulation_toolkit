//
//! Copyright © 2026
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/cache_line_padding.hpp>
#include <windows.h>

namespace stk::thread {
	//! Windows WaitOnAddress-based idle/wait policy
	//! - No mutex/CV
	//! - Uses a single epoch word that changes on notify
	//! - Requires Windows 8+ (Win11 is fine)
	template <typename Traits>
	struct win_wait_on_address_policy
	{
#if !defined( _WIN32 )
		static_assert( false, "win_wait_on_address_policy requires Windows." );
#endif

		struct context
		{
			//! epoch changes on notify. Waiters block while epoch stays equal to the captured value.
			//! Align to reduce false sharing with other hot fields.
			alignas( STK_CACHE_LINE_SIZE ) std::atomic<std::uint32_t> epoch{ 0 };
		};

		//! Backoff policy for the "idle iteration" phase (same behavior as your CV policy).
		static bool on_idle_iteration( std::uint32_t spincount ) noexcept
		{
			if( spincount < 100 )
			{
				auto backoff = spincount * 10;
				while( backoff-- )
					Traits::yield();
				return true; //! re-poll
			}
			return false; //! park
		}

		template <typename Pred>
		static void wait( context& ctx, Pred&& pred )
		{
			//! IMPORTANT:
			//! - Pred may have side effects (your pred calls poll() and writes hasTask/task).
			//! - We therefore always call pred() as the authoritative check before sleeping.
			while( true )
			{
				if( pred() )
					return;

				//! Capture the current epoch value. If it changes, we should wake.
				const std::uint32_t expected = ctx.epoch.load( std::memory_order_relaxed );

				//! Double-check pred() before sleeping to reduce missed wake windows.
				if( pred() )
					return;

				//! Wait until epoch != expected (or spuriously). INFINITE wait.
				//! WaitOnAddress compares the value at Address to CompareAddress bytes; it sleeps while equal.
				std::uint32_t compare = expected;
				(void)WaitOnAddress(
					reinterpret_cast<volatile void*>( &ctx.epoch ),
					&compare,
					sizeof( compare ),
					0xFFFFFFFFu );

				//! Loop and re-check pred.
			}
		}

		static void notify_one( context& ctx ) noexcept
		{
			//! Bump epoch then wake a sleeper.
			ctx.epoch.fetch_add( 1, std::memory_order_release );
			WakeByAddressSingle( (void*)&ctx.epoch );
		}

		static void notify_all( context& ctx ) noexcept 
		{
			ctx.epoch.fetch_add( 1, std::memory_order_release );
			WakeByAddressAll( (void*)&ctx.epoch );
		}
	};
} //! namespace stk::thread;
