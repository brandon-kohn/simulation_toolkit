//
//! Copyright � 2017
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
#include <stk/thread/job_tracker.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace stk { namespace thread {
	
	template <typename Traits = boost_thread_traits>
	class job_manager : job_tracker
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
			auto pJob = find_job( key );
			return pJob && checkStarted(pJob->get_state());
		}
		
		bool is_finished(const string_hash& key)
		{
			auto pJob = find_job( key );
			return pJob && pJob->template is<job::Finished>();
		}
		
		bool is_aborted(const string_hash& key)
		{
			auto pJob = find_job( key );
			return pJob && pJob->template is<job::Aborted>();
		}

		job* find_job( const string_hash& name ) const
		{
			return job_tracker::find_job( name );
		}

		void erase_job( const string_hash& name )
		{
			job_tracker::erase_job( name );
		}

		void erase_job( const job* pJob )
		{
			job_tracker::erase_job( pJob );
		}

		template <typename Fn, typename Exec>
		job* invoke( const stk::string_hash& key, Fn&& fn, Exec&& exec )
		{
			auto pJob = get_job( key );
			if( pJob->get_state() == job::NotStarted )
			{
				auto task = dependent_task( []()
					{ return true; },
					[key = key, fn = std::forward<Fn>( fn ), exec = std::forward<Exec>( exec ), this]()
					{ invoke_job( key, fn, exec ); } );
				while( !queue_traits::try_push( m_tasks, task ) )
					thread_traits::yield();
				m_cnd.notify_one();
			}
			return pJob;
		}

		template <typename Fn, typename Exec, typename... Dependencies>
		job* invoke_after( const stk::string_hash& key, Fn&& fn, Exec&& exec, Dependencies&&... deps )
		{
			auto pJob = get_job( key );
			if( pJob->get_state() == job::NotStarted )
			{
				auto task = dependent_task( make_predicate( std::forward<Dependencies>( deps )... ), [key = key, fn = std::forward<Fn>( fn ), exec = std::forward<Exec>( exec ), this]()
					{ invoke_job( key, fn, exec ); } );
				while( !queue_traits::try_push( m_tasks, task ) )
					thread_traits::yield();
				m_cnd.notify_one();
			}

			return pJob;
		}

	private:
		void shutdown()
		{
			m_done.store( true, std::memory_order_relaxed );
			m_cnd.notify_one();
			thread_traits::join( m_thread );
		}

		void run()
		{
			while( true )
			{
				if(dependent_task task; queue_traits::try_pop( m_tasks, task ))
				{
					if( task.is_ready() )
						task.exec();
					else
						m_internalQ.emplace_back(std::move(task));
				}

				for(auto i = m_internalQ.begin(); i != m_internalQ.end();)
				{
					auto& t = *i;
					if( t.is_ready() )
					{
						t.exec();
						i = m_internalQ.erase(i);
					} 
					else
						++i;
				}

				if(m_internalQ.empty())
				{
					unique_lock<mutex_type> lk{ m_mutex };
					m_cnd.wait_for( lk
					, m_waitTime
					, [this]()
					  { 
					      return m_tasks.size_approx() > 0 || m_done.load( std::memory_order_relaxed );
					  } );
				}
				
				if(m_done.load(std::memory_order_relaxed))
					return;
			}
		}

		std::atomic<bool>          m_done{ false };
		queue_type                 m_tasks;
		thread_type                m_thread;
		mutex_type                 m_mutex;
		condition_variable_type    m_cnd;
		boost::chrono::nanoseconds m_waitTime;
		std::vector<dependent_task>  m_internalQ;
	};

}} // namespace stk::thread

#endif // STK_THREAD_JOB_MANAGER_HPP
