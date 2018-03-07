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

#include <stk/thread/active_object.hpp>
#include <stk/thread/fiber_pool.hpp> //-> requires C++11 with constexpr/noexcept etc. vs140 etc.
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/container/fine_locked_hash_map.hpp>
#include <boost/thread/futures/wait_for_all.hpp>
#include <boost/context/stack_traits.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info.h>
#include <thread>
#include <chrono>
#include <exception>
#include <iostream>
#include <stk/thread/work_stealing_fiber_pool.hpp>
#include <stk/fiber/make_ready_future.hpp>
#include <stk/thread/thread_specific.hpp>
#include <stk/thread/boost_fiber_traits.hpp>

STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::uint32_t);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::unique_ptr<int>);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int*);

TEST(fiber_make_ready_future, construct)
{
    using namespace ::testing;
    std::vector<int> values = { 1, 2, 3, 4, 5, 6 };

    auto f = boost::fibers::make_ready_future(values);
    EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 5, 6));

    EXPECT_TRUE(f.valid());

    auto v = f.get();

    EXPECT_THAT(v, ElementsAre(1, 2, 3, 4, 5, 6));
}

TEST(active_object_test, construct_fiber_active_object)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    active_object<boost_fiber_traits> obj(boost_fiber_creation_policy<>(boost::fibers::fixedsize_stack{ 64 * 1024 }));

    std::atomic<bool> isRun(false);
    auto r = obj.send([&]() { return isRun = true; });

    EXPECT_TRUE(r.get() && isRun);
}

TEST(fiber_pool_test, construct)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    auto alloc = boost::fibers::fixedsize_stack{64 * 1024};
    {
        fiber_pool<> obj{ 10, alloc };

        std::atomic<bool> isRun{ false };

        std::vector<boost::fibers::future<void>> r;
        r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
        r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
        r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
        r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
        r.emplace_back(obj.send([]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); }));
        r.emplace_back(obj.send([&]() -> void { boost::this_fiber::sleep_for(std::chrono::milliseconds(10)); isRun = true; }));

        boost::for_each(r, [](const boost::fibers::future<void>& f) { f.wait(); });

        EXPECT_TRUE(isRun);
    }
}

