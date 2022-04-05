//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_JOB_MANAGER_HPP
#define STK_THREAD_JOB_MANAGER_HPP
#pragma once

#include <stk/thread/concurrentqueue.h>
#include <stk/thread/std_thread_kernel.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace stk { namespace thread {

	template <typename Traits = boost_thread_traits>
	class job_manager
	{
		using thread_traits = Traits;
		using queue_traits = moodycamel_concurrent_queue_traits_no_tokens;

		using thread_type = typename thread_traits::thread_type;
		using mutex_type = typename thread_traits::mutex_type;
		using condition_variable_type = typename Traits::condition_variable_type;

		template <typename T>
		using unique_lock = typename thread_traits::template unique_lock<T>;

		template <typename... Args>
		auto make_predicate( Args&&... args )
		{
			return [args = std::make_tuple( std::forward<Args>( args )... ), this]() mutable
			{
				return std::apply( [this]( auto&&... args )
					{ return ( is_finished( args ) && ... ); },
					std::move( args ) );
			};
		}

		struct dependent_task
		{
			dependent_task() = default;
			template <typename Pred, typename Task>
			dependent_task( Pred&& pred, Task&& task )
				: m_pred( std::forward<Pred>( pred ) )
				, m_task( std::forward<Task>( task ) )
			{}
			bool                  is_ready() { return m_pred(); }
			void                  exec() { m_task(); }
			std::function<bool()> m_pred;
			std::function<void()> m_task;
		};
		using queue_type = queue_traits::queue_type<dependent_task>;

	public:
		job_manager( const boost::chrono::nanoseconds& timeout = boost::chrono::milliseconds( 20 ) )
			: m_waitTime( timeout )
		{
			try
			{
				m_thread = thread_type( [this]()
					{ run(); } );
			}
			catch( ... )
			{
				shutdown();
				throw;
			}
		}

		virtual ~job_manager()
		{
			shutdown();
		}

		bool is_started(const string_hash& key)
		{
			auto checkStarted = +[]( job::job_state s )
			{
				return s == job::Running || s == job::Finished;
			};
			auto pJob = m_tracker.find_job( key );
			return pJob && checkStarted(pJob->get_state());
		}
		
		bool is_finished(const string_hash& key)
		{
			auto pJob = m_tracker.find_job( key );
			return pJob && pJob->template is<job::Finished>();
		}
		
		bool is_aborted(const string_hash& key)
		{
			auto pJob = m_tracker.find_job( key );
			return pJob && pJob->template is<job::Aborted>();
		}

		job* find_job( const string_hash& name ) const
		{
			return m_tracker.find_job( name );
		}

		void erase_job( const string_hash& name )
		{
			m_tracker.erase_job( name );
		}

		template <typename Fn, typename Exec>
		void invoke( const stk::string_hash& key, Fn&& fn, Exec&& exec )
		{
			auto task = dependent_task( []()
				{ return true; },
				[key = key, fn = std::forward<Fn>( fn ), exec = std::forward<Exec>( exec ), this]()
				{ m_tracker.invoke_job( key, fn, exec ); } );
			while( !queue_traits::try_push( m_tasks, task ) )
				thread_traits::yield();
			{
				auto lk = unique_lock<mutex_type>{ m_mutex };
				m_cnd.notify_one();
			}
		}

		template <typename Fn, typename Exec, typename... Dependencies>
		void invoke_after( const stk::string_hash& key, Fn&& fn, Exec&& exec, Dependencies&&... deps )
		{
			auto task = dependent_task( make_predicate( std::forward<Dependencies>( deps )... ), [key = key, fn = std::forward<Fn>( fn ), exec = std::forward<Exec>( exec ), this]()
				{ m_tracker.invoke_job( key, fn, exec ); } );
			while( !queue_traits::try_push( m_tasks, task ) )
				thread_traits::yield();
			{
				auto lk = unique_lock<mutex_type>{ m_mutex };
				m_cnd.notify_one();
			}
		}

	private:
		void shutdown()
		{
			m_done.store( true, std::memory_order_relaxed );
			{
				auto lk = unique_lock<mutex_type>{ m_mutex };
				m_cnd.notify_one();
			}

			thread_traits::join( m_thread );
		}

		void run()
		{
			dependent_task task;
			bool           hasTasks = queue_traits::try_pop( m_tasks, task );
			while( true )
			{
				while( hasTasks )
				{
					if( task.is_ready() )
						task.exec();
					else
						while( !queue_traits::try_push( m_tasks, std::move( task ) ) )
							thread_traits::yield();

					if( BOOST_LIKELY( !m_done.load( std::memory_order_relaxed ) ) )
						hasTasks = queue_traits::try_pop( m_tasks, task );
					else
						return;
				}

				{
					unique_lock<mutex_type> lk{ m_mutex };
					m_cnd.wait_for( lk, m_waitTime, [&task, &hasTasks, this]()
						{ return ( ( hasTasks = queue_traits::try_pop( m_tasks, task ) ) == true ) || m_done.load( std::memory_order_relaxed ); } );
				}
				if( !hasTasks )
					return;
			}
		}

		std::atomic<bool>          m_done{ false };
		queue_type                 m_tasks;
		thread_type                m_thread;
		mutex_type                 m_mutex;
		condition_variable_type    m_cnd;
		job_tracker                m_tracker;
		boost::chrono::nanoseconds m_waitTime;
	};

}} // namespace stk::thread

#endif // STK_THREAD_JOB_MANAGER_HPP
