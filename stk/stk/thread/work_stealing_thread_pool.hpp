//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
#define STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
#pragma once
#include <stk/thread/function_wrapper_with_allocator.hpp>
#include <stk/thread/barrier.hpp>
#include <stk/thread/thread_specific.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/partition_work.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/utility/scope_exit.hpp>
#include <stk/thread/pool_polling_mode.hpp>
#ifdef STK_USE_JEMALLOC
#include <stk/utility/jemallocator.hpp>
#endif
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <algorithm>
#include <memory>

namespace stk { namespace thread {

    template <typename QTraits = locked_queue_traits, typename Traits = boost_thread_traits>
    class work_stealing_thread_pool : boost::noncopyable
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
        using queue_type = typename queue_traits::template queue_type<fun_wrapper, std::allocator<fun_wrapper>, mutex_type>;
        using queue_ptr = std::unique_ptr<queue_type>;
        using unique_atomic_bool = std::unique_ptr<std::atomic<bool>>;

        template <typename T>
        using unique_lock = typename Traits::template unique_lock<T>;

        static stk::detail::random_xor_shift_generator create_generator()
        {
            std::random_device rd;
            return stk::detail::random_xor_shift_generator(rd());
        }

        static std::uint32_t get_rnd()
        {
            static auto rnd = create_generator();
            return rnd();
        }

        void worker_thread(std::uint32_t tIndex)
        {
            if (m_onThreadStart)
                m_onThreadStart();

            m_nThreads.fetch_add(1, std::memory_order_relaxed);
            m_active.fetch_add(1, std::memory_order_relaxed);
            STK_SCOPE_EXIT(
                m_active.fetch_sub(1, std::memory_order_relaxed);
                m_nThreads.fetch_sub(1, std::memory_order_relaxed);
                if (m_onThreadStop)
                    m_onThreadStop();
            );

            get_local_index() = tIndex+1;
            fun_wrapper task;
            std::uint32_t spincount = 0;
            bool hasTask = poll(tIndex, task);
            while (true)
            {
                while(hasTask)
                {
                    task();
                    spincount = 0;
                    if (!m_stop[tIndex]->load(std::memory_order_relaxed))
                        hasTask = poll(tIndex, task);
                    else
                        return;
                }

                if (++spincount < 100)
                {
                    auto backoff = spincount * 10;
                    while (backoff--)
                        thread_traits::yield();

                    hasTask = poll(tIndex, task);
                }
                else
                {
                    m_active.fetch_sub(1, std::memory_order_relaxed);
                    {
                        auto lk = unique_lock<mutex_type>{ m_pollingMtx };
                        m_pollingCnd.wait(lk, [tIndex, &hasTask, &task, this]() {return (hasTask = poll(tIndex, task)) || m_done.load(std::memory_order_relaxed) || m_stop[tIndex]->load(std::memory_order_relaxed); });
                    }
                    m_active.fetch_add(1, std::memory_order_relaxed);
                    if (!hasTask)
                        return;
                }
            }
        }

        bool poll(std::uint32_t tIndex, fun_wrapper& task)
        {
            return pop_local_queue_task(tIndex, task) || pop_task_from_pool_queue(task) || try_steal(task);
        }

        bool pop_local_queue_task(std::uint32_t i, fun_wrapper& task)
        {
            return queue_traits::try_pop(*m_localQs[i], task);
        }

        bool pop_task_from_pool_queue(fun_wrapper& task)
        {
            return queue_traits::try_steal(m_poolQ, task);
        }

        bool try_steal(fun_wrapper& task)
        {
            for (std::uint32_t i = 0; i < m_localQs.size(); ++i)
            {
                if (queue_traits::try_steal(*m_localQs[i], task))
                    return true;
            }

            return false;
        }

        bool try_steal1(fun_wrapper& task)
        {
            auto victim = get_rnd() % m_localQs.size();
            if (queue_traits::try_steal(*m_localQs[victim], task))
                return true;

            return false;
        }

        void set_done(bool v)
        {
            m_done.store(v, std::memory_order_relaxed);
            for (auto& b : m_stop)
                b->store(v);
        }

    public:

        template <typename T>
        using future = typename thread_traits::template future_type<T>;

        work_stealing_thread_pool(std::uint32_t nthreads = boost::thread::hardware_concurrency()-1)
            : m_threads(nthreads)
            //, m_workQIndex([](){ return static_cast<std::uint32_t>(-1); })
            , m_poolQ(64*1024)
            , m_localQs(nthreads)
            , m_allocator()
        {
            init();
        }

        work_stealing_thread_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, std::uint32_t nthreads = boost::thread::hardware_concurrency()-1)
            : m_threads(nthreads)
            //, m_workQIndex([](){ return static_cast<std::uint32_t>(-1); })
            , m_poolQ(64*1024)
            , m_localQs(nthreads)
            , m_onThreadStart(onThreadStart)
            , m_onThreadStop(onThreadStop)
            , m_allocator()
        {
            init();
        }

        ~work_stealing_thread_pool()
        {
            set_done(true);

            while (m_active.load(std::memory_order_relaxed) != number_threads())
            {
                auto lk = unique_lock<mutex_type>{ m_pollingMtx };
                m_pollingCnd.notify_all();
            }

            boost::for_each(m_threads, [](thread_type& t)
            {
                if (t.joinable())
                    t.join();
            });
        }

        template <typename Action>
        future<decltype(std::declval<Action>()())> send(Action&& x)
        {
            return send_impl(get_local_index(), std::forward<Action>(x));
        }

        template <typename Action>
        future<decltype(std::declval<Action>()())> send(std::uint32_t threadIndex, Action&& x)
        {
            GEOMETRIX_ASSERT(threadIndex < number_threads());
            return send_impl(threadIndex+1, std::forward<Action>(x));
        }

        template <typename Action>
        void send_no_future(Action&& x)
        {
            return send_no_future_impl(get_local_index(), std::forward<Action>(x));
        }

        template <typename Action>
        void send_no_future(std::uint32_t threadIndex, Action&& x)
        {
            GEOMETRIX_ASSERT(threadIndex < number_threads());
            return send_no_future_impl(threadIndex + 1, std::forward<Action>(x));
        }

        template <typename Range, typename TaskFn>
        void parallel_for(Range&& range, TaskFn&& task)
        {
            auto nthreads = number_threads();
            auto npartitions = nthreads * nthreads;
            using iterator_t = typename boost::range_iterator<Range>::type;
            std::atomic<std::size_t> consumed = 0;
            partition_work(range, npartitions,
                [nthreads, &consumed, &task, this](iterator_t from, iterator_t to) -> void
                {
                    std::uint32_t threadID = get_rnd() % nthreads;
                    send_no_future(threadID, [&consumed, &task, from, to, this]() -> void
                    {
                        for (auto i = from; i != to; ++i)
                        {
                            task(*i);
                            consumed.fetch_add(1, std::memory_order_relaxed);
                        }
                    });
                }
            );

            auto njobs = boost::size(range);
            wait_for([&consumed, njobs]() { return consumed.load(std::memory_order_relaxed) == njobs; });
            GEOMETRIX_ASSERT(njobs == consumed.load());
        }

        std::size_t number_threads() const { return m_nThreads.load(std::memory_order_relaxed); }

        template <typename Pred>
        void wait_for(Pred&& pred)
        {
            fun_wrapper task;
            while (!pred())
            {
                if (pop_task_from_pool_queue(task) || try_steal(task))
                {
                    task();
                }
            }
        }

    private:

        std::uint32_t& get_local_index()
        {
            static thread_local std::uint32_t workQIndex;
            return workQIndex;
        }

        void init()
        {
            try
            {
                for (std::uint32_t i = 0; i < m_threads.size(); ++i)
                {
                    m_localQs[i] = queue_ptr(new queue_type(32*1024));
                    m_stop.emplace_back(new std::atomic<bool>{ false });
                }

                for (std::uint32_t i = 0; i < m_threads.size(); ++i)
                    m_threads[i] = thread_type([i, this]() { worker_thread(i); });

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
        future<decltype(std::declval<Action>()())> send_impl(std::uint32_t threadIndex, Action&& m)
        {
            GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);
            using result_type = decltype(m());
            packaged_task<result_type> task(std::forward<Action>(m));
            auto result = task.get_future();

            fun_wrapper p(m_allocator, std::move(task));
            if (threadIndex)// != static_cast<std::uint32_t>(-1))
            {
                while (!queue_traits::try_push(*m_localQs[threadIndex-1], std::move(p)))
                    thread_traits::yield();
            }
            else
            {
                while (!queue_traits::try_push(m_poolQ, std::move(p)))
                    thread_traits::yield();
            }

            {
                auto lk = std::unique_lock<mutex_type>{ m_pollingMtx };
                m_pollingCnd.notify_one();
            }
            return std::move(result);
        }

        template <typename Action>
        void send_no_future_impl(std::uint32_t localIndex, Action&& m)
        {
            GEOMETRIX_ASSERT(m_suspendPolling.load(std::memory_order_relaxed) == false);

            fun_wrapper p(m_allocator, std::forward<Action>(m));
            if (localIndex) //!= static_cast<std::uint32_t>(-1))
            {
                while (!queue_traits::try_push(*m_localQs[localIndex-1], std::move(p)))
                    thread_traits::yield();
            }
            else
            {
                while (!queue_traits::try_push(m_poolQ, std::move(p)))
                    thread_traits::yield();
            }

            auto lk = std::unique_lock<mutex_type>{ m_pollingMtx };
            m_pollingCnd.notify_one();
        }

        std::vector<thread_type>       m_threads;
        std::vector<unique_atomic_bool>m_stop;
        std::atomic<bool>              m_done{ false };
        //thread_specific<std::uint32_t> m_workQIndex;
        queue_type                     m_poolQ;
        std::vector<queue_ptr>         m_localQs;
        std::function<void()>          m_onThreadStart;
        std::function<void()>          m_onThreadStop;
        std::atomic<std::uint32_t>     m_active{ 0 };
        std::atomic<std::uint32_t>     m_nThreads{ 0 };
        mutex_type                     m_pollingMtx;
        condition_variable_type        m_pollingCnd;
        alloc_type                     m_allocator;
    };

}}//! namespace stk::thread;

#endif // STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
