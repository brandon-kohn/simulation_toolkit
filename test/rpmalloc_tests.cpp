//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info.h>
#include <stk/utility/rpmalloc_system.hpp>
#include <geometrix/utility/scope_timer.ipp>

namespace test
{
    struct impl_base
    {
    public:
        virtual void call() = 0;
        virtual ~impl_base(){}
    };

    template <typename F>
    struct move_impl_type : impl_base
    {
        move_impl_type(F&& f)
            : m_f(std::move(f))
        {}

        void call()
        {
            m_f();
        }

        F m_f;
    };
}//! namespace test;
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(unsigned int);
STK_INSTANTIATE_RPMALLOC_SYSTEM();//! Calls rpmalloc_initialize() and finalize() using RAII in a global.
std::size_t nAllocations = 1000000;
TEST(rpmalloc_test_suite, cross_thread_bench)
{
    using namespace stk::thread;
    std::size_t nOSThreads = std::thread::hardware_concurrency()-1;
	{
		using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits>;
		pool_t pool(rpmalloc_thread_initialize, rpmalloc_thread_finalize, nOSThreads);
		using future_t = pool_t::future<void>;
		std::vector<future_t> futures;
		futures.reserve(nAllocations);
		{
			GEOMETRIX_MEASURE_SCOPE_TIME("rpmalloc_cross_thread_32_bytes");
			for (size_t i = 0; i < nAllocations; ++i)
			{
				auto pAlloc = rpmalloc(32);
				futures.emplace_back(pool.send(i++%pool.number_threads(),
					[pAlloc]()
				{
					rpfree(pAlloc);
				}));
			}

			boost::for_each(futures, [](const future_t& f) { f.wait(); });
		}
	}
}

TEST(rpmalloc_test_suite, cross_thread_bench_malloc_free)
{
    using namespace stk::thread;
    std::size_t nOSThreads = std::thread::hardware_concurrency()-1;
	using pool_t = work_stealing_thread_pool<moodycamel_concurrent_queue_traits>;
    pool_t pool(nOSThreads);

    using future_t = pool_t::future<void>;
    std::vector<future_t> futures;
    futures.reserve(nAllocations);
    {
        GEOMETRIX_MEASURE_SCOPE_TIME("malloc_cross_thread_32_bytes");
        for (size_t i = 0; i < nAllocations; ++i)
        {
            auto pAlloc = malloc(32);
            futures.emplace_back(pool.send(i++%pool.number_threads(),
                [pAlloc]()
                {
                    free(pAlloc);
                }));
        }

        boost::for_each(futures, [](const future_t& f) { f.wait(); });
    }
}
