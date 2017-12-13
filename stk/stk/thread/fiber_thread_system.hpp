
#ifndef STK_THREAD_FIBER_THREAD_SYSTEM_HPP
#define STK_THREAD_FIBER_THREAD_SYSTEM_HPP
#pragma once

#include <stk/thread/function_wrapper.hpp>
#include <stk/container/locked_queue.hpp>
#include <stk/thread/atomic_spin_lock.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <boost/noncopyable.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/fiber/all.hpp>
#include <cstdint>

#ifndef STK_NO_FIBER_POOL_BIND_TO_PROCESSOR
#include <stk/thread/bind/bind_processor.hpp>
#endif

namespace stk { namespace thread {

template <typename FiberAlgo = boost::fibers::algo::shared_work, typename Traits = boost_thread_traits>
class fiber_thread_system : boost::noncopyable
{
    using thread_traits = Traits;
    using thread_type = typename thread_traits::thread_type;
    using fiber_algo_type = FiberAlgo;
    
    void os_thread(unsigned int idx)
    {
        //! thread registers itself at work-stealing scheduler
        auto nThreads = m_threads.size() + 1;
#ifndef STK_NO_FIBER_POOL_BIND_TO_PROCESSOR
        bind_to_processor(idx);
#endif
        boost::fibers::use_scheduling_algorithm<fiber_algo_type>(/*nThreads*/);
        lock_type lk{m_mtx};
        m_cnd.wait(lk, [this](){ return m_done.load(); });
        GEOMETRIX_ASSERT(m_done);
    }

public:

    fiber_thread_system(unsigned int nOSThreads = std::thread::hardware_concurrency())
        : m_done(false)
        , m_threads(nOSThreads-1)
    {
        GEOMETRIX_ASSERT(nOSThreads > 1);//1 thread? why?
        try
        {
            //! register calling thread.
            auto nCPUS = std::thread::hardware_concurrency();
#ifndef STK_NO_FIBER_POOL_BIND_TO_PROCESSOR
            bind_to_processor(0);
#endif
            boost::fibers::use_scheduling_algorithm<boost::fibers::algo::shared_work>(/*nOSThreads*/);
            for(unsigned int i = 1; i < nOSThreads; ++i)
                m_threads[i-1] = thread_type([i, nCPUS, this](){ os_thread(i % nCPUS); });
        }
        catch(...)
        {
            shutdown();
            throw;
        }
    }

    ~fiber_thread_system()
    {
        shutdown();
    }

private:

    virtual void shutdown()
    {
        bool b = false;
        if( m_done.compare_exchange_strong(b, true) )
        {
            m_cnd.notify_all();
            boost::for_each(m_threads, [](thread_type& t){ t.join(); });
        }
    }

    std::vector<thread_type>                m_threads;
    std::atomic<bool>                       m_done;
    queue_type                              m_tasks;
    
    using lock_type = std::unique_lock<mutex_type>;
    mutex_type                              m_mtx;
    boost::fibers::condition_variable_any   m_cnd;
    allocator_type                          m_alloc;
};


}}//! namespace stk::thread;

#endif // STK_THREAD_FIBER_THREAD_SYSTEM_HPP
