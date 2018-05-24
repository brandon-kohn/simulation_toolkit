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
#include <stk/thread/thread_pool.hpp>
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/container/fine_locked_hash_map.hpp>
#include <stk/thread/boost_thread_kernel.hpp>
#include <boost/thread/futures/wait_for_all.hpp>
#include <geometrix/utility/scope_timer.ipp>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>

TEST(fine_locked_hash_map, construct)
{
    //! Had to put this verify in to stop this reference from being dropped on gcc in linux.
    using namespace ::testing;
    using namespace stk;

    static_assert(sizeof(stk::thread::tiny_atomic_spin_lock<>) == 1, "size should be one byte.");

    fine_locked_hash_map<int, int> m;
}

TEST(fine_locked_hash_map, add_item)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;

    fine_locked_hash_map<int, int> m;

    m.add_or_update(10, 20);
    m.add_or_update(20, 30);
    m.add_or_update(30, 40);

    auto r = m.find(10);
    ASSERT_TRUE(r);
    EXPECT_EQ(20, *r);

    r = m.find(20);
    ASSERT_TRUE(r);
    EXPECT_EQ(30, *r);

    r = m.find(30);
    ASSERT_TRUE(r);
    EXPECT_EQ(40, *r);
}

TEST(fine_locked_hash_map, add_update_item)
{
    using namespace ::testing;
    using namespace stk;

    fine_locked_hash_map<int, int> m;

    m.add_or_update(10, 20);
    auto r = m.add(10, 20);

    EXPECT_FALSE(r);
}

TEST(active_object_test, construct)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    active_object<> obj;

    std::atomic<bool> isRun(false);
    auto r = obj.send([&]() { return isRun = true; });

    EXPECT_TRUE(r.get() && isRun);
}

TEST(active_object_test, exception)
{
    using namespace ::testing;
    using namespace stk;
    using namespace stk::thread;
    active_object<> obj;

    auto r = obj.send([&]() -> void
    {
        throw std::logic_error("");
    });

    r.wait();
    EXPECT_TRUE(r.has_exception());
    EXPECT_THROW(r.get(), std::logic_error);
}

TEST(check_test, sizeof_mutexes)
{
    GTEST_MESSAGE() << "sizeof(std::mutex) = " << sizeof(std::mutex) << std::endl;
    GTEST_MESSAGE() << "sizeof(boost::mutex) = " << sizeof(boost::mutex) << std::endl;
    GTEST_MESSAGE() << "sizeof(stk::thread::tiny_atomic_spin_lock<>) = " << sizeof(stk::thread::tiny_atomic_spin_lock<>) << std::endl;
}
