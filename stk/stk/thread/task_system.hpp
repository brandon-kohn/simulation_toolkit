#pragma once

#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/thread/std_yield_wait_strategies.hpp>
#include <stk/thread/fixed_function.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/utility/memory_pool.hpp>
#include <boost/container/small_vector.hpp>
#include <atomic>
#include <vector>
#include <type_traits>
#include <thread>
#include <utility>
#include <ranges>

namespace stk::thread {

	template <std::size_t InlineContCount = 4, typename Lock = tiny_atomic_spin_lock<eager_std_thread_yield_wait<64>>>
	struct default_task_policy
	{
		static constexpr std::size_t inline_cont_count = InlineContCount;
		using lock_type = Lock;
	};

	template <typename Policy>
	class basic_task
	{
	public:
		using lock_type = typename Policy::lock_type;
		static constexpr std::size_t InlineCount = Policy::inline_cont_count;
		using conts_vec = boost::container::small_vector<basic_task*, InlineCount>;

		basic_task() = default;
		basic_task( const basic_task& ) = delete;
		basic_task& operator=( const basic_task& ) = delete;

		//! Allow the task_system (and only it) to manipulate internals
		template <typename, typename>
		friend class task_system;

	private:
		//! Function to execute
		stk::fixed_function<void()> fn_;

		//! Dependency counters
		std::atomic<uint32_t> deps_{ 0 };
		std::atomic<uint32_t> refs_{ 1 };
		std::atomic<bool>     completed_{ false };

		//! Continuation management
		conts_vec conts_;
		lock_type conts_lock_;
		bool      sealed_{ false };
	};


	template <typename QueueTraits = moodycamel_concurrent_queue_traits_no_tokens, typename Policy = default_task_policy<>>
	class task_system
	{
	public:
		using task = basic_task<Policy>;

		explicit task_system( work_stealing_thread_pool<QueueTraits>& pool )
			: pool_( pool )
		{
			//! Index 0 is reserved for tasks created/executed outside the pool.
			//! Worker threads use indices [1, number_threads()].
			for( auto i = 0; i <= pool.number_threads(); ++i )
				thread_pools_.emplace_back( std::make_unique<thread_local_pools>() );
		}

		template <typename F>
		task* submit( F&& f )
		{
			auto* t = make_task( std::forward<F>( f ) );
			enqueue_ready( t );
			return t;
		}

		//! Submit task that depends on all parents in a range
		template <std::ranges::range Range, typename F>
			requires std::convertible_to<std::ranges::range_value_t<Range>, task*>
		task* submit_after( Range&& parents, F&& f )
		{
			auto* t = make_task( std::forward<F>( f ) );

			const auto count = static_cast<uint32_t>( std::ranges::distance( parents ) );
			t->deps_.store( count, std::memory_order_relaxed );

			for( auto* p : parents )
				attach_cont( p, t ); //! sealed parent will immediately fulfill

			if( t->deps_.load( std::memory_order_acquire ) == 0 )
				enqueue_ready( t );

			return t;
		}

		//! Submit task that depends on all listed parent tasks (variadic form)
		template <typename F, typename... Parents>
		task* submit_after( F&& f, Parents*... parents )
		{
			static_assert( ( std::is_same_v<Parents, task> && ... ),
				"submit_after_all variadic arguments must be task*" );

			auto*              t = make_task( std::forward<F>( f ) );
			constexpr uint32_t count = sizeof...( Parents );
			t->deps_.store( count, std::memory_order_relaxed );

			( attach_cont( parents, t ), ... ); //! sealed parent will immediately fulfill

			if( t->deps_.load( std::memory_order_acquire ) == 0 )
				enqueue_ready( t );

			return t;
		}

		//! \brief Waits for the specified task to complete.
		//!
		//! This function blocks the calling thread until the given task and all of
		//! its dependent continuations have fully completed and been released.
		//!
		//! Unlike a passive wait (e.g., std::future::wait()), this call will *actively*
		//! participate in the thread pool’s work-stealing loop while waiting. That
		//! means the calling thread will execute other ready tasks in the pool to
		//! ensure progress and avoid deadlocks that could occur if all worker threads
		//! were waiting simultaneously.
		//!
		//! \param t
		//!   Pointer to the task whose completion is being waited on.
		//!
		//! \warning
		//!   - This call must not be used on a task belonging to a *different* pool,
		//!     or undefined behavior may occur.
		//!   - Never call wait() from a continuation of the same task—it will deadlock.
		//!   - Do not reuse or destroy the task pointer after calling wait(); the task
		//!     is automatically released once complete.
		//!
		//! \note
		//!   Internally, the wait loop checks `t->refs` until only one reference
		//!   (the waiting thread's) remains. While waiting, it repeatedly invokes
		//!   `pool_.do_work()` to execute pending tasks and yield CPU time when idle.
		//!
		//! \see submit(), submit_after(), submit_after_all()
		//!

		//! Wait for one or more tasks to complete.
		template <typename... Tasks>
		void wait( Tasks*... ts )
		{
			static_assert( ( std::is_same_v<Tasks, task> && ... ), "All arguments to wait(...) must be task*" );
			( wait_impl( ts ), ... );
		}

		template <std::ranges::range Range>
			requires std::convertible_to<std::ranges::range_value_t<Range>, task*>
		void wait( Range&& tasks )
		{
			for( auto* t : tasks )
				wait_impl( t );
		}

	private:
		struct thread_local_pools
		{
			stk::memory_pool<task> task_pool;
		};

		work_stealing_thread_pool<QueueTraits>&          pool_;
		std::vector<std::unique_ptr<thread_local_pools>> thread_pools_;

		thread_local_pools& local_pools()
		{
			const auto tid = work_stealing_thread_pool<QueueTraits>::get_thread_id();
			const auto idx = ( tid < thread_pools_.size() ) ? tid : 0;
			return *thread_pools_[idx];
		}

		template <typename F>
		task* make_task( F&& f )
		{
			auto& tls = local_pools();
			task* tmem = tls.task_pool.allocate();
			stk::memory_pool_base<task>::construct( tmem );
			tmem->fn_ = stk::fixed_function<void()>( std::forward<F>( f ) );
			return tmem;
		}

		inline void fulfill( task* t ) noexcept
		{
			if( t->deps.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				enqueue_ready( t );
		}

		static inline void release( task* t ) noexcept
		{
			if( !t )
				return;
			if( t->refs_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				destroy_task( t );
		}

		void enqueue_ready( task* t )
		{
			pool_.send_no_future( [this, t]() BOOST_NOEXCEPT
				{ execute( t ); } );
		}

		void attach_cont( task* parent, task* child ) noexcept
		{
			child->refs_.fetch_add( 1, std::memory_order_relaxed );
			std::lock_guard lk( parent->conts_lock_ );
			if( !parent->sealed_ )
			{
				parent->conts_.push_back( child );
				return;
			}
			if( child->deps_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				enqueue_ready( child );
			if( child->refs_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				destroy_task( child );
		}

		void execute( task* t ) noexcept
		{
#if GEOMETRIX_TEST_ENABLED( CLEESE_OUTPUT_DDM_PED_UPDATE )
			GEOMETRIX_ASSERT( !t->fn.empty() );
#endif
			t->fn_();

			typename task::conts_vec local;
			{
				std::lock_guard lk( t->conts_lock_ );
				t->sealed_ = true;
				local.swap( t->conts_ );
			}

			for( task* c : local )
			{
				if( c->deps_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
					enqueue_ready( c );
				if( c->refs_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
					destroy_task( c );
			}

			t->completed_.store( true, std::memory_order_release );
			if( t->refs_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				destroy_task( t );
		}

		static void destroy_task( task* t ) noexcept
		{
			if( !t )
				return;
			stk::deallocate_to_pool( t );
		}
		
		inline void wait_impl( task* t )
		{
			while( t->refs_.load( std::memory_order_acquire ) > 1 )
				pool_.do_work();

			release( t );
		}
	};

} // namespace stk::thread
