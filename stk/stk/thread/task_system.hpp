//
//! Copyright © 2025
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/thread/std_yield_wait_strategies.hpp>
#include <stk/thread/fixed_function.hpp>
#include <stk/utility/memory_pool.hpp>
#include <boost/container/small_vector.hpp>
#include <atomic>
#include <vector>
#include <type_traits>
#include <thread>
#include <utility>
#include <ranges>
#include <memory>
#include <mutex>
#include <exception>
#include <concepts>

namespace stk::thread {

	template <typename Pool>
	struct task_system_traits;

	template <typename QTraits, typename Traits, typename Tag, typename WaitPolicy>
	struct task_system_traits<work_stealing_thread_pool<QTraits, Traits, Tag, WaitPolicy>>
	{
		using pool_type = work_stealing_thread_pool<QTraits, Traits, Tag, WaitPolicy>;

		static auto get_thread_id( pool_type& pool )
		{
			return pool.get_thread_id();
		}

		template <typename F>
		static void enqueue( pool_type& pool, F&& task )
		{
			pool.send_no_future( std::forward<F>( task ) );
		}

		static void do_work( pool_type& pool )
		{
			pool.do_work();
		}
	};

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

		template <typename, typename>
		friend class task_system;

	private:
		stk::fixed_function<void()> fn_;
		std::atomic<uint32_t>       deps_{ 0 };
		std::atomic<uint32_t>       refs_{ 0 };
		std::atomic<bool>           completed_{ false };

		conts_vec conts_;
		lock_type conts_lock_;
		bool      sealed_{ false };
	};

	template <typename Pool, typename Policy = default_task_policy<>>
	class task_system
	{
	public:
		using task = basic_task<Policy>;
		using pool_type = Pool;

		class task_handle
		{
		public:
			task_handle() = default;

			task_handle( const task_handle& ) = delete;
			task_handle& operator=( const task_handle& ) = delete;

			task_handle( task_handle&& other ) noexcept
				: owner_( std::exchange( other.owner_, nullptr ) )
				, task_( std::exchange( other.task_, nullptr ) )
			{
			}

			task_handle& operator=( task_handle&& other ) noexcept
			{
				if( this == &other )
					return *this;

				reset();
				owner_ = std::exchange( other.owner_, nullptr );
				task_ = std::exchange( other.task_, nullptr );
				return *this;
			}

			~task_handle()
			{
				reset();
			}

			[[nodiscard]] task* get() const noexcept
			{
				return task_;
			}

			[[nodiscard]] explicit operator bool() const noexcept
			{
				return task_ != nullptr;
			}

			void reset() noexcept
			{
				if( owner_ && task_ )
					owner_->release_ref( task_ );

				owner_ = nullptr;
				task_ = nullptr;
			}

		private:
			friend class task_system;

			task_handle( task_system* owner, task* t ) noexcept
				: owner_( owner )
				, task_( t )
			{
			}

			task_system* owner_{ nullptr };
			task*        task_{ nullptr };
		};

		explicit task_system( pool_type& pool )
			: pool_( pool )
		{
			for( auto i = 0; i <= pool.number_threads(); ++i )
				thread_pools_.emplace_back( std::make_unique<thread_local_pools>() );
		}

		template <typename F>
			requires std::is_nothrow_invocable_v<F&>
		[[nodiscard]] task_handle submit( F&& f )
		{
			auto* t = make_task( std::forward<F>( f ) );
			enqueue_ready( t );
			return task_handle( this, t );
		}

		template <std::ranges::range Range, typename F>
			requires std::same_as<std::remove_cvref_t<std::ranges::range_value_t<Range>>, task_handle> && std::is_nothrow_invocable_v<F&>
		[[nodiscard]] task_handle submit_after( Range&& parents, F&& f )
		{
			auto* t = make_task( std::forward<F>( f ) );

			const auto count = static_cast<uint32_t>( std::ranges::distance( parents ) );
			t->deps_.store( count + 1, std::memory_order_relaxed ); //! +1 sentinel resolved below

			for( auto&& h : parents )
				attach_cont( h.get(), t );

			//! Resolve the sentinel; if all parents already completed this is the final decrement.
			if( t->deps_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				enqueue_ready( t );

			return task_handle( this, t );
		}

		template <typename F, typename... Parents>
			requires std::is_nothrow_invocable_v<F&>
		[[nodiscard]] task_handle submit_after( F&& f, const Parents&... parents )
		{
			static_assert( ( std::is_same_v<std::remove_cvref_t<Parents>, task_handle> && ... ),
				"submit_after variadic arguments must be task_handle" );

			auto* t = make_task( std::forward<F>( f ) );

			constexpr uint32_t count = sizeof...( Parents );
			t->deps_.store( count + 1, std::memory_order_relaxed ); //! +1 sentinel resolved below

			( attach_cont( parents.get(), t ), ... );

			//! Resolve the sentinel; if all parents already completed this is the final decrement.
			if( t->deps_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				enqueue_ready( t );

			return task_handle( this, t );
		}

		template <typename... Handles>
		void wait( Handles&... hs )
		{
			static_assert( ( std::is_same_v<std::remove_cvref_t<Handles>, task_handle> && ... ),
				"All arguments to wait(...) must be task_handle" );
			( wait_impl( hs ), ... );
		}

		template <std::ranges::range Range>
			requires std::same_as<std::remove_cvref_t<std::ranges::range_value_t<Range>>, task_handle>
		void wait( Range&& handles )
		{
			for( auto& h : handles )
				wait_impl( h );
		}
		
		void assist()
		{
			task_system_traits<pool_type>::do_work( pool_ );
		}

	private:
		struct thread_local_pools
		{
			stk::memory_pool<task> task_pool;
		};

		pool_type&                                       pool_;
		std::vector<std::unique_ptr<thread_local_pools>> thread_pools_;

		thread_local_pools& local_pools()
		{
			const auto tid = task_system_traits<pool_type>::get_thread_id( pool_ );
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
			tmem->deps_.store( 0, std::memory_order_relaxed );
			tmem->refs_.store( 2, std::memory_order_relaxed );
			tmem->completed_.store( false, std::memory_order_relaxed );
			tmem->conts_.clear();
			tmem->sealed_ = false;

			return tmem;
		}

		void enqueue_ready( task* t )
		{
			task_system_traits<pool_type>::enqueue( pool_, [this, t]() BOOST_NOEXCEPT
				{ execute( t ); } );
		}

		void attach_cont( task* parent, task* child ) noexcept
		{
			GEOMETRIX_ASSERT( parent != nullptr );
			GEOMETRIX_ASSERT( child != nullptr );

			child->refs_.fetch_add( 1, std::memory_order_relaxed );

			std::lock_guard lk( parent->conts_lock_ );

			if( !parent->sealed_ )
			{
				parent->conts_.push_back( child );
				return;
			}

			if( child->deps_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				enqueue_ready( child );

			release_ref( child );
		}

		void execute( task* t ) noexcept
		{
			GEOMETRIX_ASSERT( t != nullptr );
			GEOMETRIX_ASSERT( !t->fn_.empty() );

			try
			{
				t->fn_();
			}
			catch( ... )
			{
				std::terminate();
			}

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

				release_ref( c );
			}

			t->completed_.store( true, std::memory_order_release );
			release_ref( t );
		}

		void wait_impl( task_handle& h )
		{
			task* t = h.get();
			if( !t )
				return;

			while( !t->completed_.load( std::memory_order_acquire ) )
				task_system_traits<pool_type>::do_work( pool_ );

			h.reset();
		}

		void release_ref( task* t ) noexcept
		{
			if( !t )
				return;

			if( t->refs_.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				destroy_task( t );
		}

		static void destroy_task( task* t ) noexcept
		{
			if( !t )
				return;

			stk::deallocate_to_pool( t );
		}
	};

	template <typename TaskSystem, std::size_t InlineTaskCount = 8>
	class basic_task_scope
	{
	public:
		using task_system_type = TaskSystem;
		using task_handle      = typename task_system_type::task_handle;

		explicit basic_task_scope( task_system_type& tasks, bool cancel_on_exception = true )
			: tasks_( tasks )
			, cancel_on_exception_( cancel_on_exception )
		{
		}

		basic_task_scope( const basic_task_scope& ) = delete;
		basic_task_scope& operator=( const basic_task_scope& ) = delete;

		basic_task_scope( basic_task_scope&& ) = delete;
		basic_task_scope& operator=( basic_task_scope&& ) = delete;

		~basic_task_scope() noexcept
		{
			wait_noexcept();

			//! If a task failed and the caller never explicitly observed it,
			//! do not silently swallow it on the happy path.
			if( has_exception_.load( std::memory_order_acquire ) )
			{
				if( std::uncaught_exceptions() == 0 )
					std::terminate();
			}
		}

		template <typename F>
		void run( F&& f )
		{
			handles_.push_back(
				tasks_.submit(
					[this, fn = std::forward<F>( f )]() noexcept mutable
					{
						if( cancel_on_exception_ && has_exception_.load( std::memory_order_acquire ) )
							return;

						try
						{
							fn();
						}
						catch( ... )
						{
							record_exception( std::current_exception() );
						}
					} ) );
		}

		void wait()
		{
			tasks_.wait( handles_ );
			rethrow_if_exception();
		}

		void wait_noexcept() noexcept
		{
			tasks_.wait( handles_ );
		}

		[[nodiscard]] bool has_exception() const noexcept
		{
			return has_exception_.load( std::memory_order_acquire );
		}

	private:
		void record_exception( std::exception_ptr eptr ) noexcept
		{
			if( !eptr )
				return;

			if( !has_exception_.exchange( true, std::memory_order_acq_rel ) )
			{
				std::lock_guard lk( exception_mutex_ );
				first_exception_ = std::move( eptr );
			}
		}

		void rethrow_if_exception()
		{
			if( !has_exception_.load( std::memory_order_acquire ) )
				return;

			std::exception_ptr eptr;
			{
				std::lock_guard lk( exception_mutex_ );
				eptr = std::exchange( first_exception_, nullptr );
			}

			has_exception_.store( false, std::memory_order_release );

			if( eptr )
				std::rethrow_exception( eptr );
		}

		task_system_type& tasks_;
		boost::container::small_vector<task_handle, InlineTaskCount> handles_;

		std::atomic<bool> has_exception_{ false };
		std::exception_ptr first_exception_;
		std::mutex exception_mutex_;
		bool cancel_on_exception_{ true };
	};

	template <typename TaskSystem, std::size_t InlineTaskCount = 8>
	using task_scope = basic_task_scope<TaskSystem, InlineTaskCount>;

	template <typename TaskSystem, typename Lock = tiny_atomic_spin_lock<eager_std_thread_yield_wait<64>>>
	class basic_thread_local_task_scope
	{
	public:
		using task_system_type = TaskSystem;

		explicit basic_thread_local_task_scope( task_system_type& tasks, bool cancel_on_exception = true ) noexcept
			: tasks_( &tasks )
			, cancel_on_exception_( cancel_on_exception )
			, prev_( current_ )
		{
			current_ = this;
		}

		basic_thread_local_task_scope( const basic_thread_local_task_scope& ) = delete;
		basic_thread_local_task_scope& operator=( const basic_thread_local_task_scope& ) = delete;

		~basic_thread_local_task_scope() noexcept
		{
			wait_noexcept();
			current_ = prev_;

			if( has_exception_.load( std::memory_order_acquire ) && std::uncaught_exceptions() == 0 )
				std::terminate();
		}

		template <typename F>
		void spawn( F&& f )
		{
			pending_.fetch_add( 1, std::memory_order_relaxed );

			try
			{
				tasks_->submit(
					[this, fn = std::forward<F>( f )]() mutable noexcept
					{
						if( cancel_on_exception_ && has_exception_.load( std::memory_order_acquire ) )
						{
							complete_one();
							return;
						}

						try
						{
							fn();
						}
						catch( ... )
						{
							record_exception( std::current_exception() );
						}

						complete_one();
					} );
			}
			catch( ... )
			{
				//! If submit itself throws before ownership transfers, balance the count.
				pending_.fetch_sub( 1, std::memory_order_acq_rel );
				throw;
			}
		}

		void wait()
		{
			while( pending_.load( std::memory_order_acquire ) != 0 )
				tasks_->assist();

			rethrow_if_exception();
		}

		void wait_noexcept() noexcept
		{
			while( pending_.load( std::memory_order_acquire ) != 0 )
				tasks_->assist();
		}

		[[nodiscard]] static basic_thread_local_task_scope* current() noexcept
		{
			return current_;
		}

		[[nodiscard]] static bool has_current() noexcept
		{
			return current_ != nullptr;
		}

		template <typename F>
		static void spawn_current( F&& f )
		{
			auto* s = current_;
			GEOMETRIX_ASSERT( s != nullptr );
			s->spawn( std::forward<F>( f ) );
		}

	private:
		void complete_one() noexcept
		{
			pending_.fetch_sub( 1, std::memory_order_acq_rel );
		}

		void record_exception( std::exception_ptr eptr ) noexcept
		{
			if( !eptr )
				return;

			if( !has_exception_.exchange( true, std::memory_order_acq_rel ) )
			{
				std::lock_guard<Lock> lk( exception_lock_ );
				first_exception_ = std::move( eptr );
			}
		}

		void rethrow_if_exception()
		{
			if( !has_exception_.load( std::memory_order_acquire ) )
				return;

			std::exception_ptr eptr;
			{
				std::lock_guard<Lock> lk( exception_lock_ );
				eptr = std::exchange( first_exception_, nullptr );
			}

			has_exception_.store( false, std::memory_order_release );

			if( eptr )
				std::rethrow_exception( eptr );
		}

		task_system_type*     tasks_{ nullptr };
		std::atomic<uint32_t> pending_{ 0 };
		std::atomic<bool>     has_exception_{ false };
		std::exception_ptr    first_exception_;
		Lock                  exception_lock_;
		bool                  cancel_on_exception_{ true };

		basic_thread_local_task_scope* prev_{ nullptr };

		static thread_local basic_thread_local_task_scope* current_;
	};

	template <typename TaskSystem, typename Lock>
	thread_local basic_thread_local_task_scope<TaskSystem, Lock>*
		basic_thread_local_task_scope<TaskSystem, Lock>::current_
		= nullptr;

} // namespace stk::thread