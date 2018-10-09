//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <geometrix/utility/scope_timer.ipp>
#include <stk/thread/work_stealing_thread_pool.hpp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info_no_tokens.h>
#include <stk/thread/thread_specific_single_instance_map_policy.hpp>
#include <stk/container/type_storage_pod.hpp>

#define STK_DEFINE_THREAD_SPECIFIC_HIVE_INLINE
#include <stk/thread/thread_specific.hpp>

#define STK_DEFINE_THREAD_DATA_VECTOR_INLINE
#if(BOOST_MSVC && BOOST_MSVC < 1900)
#define STK_NO_CXX11_THREADSAFE_LOCAL_STATICS
#endif

#include <boost/smart_ptr/make_unique.hpp>
#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>

#include <boost/preprocessor/stringize.hpp>

STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int, thread_specific_unordered_map_policy<int>, default_thread_specific_tag);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int, thread_specific_std_map_policy<int>, default_thread_specific_tag);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int, thread_specific_flat_map_policy<int>, default_thread_specific_tag);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::unique_ptr<int>, thread_specific_std_map_policy<std::unique_ptr<int>>, default_thread_specific_tag);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int*, thread_specific_std_map_policy<int*>, default_thread_specific_tag);

TEST(thread_specific_tests, thread_specific_interface)
{
    using namespace stk;
    using namespace stk::thread;
	{
		thread_specific<int> sut{ []() { return 10; } };

		EXPECT_EQ(10, *sut);

		*sut = 5;

		EXPECT_EQ(5, *sut);
	}
}

TEST(thread_specific_tests, thread_specific_int)
{
    using namespace stk;
    using namespace stk::thread;
    thread_specific<int> sut{ []() { return 10; } };

    std::vector<std::thread> thds;
    for(int i = 0; i < 10; ++i)
    {
        thds.emplace_back([i, &sut]()
        {
            *sut = i;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            int v = *sut;
            EXPECT_EQ(i, v);
        });
    }

    boost::for_each(thds, [](std::thread& thd) { thd.join(); });
}

TEST(thread_specific_tests, thread_specific_int_ptr)
{
    using namespace stk;
    using namespace stk::thread;
    std::atomic<int> up{ 0 }, down{ 0 };
    {
        thread_specific<int*> sut{ [&up]() { ++up; return new int(10); }, [&down](int*& p) { ++down;  delete p; } };

        std::vector<std::thread> thds;
        for (int i = 0; i < 10; ++i)
        {
            thds.emplace_back([i, &sut]()
            {
                **sut = i;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                int* v = *sut;
                EXPECT_EQ(i, *v);
            });
        }

        boost::for_each(thds, [](std::thread& thd) { thd.join(); });
    }
    EXPECT_NE(0, up.load());
    EXPECT_EQ(down.load(), up.load());
}

TEST(thread_specific_tests, thread_specific_int_ptr_single_instance)
{
    using namespace stk;
    using namespace stk::thread;
    std::atomic<int> up{ 0 }, down{ 0 };
    {
        thread_specific<int*, thread_specific_single_instance_map_policy<int*>> sut{ [&up]() { ++up; return new int(10); }, [&down](int*& p) { ++down;  delete p; } };

        std::vector<std::thread> thds;
        for (int i = 0; i < 10; ++i)
        {
            thds.emplace_back([i, &sut]()
            {
                **sut = i;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                int* v = *sut;
                EXPECT_EQ(i, *v);
            });
        }

        boost::for_each(thds, [](std::thread& thd) { thd.join(); });
    }
    EXPECT_NE(0, up.load());
    EXPECT_EQ(down.load(), up.load());
}

TEST(thread_specific_tests, const_thread_specific_int)
{
    using namespace stk;
    using namespace stk::thread;
    const thread_specific<int> sut{ []() { return 10; } };

    std::vector<std::thread> thds;
    for (int i = 0; i < 10; ++i)
    {
        thds.emplace_back([i, &sut]()
        {
            int v = *sut;
            EXPECT_EQ(10, v);
        });
    }

    boost::for_each(thds, [](std::thread& thd) { thd.join(); });
}

TEST(thread_specific_tests, thread_specific_threads_go_out_of_scope)
{
    using namespace stk;
    using namespace stk::thread;
    std::atomic<int> up{ 0 }, down{ 0 };
    thread_specific<int*> sut{ [&up]() { ++up; return new int(10); }, [&down](int*& p) { ++down;  delete p; } };
    {
        std::vector<std::thread> thds;
        for (int i = 0; i < 10; ++i)
        {
            thds.emplace_back([i, &sut]()
            {
                **sut = i;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                int* v = *sut;
                EXPECT_EQ(i, *v);
            });
        }

        boost::for_each(thds, [](std::thread& thd) { thd.join(); });
    }

	sut.for_each_thread_value([](int*& p)
	{
		//! There shouldn't be any.
		ASSERT_FALSE(true);
	});
    EXPECT_NE(0, up.load());
    EXPECT_EQ(down.load(), up.load());
}

TEST(thread_specific_tests, thread_specific_tss_go_out_of_scope)
{
    using namespace stk;
    using namespace stk::thread;
    std::atomic<int> up{ 0 }, down{ 0 };
	std::mutex cmtx;
	std::condition_variable cnd;
	std::vector<std::thread> thds;
	std::atomic<int> gate{ 0 };
    {
		thread_specific<int*> sut{ [&up]() { ++up; return new int(10); }, [&down](int*& p) { ++down;  delete p; } };
        for (int i = 0; i < 10; ++i)
        {
            thds.emplace_back([i, &gate, &sut]()
            {
                **sut = i;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                int* v = *sut;
                EXPECT_EQ(i, *v);
				++gate;
				while (gate != 0);

				int q = 0;
            });
        }

		while (gate != 10);
    }

	gate.store(0);
	boost::for_each(thds, [](std::thread& thd) { thd.join(); });
    EXPECT_NE(0, up.load());
    EXPECT_EQ(down.load(), up.load());
}

TEST(thread_specific_tests, thread_specific_unique_ptr)
{
    using namespace stk;
    using namespace stk::thread;
    thread_specific<std::unique_ptr<int>> sut{ []() { return boost::make_unique<int>(10); } };

    std::vector<std::thread> thds;
    for (int i = 0; i < 10; ++i)
    {
        thds.emplace_back([i, &sut]()
        {
            std::unique_ptr<int>& p = *sut;
            EXPECT_EQ(10, *p.get());
            *p = i;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::unique_ptr<int>& p2 = *sut;
            EXPECT_EQ(p.get(), p2.get());
            EXPECT_EQ(i, *p2);
        });
    }

    boost::for_each(thds, [](std::thread& thd) { thd.join(); });
}

TEST(thread_specific_tests, thread_specific_int_two_instances)
{
    using namespace stk;
    using namespace stk::thread;
    thread_specific<int> sut{ []() { return 10; } };
    thread_specific<int> sut2{ []() { return 20; } };

    std::vector<std::thread> thds;
    for (int i = 0; i < 10; ++i)
    {
        thds.emplace_back([i, &sut, &sut2]()
        {
            *sut = i;
            *sut2 = i * 2;
        });
    }

    EXPECT_EQ(10, *sut);
    boost::for_each(thds, [](std::thread& thd) { thd.join(); });
    EXPECT_EQ(20, *sut2);
}

TEST(thread_specific_tests, fixed_map_thread_specific)
{
    using namespace stk;
    using namespace stk::thread;
    
	const thread_specific<int, thread_specific_fixed_flat_map_policy<int, 2>> sut1{ []() { return 10; } };
	const thread_specific<int, thread_specific_fixed_flat_map_policy<int, 2>> sut2{ []() { return 20; } };

    std::vector<std::thread> thds;
    for (int i = 0; i < 10; ++i)
    {
        thds.emplace_back([i, &sut1, &sut2]()
        {
            int v = *sut1;
            EXPECT_EQ(10, v);
            v = *sut2;
            EXPECT_EQ(20, v);
        });
    }

    boost::for_each(thds, [](std::thread& thd) { thd.join(); });
}

TEST(thread_specific_tests, single_instance_thread_specific)
{
    using namespace stk;
    using namespace stk::thread;
    
	const thread_specific<int, thread_specific_single_instance_map_policy<int>> sut1{ []() { return 10; } };

    std::vector<std::thread> thds;
    for (int i = 0; i < 10; ++i)
    {
        thds.emplace_back([i, &sut1]()
        {
            int v = *sut1;
            EXPECT_EQ(10, v);
        });
    }

    boost::for_each(thds, [](std::thread& thd) { thd.join(); });
}

TEST(timing, single_instance_thread_specific_timing)
{
    using namespace stk;
    using namespace stk::thread;
    work_stealing_thread_pool<moodycamel_concurrent_queue_traits_no_tokens, boost_thread_traits> pool;
    std::size_t nRuns = 1000000;
    {
        GEOMETRIX_MEASURE_SCOPE_TIME("thread_specific_single_instance");
		thread_specific<int, thread_specific_single_instance_map_policy<int>> sut{ []() { return 10; } };
        pool.parallel_apply(nRuns, [&sut](int)
		{
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = *sut;
                ++v;
            }
        });
    }
}

TEST(timing, fixed_flat_map_thread_specific_timing)
{
    using namespace stk;
    using namespace stk::thread;
    work_stealing_thread_pool<moodycamel_concurrent_queue_traits_no_tokens, boost_thread_traits> pool;
    std::size_t nRuns = 1000000;
    {
        GEOMETRIX_MEASURE_SCOPE_TIME("thread_specific_fixed_flat_map");
        thread_specific<int, thread_specific_fixed_flat_map_policy<int, 1>> sut{ []() { return 10; } };
        pool.parallel_apply(nRuns, [&sut](int)
		{
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = *sut;
                ++v;
            }
        });
    }
}

TEST(timing, flat_map_thread_specific_timing)
{
    using namespace stk;
    using namespace stk::thread;
    work_stealing_thread_pool<moodycamel_concurrent_queue_traits_no_tokens, boost_thread_traits> pool;
    std::size_t nRuns = 1000000;
    {
        GEOMETRIX_MEASURE_SCOPE_TIME("thread_specific_flat_map");
        thread_specific<int, thread_specific_flat_map_policy<int>> sut{ []() { return 10; } };
        pool.parallel_apply(nRuns, [&sut](int)
		{
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = *sut;
                ++v;
            }
        });
    }
}

TEST(timing, compare_thread_specific_and_boost_tss)
{
    using namespace stk;
    using namespace stk::thread;
    work_stealing_thread_pool<moodycamel_concurrent_queue_traits_no_tokens, boost_thread_traits> pool;
    std::size_t nRuns = 1000000;
    {
        GEOMETRIX_MEASURE_SCOPE_TIME("thread_specific_unordered");
        thread_specific<int, thread_specific_unordered_map_policy<int>> sut{ []() { return 10; } };
        pool.parallel_apply(nRuns, [&sut](int) 
		{
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = *sut;
                ++v;
            }
        });
    }

    {
        GEOMETRIX_MEASURE_SCOPE_TIME("thread_specific_std_map");
        thread_specific<int, thread_specific_std_map_policy<int>> sut{ []() { return 10; } };
        pool.parallel_apply(nRuns, [&sut](int)
		{
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = *sut;
                ++v;
            }
        });
    }

	{
        GEOMETRIX_MEASURE_SCOPE_TIME("boost_tss");
        boost::thread_specific_ptr<int> sut;
        pool.parallel_apply(nRuns, [&sut](int) {
            if (!sut.get())
                sut.reset(new int{ 10 });
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = *sut;
                ++v;
            }
        });
    }

    {
        GEOMETRIX_MEASURE_SCOPE_TIME("thread_local");
        pool.parallel_apply(nRuns, [](int) {
            static STK_THREAD_LOCAL_POD int sut = 10;
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = sut;
                ++v;
            }
        });
    }

}

