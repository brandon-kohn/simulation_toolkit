//
//! Copyright � 2017
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
#include <stk/thread/function_wrapper.hpp>
#include <stk/thread/barrier.hpp>
#include <stk/thread/scalable_task_counter.hpp>
#include <stk/thread/task_counter.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/partition_work.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <stk/thread/thread_local_pod.hpp>
#include <stk/utility/scope_exit.hpp>
#include <stk/utility/none.hpp>
#include <stk/thread/bind/bind_processor.hpp>
#include <stk/thread/cache_line_padding.hpp>
#include <stk/compiler/warnings.hpp>
#include <stk/utility/aligned_alloc.hpp>
#include <stk/container/experimental/detail/skip_list.hpp>
#ifdef STK_USE_JEMALLOC
#include <stk/utility/jemallocator.hpp>
#elif defined(STK_USE_RPMALLOC)
#include <stk/utility/rpmalloc_allocator.hpp>
#endif
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <algorithm>
#include <memory>

STK_WARNING_PUSH()
#define STK_DISABLE STK_WARNING_PADDED
#include STK_DO_DISABLE_WARNING()

namespace stk {
    namespace thread {

        //! Tag is used to allow customization of the type for better control over the static aspect of the threadpool thread ID/indices supplied via get.
        struct default_threadpool_tag
        {};

        template <typename QTraits = locked_queue_traits, typename Traits = boost_thread_traits, typename Tag = default_threadpool_tag>
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
#elif defined(STK_USE_RPMALLOC)
            using alloc_type = stk::rpmalloc_allocator<char>;
#else
            using alloc_type = std::allocator<char>;
#endif

            struct BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) padded_atomic_bool : std::atomic<bool>
            {
                padded_atomic_bool()
                    : std::atomic<bool>(false)
                {}
            };

            using fun_wrapper = function_wrapper_with_allocator<alloc_type>;
            using queue_type = typename queue_traits::template queue_type<fun_wrapper, std::allocator<fun_wrapper>, mutex_type>;
            using queue_info = typename queue_traits::queue_info;

            template <typename Q>
            struct BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) padded_queue : Q
            {
                padded_queue()
                    : Q{ 1024 }
                {}
            };

            template <typename T>
            using unique_lock = typename Traits::template unique_lock<T>;
            using counter = task_counter;

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

            void worker_thread(std::uint32_t tIndex, bool bindToProcs, std::false_type)//! Q has queue_info
            {
                if (bindToProcs)
                    bind_to_processor((tIndex + 1) % std::thread::hardware_concurrency());
                if (m_onThreadStart)
                    m_onThreadStart();

                m_nThreads.fetch_add(1, std::memory_order_relaxed);
                m_active.fetch_add(1, std::memory_order_relaxed);
                STK_SCOPE_EXIT(
                    m_active.fetch_sub(1, std::memory_order_relaxed);
                m_nThreads.fetch_sub(1, std::memory_order_relaxed);
                //m_spinning[tIndex].store(false, std::memory_order_relaxed);
                if (m_onThreadStop)
                    m_onThreadStop();
                );

                auto tid = tIndex + 1;
                get_thread_id() = tid;
                fun_wrapper task;
                std::uint32_t spincount = 0;
                std::uint32_t lastStolenIndex = tIndex;
                std::vector<queue_info> queueInfo;
                queueInfo.push_back(queue_traits::get_queue_info(m_poolQ));
                for (auto& q : m_localQs)
                    queueInfo.emplace_back(queue_traits::get_queue_info(q));

                bool hasTask = poll(queueInfo, tIndex, lastStolenIndex, task);
                while (true) {
                    if (hasTask) {
                        try {
                            task();
                            m_nTasksOutstanding.decrement(tid);
                        } catch (...) {
                        }//! TODO: Could collect there and let users iterate over exceptions collected to handle?

                        if (BOOST_LIKELY(!m_stopThread[tIndex].load(std::memory_order_relaxed))) {
                            spincount = 0;
                            hasTask = poll(queueInfo, tIndex, lastStolenIndex, task);
                        } else
                            return;
                    } else if (++spincount < 100) {
                        auto backoff = spincount * 10;
                        while (backoff--) {
                            thread_traits::yield();//! yield works better for larger payloads.
                        }
                        if (BOOST_LIKELY(!m_stopThread[tIndex].load(std::memory_order_relaxed)))
                            hasTask = poll(queueInfo, tIndex, lastStolenIndex, task);
                        else
                            return;
                    } else {
                        m_active.fetch_sub(1, std::memory_order_relaxed);
                        {
                            auto lk = unique_lock<mutex_type>{ m_pollingMtx };
                            m_pollingCnd.wait(lk, [&queueInfo, tIndex, &lastStolenIndex, &hasTask, &task, this]()
                            {
                                return (hasTask = poll(queueInfo, tIndex, lastStolenIndex, task)) || m_stopThread[tIndex].load(std::memory_order_relaxed) || m_done.load(std::memory_order_relaxed);
                            });
                        }
                        m_active.fetch_add(1, std::memory_order_relaxed);
                        if (!hasTask)
                            return;
                    }
                }
                return;
            }

            void worker_thread(std::uint32_t tIndex, bool bindToProcs, std::true_type) //! Q has void queue_info.
            {
                if (bindToProcs)
                    bind_to_processor((tIndex + 1) % std::thread::hardware_concurrency());
                if (m_onThreadStart)
                    m_onThreadStart();

                m_nThreads.fetch_add(1, std::memory_order_relaxed);
                m_active.fetch_add(1, std::memory_order_relaxed);
                STK_SCOPE_EXIT(
                    m_active.fetch_sub(1, std::memory_order_relaxed);
                m_nThreads.fetch_sub(1, std::memory_order_relaxed);
                //m_spinning[tIndex].store(false, std::memory_order_relaxed);
                if (m_onThreadStop)
                    m_onThreadStop();
                );

                auto tid = tIndex + 1;
                get_thread_id() = tid;
                fun_wrapper task;
                std::uint32_t spincount = 0;
                std::uint32_t lastStolenIndex = tIndex;
                bool hasTask = poll(tIndex, lastStolenIndex, task);
                while (true) {
                    if (hasTask) {
                        try {
                            task();
                            m_nTasksOutstanding.decrement(tid);
                        } catch (...) {
                            GEOMETRIX_ASSERT(false);//! Tasks are supposed to be BOOST_NOEXCEPT.
                        }//! TODO: Could collect there and let users iterate over exceptions collected to handle?

                        if (BOOST_LIKELY(!m_stopThread[tIndex].load(std::memory_order_relaxed))) {
                            spincount = 0;
                            hasTask = poll(tIndex, lastStolenIndex, task);
                        } else
                            return;
                    } else if (++spincount < 100) {
                        auto backoff = spincount * 10;
                        while (backoff--) {
                            thread_traits::yield();//! yield works better for larger payloads.
                        }
                        if (BOOST_LIKELY(!m_stopThread[tIndex].load(std::memory_order_relaxed)))
                            hasTask = poll(tIndex, lastStolenIndex, task);
                        else
                            return;
                    } else {
                        m_active.fetch_sub(1, std::memory_order_relaxed);
                        {
                            auto lk = unique_lock<mutex_type>{ m_pollingMtx };
                            m_pollingCnd.wait(lk, [tIndex, &lastStolenIndex, &hasTask, &task, this]()
                            {
                                return ((hasTask = poll(tIndex, lastStolenIndex, task)) == true) || m_stopThread[tIndex].load(std::memory_order_relaxed) || m_done.load(std::memory_order_relaxed);
                            });
                        }
                        m_active.fetch_add(1, std::memory_order_relaxed);
                        if (!hasTask)
                            return;
                    }
                }
                return;
            }

            bool poll(std::vector<queue_info>& queueInfo, std::uint32_t tIndex, std::uint32_t& lastStolenIndex, fun_wrapper& task)
            {
                auto r = pop_local_queue_task(queueInfo, tIndex, task) || pop_task_from_pool_queue(queueInfo[0], task) || try_steal(queueInfo, lastStolenIndex, task);
                //m_spinning[tIndex].store(!r, std::memory_order_relaxed);
                return r;
            }

            bool pop_local_queue_task(std::vector<queue_info>& queueInfo, std::uint32_t i, fun_wrapper& task)
            {
                auto r = queue_traits::try_pop(m_localQs[i], queueInfo[i + 1], task);
                return r;
            }

            bool pop_task_from_pool_queue(queue_info& queueInfo, fun_wrapper& task)
            {
                return queue_traits::try_steal(m_poolQ, queueInfo, task);
            }

            bool try_steal(std::vector<queue_info>& queueInfo, std::uint32_t lastStolenIndex, fun_wrapper& task)
            {
                auto qSize = m_localQs.size();
                std::uint32_t count = 0;
                for (std::uint32_t i = lastStolenIndex; count < qSize; i = (i + 1) % qSize, ++count) {
                    if (queue_traits::try_steal(m_localQs[i], queueInfo[i + 1], task)) {
                        lastStolenIndex = i;
                        return true;
                    }
                }

                return false;
            }

            bool poll(std::uint32_t tIndex, std::uint32_t& lastStolenIndex, fun_wrapper& task)
            {
                auto r = pop_local_queue_task(tIndex, task) || pop_task_from_pool_queue(task) || try_steal(lastStolenIndex, task);
                //m_spinning[tIndex].store(!r, std::memory_order_relaxed);
                return r;
            }

            bool pop_local_queue_task(std::uint32_t i, fun_wrapper& task)
            {
                auto r = queue_traits::try_pop(m_localQs[i], task);
                return r;
            }

            bool pop_task_from_pool_queue(fun_wrapper& task)
            {
                return queue_traits::try_steal(m_poolQ, task);
            }

            bool try_steal(std::uint32_t lastStolenIndex, fun_wrapper& task)
            {
                auto qSize = m_localQs.size();
                std::uint32_t count = 0;
                for (std::uint32_t i = lastStolenIndex; count < qSize; i = (i + 1) % qSize, ++count) {
                    if (queue_traits::try_steal(m_localQs[i], task)) {
                        lastStolenIndex = i;
                        return true;
                    }
                }

                return false;
            }

            void set_done(bool v)
            {
                m_done.store(v, std::memory_order_relaxed);
                for (auto& b : m_stopThread)
                    b.store(v, std::memory_order_relaxed);
            }

        public:

            template <typename T>
            using future = typename thread_traits::template future_type<T>;

            template <typename T>
            static bool is_ready( const future<T>& f )
			{
				return thread_traits::is_ready( f );
			}

            work_stealing_thread_pool(std::uint32_t nthreads = boost::thread::hardware_concurrency() - 1, bool bindToProcs = false)
                : m_threads(nthreads)
                , m_stopThread(nthreads)
                //, m_spinning(nthreads)
                , m_poolQ(1024)
                , m_localQs(nthreads)
                , m_nTasksOutstanding(nthreads + 1)
            {
                init(bindToProcs);
            }

            work_stealing_thread_pool(std::function<void()> onThreadStart, std::function<void()> onThreadStop, std::uint32_t nthreads = boost::thread::hardware_concurrency() - 1, bool bindToProcs = false)
                : m_threads(nthreads)
                , m_stopThread(nthreads)
                //, m_spinning(nthreads)
                , m_poolQ(1024)
                , m_localQs(nthreads)
                , m_nTasksOutstanding(nthreads + 1)
                , m_onThreadStart(onThreadStart)
                , m_onThreadStop(onThreadStop)
            {
                init(bindToProcs);
            }

            ~work_stealing_thread_pool()
            {
                //while (number_threads()) 
				{
                    auto lk = unique_lock<mutex_type>{ m_pollingMtx };
					set_done(true);
	                m_pollingCnd.notify_all();
                }

                //GEOMETRIX_ASSERT(number_threads() == 0);

                for (auto& t : m_threads) {
                    if (t.joinable()) {
//#ifdef BOOST_MSVC
//                        t.detach();//! Joining here hangs on some platforms. Detaching should be safe as the pool is done.
//#else
                        t.join();
//#endif
                    }
                }
            }

            template <typename Task>
            future<decltype(std::declval<Task>()())> send(Task&& x) BOOST_NOEXCEPT
            {
                return send_impl(get_thread_id(), std::forward<Task>(x));
            }

            //! Send a task to the specified thread index. Thread indices are in the range of [1, nthreads]. 0 is the pool queue.
            template <typename Task>
            future<decltype(std::declval<Task>()())> send(std::uint32_t threadQueueIndex, Task&& x) BOOST_NOEXCEPT
            {
                GEOMETRIX_ASSERT(threadQueueIndex < number_threads() + 1);
                return send_impl(threadQueueIndex, std::forward<Task>(x));
            }

            template <typename Task>
            void send_no_future(Task&& x) BOOST_NOEXCEPT
            {
                return send_no_future_impl(get_thread_id(), std::forward<Task>(x));
            }

            //! Send a task to the specified thread index. Thread indices are in the range of [1, nthreads]. 0 is the pool queue.
            template <typename Task>
            void send_no_future(std::uint32_t threadQueueIndex, Task&& x) BOOST_NOEXCEPT
            {
                GEOMETRIX_ASSERT(threadQueueIndex < number_threads() + 1);
                return send_no_future_impl(threadQueueIndex, std::forward<Task>(x));
            }

            template <typename Range, typename TaskFn>
            void parallel_for(Range&& range, TaskFn&& task) BOOST_NOEXCEPT
            {
                auto nthreads = number_threads();
                auto npartitions = nthreads * nthreads;
                using value_t = typename boost::range_value<Range>::type;
#ifndef BOOST_NO_CXX11_NOEXCEPT
                parallel_for_impl(std::forward<Range>(range), std::forward<TaskFn>(task), nthreads, npartitions, std::integral_constant<bool, BOOST_NOEXCEPT(task(std::declval<value_t>()))>());
#else
                parallel_for_impl(std::forward<Range>(range), std::forward<TaskFn>(task), nthreads, npartitions, std::false_type());
#endif
            }

            template <typename TaskFn>
            void parallel_apply(std::ptrdiff_t count, TaskFn&& task) BOOST_NOEXCEPT
            {
                auto nthreads = number_threads();
                auto npartitions = nthreads * nthreads;
#ifndef BOOST_NO_CXX11_NOEXCEPT
                parallel_apply_impl(count, std::forward<TaskFn>(task), nthreads, npartitions, std::integral_constant<bool, BOOST_NOEXCEPT(task(0))>());
#else
                parallel_apply_impl(count, std::forward<TaskFn>(task), nthreads, npartitions, std::false_type());
#endif
            }

            template <typename Range, typename TaskFn>
            void parallel_for(Range&& range, TaskFn&& task, std::size_t npartitions) BOOST_NOEXCEPT
            {
                using value_t = typename boost::range_value<Range>::type;
#ifndef BOOST_NO_CXX11_NOEXCEPT
                parallel_for_impl(std::forward<Range>(range), std::forward<TaskFn>(task), number_threads(), npartitions, std::integral_constant<bool, BOOST_NOEXCEPT(task(std::declval<value_t>()))>());
#else
                parallel_for_impl(std::forward<Range>(range), std::forward<TaskFn>(task), number_threads(), npartitions, std::false_type());
#endif
            }

            template <typename TaskFn>
            void parallel_apply(std::ptrdiff_t count, TaskFn&& task, std::size_t npartitions) BOOST_NOEXCEPT
            {
#ifndef BOOST_NO_CXX11_NOEXCEPT
                parallel_apply_impl(count, std::forward<TaskFn>(task), number_threads(), npartitions, std::integral_constant<bool, BOOST_NOEXCEPT(task(0))>());
#else
                parallel_apply_impl(count, std::forward<TaskFn>(task), number_threads(), npartitions, std::false_type());
#endif
            }

            std::uint32_t number_threads() const BOOST_NOEXCEPT
            {
                return m_nThreads.load(std::memory_order_relaxed);
            }

            void wait_for_all_tasks() BOOST_NOEXCEPT
            {
                auto pred = [this]() BOOST_NOEXCEPT{ return !has_outstanding_tasks(); };
                wait_for(pred);
            }

            template <typename Pred>
            void wait_for(Pred&& pred) BOOST_NOEXCEPT
            {
#ifndef BOOST_NO_CXX11_NOEXCEPT
                static_assert(BOOST_NOEXCEPT(pred()), "Pred must be BOOST_NOEXCEPT.");
#endif
                auto tid = get_thread_id();
                fun_wrapper task;
                std::uint32_t lastStolenIndex = 0;
                while (!pred()) {
                    if (pop_task_from_pool_queue(task) || try_steal(lastStolenIndex, task)) {
                        task();
                        m_nTasksOutstanding.decrement(tid);
                    }
                }
            }

            void do_work() BOOST_NOEXCEPT
            {
                auto tid = get_thread_id();
                fun_wrapper tsk;
				auto        lastStolenIndex = std::uint32_t{};
				do_work_impl( tsk, lastStolenIndex, tid );
            }
            
            template <typename Futures>
            void wait_or_work(Futures&&fs) BOOST_NOEXCEPT
            {
                auto tid = get_thread_id();
                fun_wrapper tsk;
                std::uint32_t lastStolenIndex = 0;
                for (auto it = std::begin(fs); it != std::end(fs);) {
                    if (!is_ready(*it)) {
						do_work_impl( tsk, lastStolenIndex, tid );
                    } else
                        ++it;
                }
            }

            //! Return the index of the first thread found spinning. The result is the index + 1. If 0 is returned, then none are spinning.
    //      std::uint32_t get_spinning_index() const
    //      {
    //          for (std::uint32_t i = 0; i < m_spinning.size(); ++i)
    //              if (m_spinning[i].load(std::memory_order_relaxed))
    //                  return i + 1;
    //          return 0;
    //      }

            std::uint32_t get_rnd_queue_index() const
            {
                static STK_THREAD_LOCAL_POD std::size_t id = 0;
                auto idx = (++id % (m_threads.size() - 1)) + 1;
                GEOMETRIX_ASSERT(idx > 0 && idx < m_threads.size());
                return idx;
            }

            //! If the thread is in the pool the id will be 1..N. Subtract 1 to get the local queue index. If the thread is not in the pool return 0. Usually 0 should be the main thread.
            static std::uint32_t& get_thread_id() BOOST_NOEXCEPT
            {
                static STK_THREAD_LOCAL_POD std::uint32_t id = 0;
                return id;
            }

            bool has_outstanding_tasks() const BOOST_NOEXCEPT
            {
                return m_nTasksOutstanding.count() != 0;
            }

			void* operator new(std::size_t i)
			{
				return stk::aligned_alloc(i, STK_CACHE_LINE_SIZE);
			}

			void operator delete(void* p)
			{
				return stk::free(p);
			}

        private:

            void do_work_impl(fun_wrapper& tsk, std::uint32_t& lastStolenIndex, std::uint32_t tid ) BOOST_NOEXCEPT
            {
				if( pop_task_from_pool_queue( tsk ) || try_steal( lastStolenIndex, tsk ) ) {
					tsk();
					m_nTasksOutstanding.decrement( tid );
				}
            }

            void init(bool bindToProcs)
            {
                try {
                    if (bindToProcs)
                        bind_to_processor(0);

                    using qinfo_is_void = typename stk::is_none<queue_info>::type;
                    for (std::uint32_t i = 0; i < m_threads.size(); ++i)
                        m_threads[i] = thread_type([i, bindToProcs, this]()
                    {
                        worker_thread(i, bindToProcs, qinfo_is_void());
                    });

                    while (number_threads() != m_threads.size())
                        thread_traits::yield();
                } catch (...) {
                    set_done(true);
                    throw;
                }
            }

            template <typename Range, typename TaskFn>
            void parallel_for_impl(Range&& range, TaskFn&& task, std::size_t nthreads, std::size_t npartitions, std::false_type) BOOST_NOEXCEPT
            {
                using iterator_t = typename boost::range_iterator<Range>::type;
                std::vector<future<void>> fs;
                fs.reserve(npartitions);
                std::size_t njobs = 0;
                partition_work(range, npartitions,
                               [nthreads, &njobs, &fs, &task, this](iterator_t from, iterator_t to) -> void
                {
                    //std::uint32_t threadID = get_rnd_queue_index();//get_spinning_index();
                    //std::uint32_t threadID = get_spinning_index();
                    std::uint32_t threadID = static_cast<std::uint32_t>(++njobs % nthreads + 1);
                    fs.emplace_back(send(threadID, [&task, from, to]() -> void
                    {
                        for (auto i = from; i != to; ++i) {
                            task(*i);
                        }
                    }));
                }
                );

                wait_or_work(fs);
            }

            template <typename TaskFn>
            void parallel_apply_impl(std::ptrdiff_t count, TaskFn&& task, std::size_t nthreads, std::size_t npartitions, std::false_type) BOOST_NOEXCEPT
            {
                std::vector<future<void>> fs;
                fs.reserve(npartitions);
                std::size_t njobs = 0;
                partition_work(count, npartitions,
                               [nthreads, &njobs, &fs, &task, this](std::ptrdiff_t from, std::ptrdiff_t to) -> void
                {
                    std::uint32_t threadID = static_cast<std::uint32_t>(++njobs % nthreads + 1);
                    fs.emplace_back(send(threadID, [&task, from, to]() -> void
                    {
                        for (auto i = from; i != to; ++i) {
                            task(i);
                        }
                    }));
                }
                );

                wait_or_work(fs);
            }

            template <typename Range, typename TaskFn>
            void parallel_for_impl(Range&& range, TaskFn&& task, std::size_t nthreads, std::size_t npartitions, std::true_type) BOOST_NOEXCEPT
            {
                using value_t = typename boost::range_value<Range>::type;

#ifndef BOOST_NO_CXX11_NOEXCEPT
                static_assert(BOOST_NOEXCEPT(task(std::declval<value_t>())), "call to parallel_for must have BOOST_NOEXCEPT task");
#endif
                using iterator_t = typename boost::range_iterator<Range>::type;
                counter consumed(nthreads + 1);
                std::uint32_t njobs = 0;
                partition_work(range, npartitions,
                               [nthreads, &consumed, &njobs, &task, this](iterator_t from, iterator_t to) -> void
                {
                    ++njobs;
                    std::uint32_t threadID = njobs % nthreads + 1;
                    send_no_future(threadID, [&consumed, &task, from, to]() BOOST_NOEXCEPT -> void
                                   {
                                       STK_SCOPE_EXIT(consumed.increment(get_thread_id()));
                                       for (auto i = from; i != to; ++i) {
                                           task(*i);
                                       }
                                   });
                }
                );

                wait_for([&consumed, njobs]() BOOST_NOEXCEPT{ return consumed.count() == njobs; });
            }

            template <typename TaskFn>
            void parallel_apply_impl(std::ptrdiff_t count, TaskFn&& task, std::size_t nthreads, std::size_t npartitions, std::true_type) BOOST_NOEXCEPT
            {
#ifndef BOOST_NO_CXX11_NOEXCEPT
                static_assert(BOOST_NOEXCEPT(task(0)), "call to parallel_apply_BOOST_NOEXCEPT must have BOOST_NOEXCEPT task");
#endif
                task_counter consumed(nthreads + 1);
                std::uint32_t njobs = 0;
                partition_work(count, npartitions,
                               [nthreads, &consumed, &njobs, &task, this](std::ptrdiff_t from, std::ptrdiff_t to) -> void
                {
                    ++njobs;
                    std::uint32_t threadID = njobs % nthreads + 1;
                    send_no_future(threadID, [&consumed, &task, from, to]() BOOST_NOEXCEPT -> void
                                   {
                                       STK_SCOPE_EXIT(consumed.increment(get_thread_id()));
                                       for (auto i = from; i != to; ++i) {
                                           task(i);
                                       }
                                   });
                }
                );

                wait_for([&consumed, njobs]() BOOST_NOEXCEPT{ return consumed.count() == njobs; });
            }

            template <typename Task>
            future<decltype(std::declval<Task>()())> send_impl(std::uint32_t threadQueueIndex, Task&& m) BOOST_NOEXCEPT
            {
                using result_type = decltype(m());
                auto tid = get_thread_id();
                m_nTasksOutstanding.increment(tid);
                packaged_task<result_type> task(std::forward<Task>(m));
                auto result = task.get_future();

                fun_wrapper p(std::move(task));
                if (threadQueueIndex) {
                    if (!queue_traits::try_push(m_localQs[threadQueueIndex - 1], std::move(p))) {
                        p();
                        m_nTasksOutstanding.decrement(tid);
                        return std::move(result);
                    }
                } else {
                    if (!queue_traits::try_push(m_poolQ, std::move(p))) {
                        p();
                        m_nTasksOutstanding.decrement(tid);
                        return std::move(result);
                    }
                }

                {
                    //auto lk = unique_lock<mutex_type>{ m_pollingMtx };
                    m_pollingCnd.notify_one();
                }

                return std::move(result);
            }

            template <typename Task>
            void send_no_future_impl(std::uint32_t threadQueueIndex, Task&& m) BOOST_NOEXCEPT
            {
#ifndef BOOST_NO_CXX11_NOEXCEPT
                static_assert(BOOST_NOEXCEPT(m()), "call to send_no_future must have BOOST_NOEXCEPT task");
#endif
                auto tid = get_thread_id();
                m_nTasksOutstanding.increment(tid);
                fun_wrapper p(std::forward<Task>(m));
                if (threadQueueIndex) {
                    if (!queue_traits::try_push(m_localQs[threadQueueIndex - 1], std::move(p))) {
                        p();
                        m_nTasksOutstanding.decrement(tid);
                        return;
                    }
                } else {
                    if (!queue_traits::try_push(m_poolQ, std::move(p))) {
                        p();
                        m_nTasksOutstanding.decrement(tid);
                        return;
                    }
                }

                //auto lk = unique_lock<mutex_type>{ m_pollingMtx };
                m_pollingCnd.notify_one();
            }

            std::vector<thread_type>                                                   m_threads;

            BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) std::vector<padded_atomic_bool>       m_stopThread;
            //BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) std::vector<padded_atomic_bool>       m_spinning;
            BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) std::atomic<bool>                     m_done{ false };
            BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) queue_type                            m_poolQ;
            BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) std::vector<padded_queue<queue_type>> m_localQs;
            BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) std::atomic<std::uint32_t>            m_active{ 0 };
            BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) std::atomic<std::uint32_t>            m_nThreads{ 0 };
            BOOST_ALIGNMENT(STK_CACHE_LINE_SIZE) scalable_task_counter                 m_nTasksOutstanding;

            std::function<void()>                                              m_onThreadStart;
            std::function<void()>                                              m_onThreadStop;
            mutex_type                                                         m_pollingMtx;
            condition_variable_type                                            m_pollingCnd;
        };

    }
}//! namespace stk::thread;
STK_WARNING_POP()

#endif // STK_THREAD_WORK_STEALING_THREAD_POOL_HPP
