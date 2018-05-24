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
#include <stk/thread/thread_specific.hpp>
#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>

STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::unique_ptr<int>);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int*);
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

TEST(timing, DISABLED_compare_thread_specific_and_boost_tss)
{
    using namespace stk;
    using namespace stk::thread;
    work_stealing_thread_pool<moodycamel_concurrent_queue_traits, boost_thread_traits> pool;
    std::size_t nRuns = 100000;
    {
        GEOMETRIX_MEASURE_SCOPE_TIME("thread_specific");
        thread_specific<int> sut{ []() { return 10; } };
        pool.parallel_apply(nRuns, [&sut](int) {
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
            static thread_local int sut = 10;
            for (auto i = 0; i < 10000; ++i)
            {
                int& v = sut;
                ++v;
            }
        });
    }

}

