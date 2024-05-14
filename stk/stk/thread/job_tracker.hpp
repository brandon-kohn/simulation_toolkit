//
//! Copyright © 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/container/concurrent_pointer_unordered_map.hpp>
#include <stk/utility/memory_pool.hpp>
#include <stk/utility/string_hash.hpp>
#include <stk/thread/scalable_task_counter.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/utility/scope_exit.hpp>
#include <geometrix/utility/assert.hpp>

#include <atomic>
#include <mutex>
#include <cstdint>

namespace stk {

	template <typename TIDAccessor, typename Executor, typename Traits = thread::boost_thread_traits>
	class simple_job_tracker
	{
		using thread_traits = Traits;

	public:
		simple_job_tracker( std::size_t nthreads, TIDAccessor&& tidAccess, Executor&& exec )
			: m_counter( nthreads + 1 )
			, m_getTID{ std::forward<TIDAccessor>( tidAccess ) }
			, m_executor{ std::forward<Executor>( exec ) }
		{}

		template <typename Job>
		auto add_job( Job&& job )
		{
			m_counter.increment( m_getTID() );
			m_executor( [job = std::move( job ), this]()
				{
					STK_SCOPE_EXIT( m_counter.decrement( m_getTID() ) );
					job(); } );
		}

		template <typename Job>
		void operator()( Job&& job )
		{
			add_job( std::forward<Job>( job ) );
		}

		bool is_finished() const
		{
			return m_counter.count() == 0;
		}

		template <typename DoWork>
		void wait_or_work( DoWork&& doWork )
		{
			while( !is_finished() )
				doWork();
		}

	private:
		thread::scalable_task_counter m_counter;
		TIDAccessor                   m_getTID;
		Executor                      m_executor;
	};

	template <typename ThreadIdxAccessor, typename Executor>
	inline simple_job_tracker<ThreadIdxAccessor, Executor> make_job_tracker( std::size_t nthreads, ThreadIdxAccessor&& tids, Executor&& exec )
	{
		return simple_job_tracker<ThreadIdxAccessor, Executor>{ nthreads, std::forward<ThreadIdxAccessor>( tids ), std::forward<Executor>( exec ) };
	}

	struct job
	{
		enum job_state : std::uint8_t
		{
			NotStarted = 0,
			Running = 1,
			Finished = 2,
			Aborted = 3
		};

		job()
			: state{}
			, hash( "invalid" )
		{}
		job( const string_hash& n )
			: state{}
			, hash{ n }
		{}

		template <job_state S>
		void set()
		{
			if constexpr( S == Running )
			{
				auto expected = NotStarted;
				state.compare_exchange_strong( expected, S, std::memory_order_relaxed );
			}
			if constexpr( S == Finished || S == Aborted )
			{
				auto expected = Running;
				state.compare_exchange_strong( expected, S, std::memory_order_relaxed );
			}
		}

		template <job_state S>
		bool is() const
		{
			return state.load( std::memory_order_relaxed ) == S;
		}

		job_state get_state() const { return state.load( std::memory_order_relaxed ); }

	private:
		friend class job_tracker;
		std::atomic<job_state> state;
		stk::string_hash       hash;
	};

	class job_tracker
	{
	public:
		job_tracker() = default;

		job* find_job( const string_hash& name ) const
		{
			return find_job_impl( name );
		}

		void erase_job( const string_hash& name )
		{
			auto key = name.hash();
			m_map.erase_direct( key );
		}

		void erase_job( const job* pJob )
		{
			GEOMETRIX_ASSERT( pJob );
			auto key = pJob->hash.hash();
			m_map.erase_direct( key );
		}

		template <typename JobFn, typename Executor>
		job* invoke_job( const string_hash& name, JobFn&& fn, Executor&& executor )
		{
			auto* j = get_job( name );
			GEOMETRIX_ASSERT( j->get_state() == job::NotStarted );
			executor( [j = j, name = name, fn = fn, executor = executor, this]()
				{
				j->template set<job::Running>();
				try
				{
				    fn();
				}
                catch (...)
                {
					j->template set<job::Aborted>();
					throw;
                }
				j->template set<job::Finished>(); } );
			return j;
		}

		void quiesce()
		{
			m_qsbr.flush();
		}

	protected:
		job* get_job( const string_hash& name )
		{
			return get_job_impl( name );
		}

		using mpool_t = memory_pool<job>;
		struct pool_deleter
		{
			void operator()( job* v ) const
			{
				stk::deallocate_to_pool( v );
			}
		};

		job* find_job_impl( const string_hash& name ) const
		{
			auto  key = name.hash();
			auto* j = m_map.find( key );
			return j;
		}

		job* get_job_impl( const string_hash& name )
		{
			auto  key = name.hash();
			auto* j = m_map.find( key );
			if( j != nullptr )
				return j;

			auto [ivalue, inserted] = m_map.insert( key, std::unique_ptr<job, pool_deleter>{ mpool_t::construct( m_pool.allocate() ) } );
			return ivalue;
		}

		mutable mpool_t                                                                                                 m_pool;
		mutable concurrent_pointer_unordered_map<std::size_t, job, pool_deleter, junction::QSBRMemoryReclamationPolicy> m_map;
		junction::QSBR                                                                                                  m_qsbr;
	};

} // namespace stk
