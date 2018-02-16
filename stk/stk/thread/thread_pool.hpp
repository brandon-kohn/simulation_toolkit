
#ifndef STK_THREAD_WAITABLETHREADPOOL_HPP
#define STK_THREAD_WAITABLETHREADPOOL_HPP
#pragma once

#include <stk/thread/function_wrapper_with_allocator.hpp>
#include <stk/thread/function_wrapper.hpp>
#include <stk/container/locked_queue.hpp>
#ifdef STK_USE_JEMALLOC
#include <stk/utility/jemallocator.hpp>
#endif
#include <stk/utility/scope_exit.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/partition_work.hpp>
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>

namespace stk { namespace thread {

    template <typename QTraits = locked_queue_traits, typename Traits = boost_thread_traits>
    class thread_pool : boost::noncopyable
    {
        using thread_traits = Traits;
        using queue_traits = QTraits;

        using thread_type = typename thread_traits::thread_type;

        template <typename T>
        using packaged_task = typename Traits::template packaged_task_type<T>;

        using condition_variable_type = typename Traits::condition_variable_type;

        using mutex_type = typename thread_traits::mutex_type;
#ifdef STK_USE_JEMALLOC
		using alloc_type = stk::jemallocator<char>;
#else
		using alloc_type = std::allocator<char>;
#endif
		using fun_wrapper = function_wrapper_with_allocator<alloc_type>;
		//using fun_wrapper = function_wrapper;
		using queue_type = typename queue_traits::template queue_type<fun_wrapper, std::allocator<fun_wrapper>, mutex_type>;
        using unique_atomic_bool = std::unique_ptr<std::atomic<bool>>;
        template <typename T>
        using unique_lock = typename Traits::template unique_lock<T>;

        void worker_thread(std::uint32_t idx)
        {
            if (m_onThreadStart)
                m_onThreadStart();
            m_nThreads.fetch_add(1, std::memory_order_relaxed);
            m_active.fetch_add(1, std::memory_order_relaxed);
            STK_SCOPE_EXIT(
                m_nThreads.fetch_sub(1, std::memory_order_relaxed);
                m_active.fetch_sub(1, std::memory_order_relaxed);
                if (m_onThreadStop)
                    m_onThreadStop();
            );

            fun_wrapper task;
            std::uint32_t spincount = 0;
            bool hasTasks = queue_traits::try_pop(m_tasks, task);
            while (true)
            {
                if(hasTasks)
                {
                    task();
					if (BOOST_LIKELY(!m_stop[idx]->load(std::memory_order_relaxed)))
					{
						spincount = 0;
						hasTasks = queue_traits::try_pop(m_tasks, task);
					}
					else
						return;
                }
				else if (++spincount < 100)
                {
                    auto backoff = spincount * 10;
                    while (backoff--)
                        thread_traits::yield();
					if (BOOST_LIKELY(!m_stop[idx]->load(std::memory_order_relaxed)))
                        hasTasks = queue_traits::try_pop(m_tasks, task);
					else
						return;
                }
                else
                {
                    m_active.fetch_sub(1, std::memory_order_relaxed);
                    {
                        unique_lock<mutex_type> lk{ m_mutex };
                        m_cnd.wait(lk, [&task, &hasTasks, idx, this]() { return (hasTasks = queue_traits::try_pop(m_tasks, task)) || m_stop[idx]->load(std::memory_order_relaxed) || m_done.load(std::memory_order_relaxed); });
                    }
                    m_active.fetch_add(1, std::memory_order_relaxed);
                    if (!hasTasks)
                        return;
                }
            }
        }

    public:

        template <typename T>
        using future = typename Traits::template future_type<T>;

        thread_pool(std::uint32_t nThreads = std::thread::hardware_concurrency() - 1)
        {
            init(nThreads);
        }

        thread_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, std::uint32_t nthreads = std::thread::hardware_concurrency() - 1)
            : m_onThreadStart(onThreadStart)
            , m_onThreadStop(onThreadStop)
        {
            init(nthreads);
        }

        ~thread_pool()
        {
            set_done(true);

            //while (number_threads())
            {
                auto lk = unique_lock<mutex_type>{ m_mutex };
                m_cnd.notify_all();
            }

            boost::for_each(m_threads, [](thread_type& t) { if (t.joinable()) t.join(); });
        }

        template <typename Action>
        future<decltype(std::declval<Action>()())> send(Action&& x)
        {
            return send_impl(std::forward<Action>(x));
        }

        std::size_t number_threads() const noexcept { return m_nThreads.load(std::memory_order_relaxed); }

        template <typename Pred>
        void wait_for(Pred&& pred) noexcept
        {
            static_assert(noexcept(pred()), "Pred must be noexcept.");
            fun_wrapper task;
            while (!pred())
            {
                if (queue_traits::try_pop(m_tasks, task))
                {
                    task();
                }
            }
        }

		void wait_or_work(std::vector<future<void>>&fs) noexcept
		{
			fun_wrapper tsk;
			std::uint32_t lastIndexHit = 0;
			for (auto it = fs.begin(); it != fs.end();)
			{
				if (!thread_traits::is_ready(*it))
				{
					if (queue_traits::try_pop(m_tasks, tsk))
						tsk();
				}
				else
					++it;
			}
		}

        template <typename Action>
        void send_no_future(Action&& m) noexcept
        {
            fun_wrapper p(std::forward<Action>(m));
			if (!queue_traits::try_push(m_tasks, std::move(p)))
			{
				p();
				return;
			}

            unique_lock<mutex_type> lk{ m_mutex };
            m_cnd.notify_one();
        }

		template <typename Range, typename TaskFn>
		void parallel_for(Range&& range, TaskFn&& task) noexcept
		{
			auto nthreads = number_threads();
			auto npartitions = nthreads * (nthreads-1);
			using value_t = typename boost::range_value<Range>::type;
			parallel_for_impl(std::forward<Range>(range), std::forward<TaskFn>(task), npartitions, std::integral_constant<bool, noexcept(task(std::declval<value_t>()))>());
		}

		template <typename TaskFn>
		void parallel_apply(std::ptrdiff_t count, TaskFn&& task) noexcept
		{
			auto nthreads = number_threads();
			auto npartitions = nthreads * (nthreads-1);
			parallel_apply_impl(count, std::forward<TaskFn>(task), npartitions, std::integral_constant<bool, noexcept(task(0))>());
		}

		template <typename Range, typename TaskFn>
		void parallel_for(Range&& range, TaskFn&& task, std::size_t npartitions) noexcept
		{
			using value_t = typename boost::range_value<Range>::type;
			parallel_for_impl(std::forward<Range>(range), std::forward<TaskFn>(task), npartitions, std::integral_constant<bool, noexcept(task(std::declval<value_t>()))>());
		}

		template <typename TaskFn>
		void parallel_apply(std::ptrdiff_t count, TaskFn&& task, std::size_t npartitions) noexcept
		{
			parallel_apply_impl(count, std::forward<TaskFn>(task), npartitions, std::integral_constant<bool, noexcept(task(0))>());
		}

    private:

        void set_done(bool v)
        {
            m_done.store(v, std::memory_order_relaxed);
            for (auto& b : m_stop)
                b->store(v);
        }

        void init(std::uint32_t nThreads)
        {
            GEOMETRIX_ASSERT(nThreads > 1);//! why 1?
            try
            {
				for (std::uint32_t i = 0; i < nThreads; ++i)
				{
					m_stop.emplace_back(new std::atomic<bool>{ false });
				}

                m_threads.reserve(nThreads);
                for (std::uint32_t i = 0; i < nThreads; ++i)
                    m_threads.emplace_back([i, this]() { worker_thread(i); });

                while (number_threads() != m_threads.size())
                    thread_traits::yield();
            }
            catch (...)
            {
                set_done(true);
                throw;
            }
        }

        template <typename Action>
        future<decltype(std::declval<Action>()())> send_impl(Action&& m)
        {
            using result_type = decltype(m());
            packaged_task<result_type> task(std::forward<Action>(m));
            auto result = task.get_future();
            fun_wrapper p(std::move(task));
			if (!queue_traits::try_push(m_tasks, std::move(p)))
			{
				p();
				return std::move(result);
			}

            unique_lock<mutex_type> lk{ m_mutex };
            m_cnd.notify_one();
            return std::move(result);
        }

		template <typename Range, typename TaskFn>
		void parallel_for_impl(Range&& range, TaskFn&& task, std::size_t npartitions, std::false_type) noexcept
		{
			using iterator_t = typename boost::range_iterator<Range>::type;
			std::vector<future<void>> fs;
			fs.reserve(npartitions);
			partition_work(range, npartitions,
				[&fs, &task, this](iterator_t from, iterator_t to) -> void
			{
				fs.emplace_back(send([&task, from, to]() -> void
				{
					for (auto i = from; i != to; ++i)
					{
						task(*i);
					}
				}));
			}
			);

			wait_or_work(fs);
		}

		template <typename TaskFn>
		void parallel_apply_impl(std::ptrdiff_t count, TaskFn&& task, std::size_t npartitions, std::false_type) noexcept
		{
			std::vector<future<void>> fs;
			fs.reserve(npartitions);
			partition_work(count, npartitions,
				[&fs, &task, this](std::ptrdiff_t from, std::ptrdiff_t to) -> void
				{
					fs.emplace_back(send([&task, from, to]() -> void
					{
						for (auto i = from; i != to; ++i)
						{
							task(i);
						}
					}));
				}
			);

			wait_or_work(fs);
		}

		template <typename Range, typename TaskFn>
		void parallel_for_impl(Range&& range, TaskFn&& task, std::size_t npartitions, std::true_type) noexcept
		{
			using value_t = typename boost::range_value<Range>::type;
			static_assert(noexcept(task(std::declval<value_t>())), "call to parallel_for_noexcept must have noexcept task");
			using iterator_t = typename boost::range_iterator<Range>::type;
			std::atomic<std::uint32_t> consumed{ 0 };
			std::uint32_t njobs = 0;
			partition_work(range, npartitions,
				[&consumed, &njobs, &task, this](iterator_t from, iterator_t to) -> void
				{
					++njobs;
					send_no_future([&consumed, &task, from, to]() noexcept -> void
					{
						STK_SCOPE_EXIT(consumed.fetch_add(1, std::memory_order_relaxed));
						for (auto i = from; i != to; ++i)
						{
							task(*i);
						}
					});
				}
			);

			wait_for([&consumed, njobs]() noexcept { return consumed.load(std::memory_order_relaxed) == njobs; });
		}

		template <typename TaskFn>
		void parallel_apply_impl(std::ptrdiff_t count, TaskFn&& task, std::size_t npartitions, std::true_type) noexcept
		{
			static_assert(noexcept(task(0)), "call to parallel_apply_noexcept must have noexcept task");
			std::atomic<std::uint32_t> consumed{ 0 };
			std::uint32_t njobs = 0;
			partition_work(count, npartitions,
				[&consumed, &njobs, &task, this](std::ptrdiff_t from, std::ptrdiff_t to) -> void
				{
					++njobs;
					send_no_future([&consumed, &task, from, to]() noexcept -> void
					{
						STK_SCOPE_EXIT(consumed.fetch_add(1, std::memory_order_relaxed));
						for (auto i = from; i != to; ++i)
						{
							task(i);
						}
					});
				}
			);

			wait_for([&consumed, njobs]() noexcept { return consumed.load(std::memory_order_relaxed) == njobs; });
		}

        std::atomic<bool>               m_done{ false };
        std::atomic<std::uint32_t>      m_nThreads{ 0 };
        std::atomic<std::uint32_t>      m_active{ 0 };
        std::vector<unique_atomic_bool> m_stop;
        std::vector<thread_type>        m_threads;
        queue_type                      m_tasks;
        std::function<void()>           m_onThreadStart;
        std::function<void()>           m_onThreadStop;
        mutex_type                      m_mutex;
        condition_variable_type         m_cnd;
    };

}}//! namespace stk::thread;

#endif // STK_THREAD_WAITABLETHREADPOOL_HPP
