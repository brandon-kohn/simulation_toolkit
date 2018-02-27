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
#include <stk/thread/fiber_pool.hpp> //-> requires C++11 with constexpr/noexcept etc. vs140 etc.
#include <stk/thread/tiny_atomic_spin_lock.hpp>
#include <stk/container/fine_locked_hash_map.hpp>
#include <boost/thread/futures/wait_for_all.hpp>

#include <boost/context/stack_traits.hpp>

#include <geometrix/utility/scope_timer.ipp>

#include <stk/thread/concurrentqueue.h>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>
#include <stk/thread/work_stealing_thread_pool.hpp>
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::uint32_t);
#include <stk/thread/thread_specific.hpp>
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(int);
STK_THREAD_SPECIFIC_INSTANCE_DEFINITION(std::unique_ptr<int>);
#include <stk/container/concurrent_skip_list.hpp>
#include <stk/container/lock_free_concurrent_skip_list.hpp>

#include <cds/init.h>
#include <cds/gc/hp.h>
#include <cds/urcu/general_buffered.h>
typedef cds::urcu::gc< cds::urcu::general_buffered<> >    rcu_gpb;
struct libcds_raii
{

	libcds_raii()
	{
		cds::Initialize();
		hpGC = new cds::gc::HP{};
		gpbRCU = new rcu_gpb{};
		cds::threading::Manager::attachThread();
	}

	libcds_raii(const libcds_raii&) = delete;
	libcds_raii& operator=(const libcds_raii&) = delete;

	libcds_raii(libcds_raii&& rhs)
		: hpGC(rhs.hpGC)
		, gpbRCU(rhs.gpbRCU)
	{
		rhs.hpGC = nullptr;
		rhs.gpbRCU = nullptr;
	}

	libcds_raii& operator=(libcds_raii&&rhs)
	{
		std::swap(hpGC, rhs.hpGC);
		std::swap(gpbRCU, rhs.gpbRCU);
	}

	~libcds_raii()
	{
		if (hpGC)
		{
			delete hpGC;
			delete gpbRCU;
			cds::Terminate();
		}
	}

	cds::gc::HP* hpGC{ nullptr };
	rcu_gpb* gpbRCU{ nullptr };
};

auto cdsSinglton = libcds_raii();

TEST(concurrent_skip_list_tests, construct)
{
	using namespace stk;
	using namespace stk::thread;
	
	EXPECT_NO_THROW((concurrent_map<int, int>()));
}

TEST(lf_concurrent_skip_list_tests, construct)
{
	using namespace stk;
	using namespace stk::thread;
	
	EXPECT_NO_THROW((lock_free_concurrent_map<int, int>()));
}

TEST(concurrent_skip_list_tests, empty)
{
	using namespace stk;
	using namespace stk::thread;

	concurrent_map<int, int> m;

	EXPECT_TRUE(m.empty());
	EXPECT_TRUE(m.size() == 0);
}

TEST(lf_concurrent_skip_list_tests, empty)
{
	using namespace stk;
	using namespace stk::thread;

	lock_free_concurrent_map<int, int> m;

	EXPECT_TRUE(m.empty());
	EXPECT_TRUE(m.size() == 0);
}

TEST(concurrent_skip_list_tests, find_on_empty_returns_end)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;
	
	concurrent_map<int, int> m;
	auto it = m.find(10);

	EXPECT_EQ(it, m.end());
}

TEST(concurrent_skip_list_tests, insert_to_empty)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;
	
	concurrent_map<int, int> m;
	bool inserted = false;
	iterator it;

	std::tie(it, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_TRUE(inserted);
	EXPECT_NE(it, m.end());
	EXPECT_EQ(20, it->second);
}

TEST(concurrent_skip_list_tests, insert_two_no_replication)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;

	concurrent_map<int, int> m;
	bool inserted = false;
	iterator it;

	std::tie(it, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_TRUE(inserted);
	std::tie(it, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_FALSE(inserted);

	EXPECT_NE(it, m.end());
	EXPECT_EQ(20, it->second);
}

TEST(concurrent_skip_list_tests, insert_two_find_second)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;

	concurrent_map<int, int> m;
	bool inserted = false;
	iterator it;

	std::tie(it, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_TRUE(inserted);
	std::tie(it, inserted) = m.insert(std::make_pair(20, 30));
	EXPECT_TRUE(inserted);
	EXPECT_NE(it, m.end());
	EXPECT_EQ(30, it->second);

	it = m.find(10);
	EXPECT_NE(it, m.end());
	EXPECT_EQ(20, it->second);
	it = m.find(20);
	EXPECT_NE(it, m.end());
	EXPECT_EQ(30, it->second);
}

TEST(concurrent_skip_list_tests, clear_test)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;

	concurrent_map<int, int> m;
	for (int i = 0; i < 10; ++i)
		m.insert(std::make_pair(i, 2 * i));

	m.clear();

	EXPECT_TRUE(m.empty());
	EXPECT_EQ(0, m.size());
}

TEST(concurrent_skip_list_tests, insert_two_remove_first_iterator_remains_valid)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;

	concurrent_map<int, int> m;
	bool inserted = false;
	iterator it;

	std::tie(it, inserted) = m.insert(std::make_pair(20, 30));
	EXPECT_TRUE(inserted);
	std::tie(it, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_TRUE(inserted);

	auto nit = m.erase(10);

	EXPECT_NE(it, m.end());
	EXPECT_EQ(20, it->second);
}

TEST(concurrent_skip_list_tests, copy_iterator)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;

	concurrent_map<int, int> m;
	bool inserted = false;
	iterator it, it2;

	std::tie(it, inserted) = m.insert(std::make_pair(20, 30));
	EXPECT_TRUE(inserted);

	it2 = it;

	EXPECT_EQ(it2, it);
	EXPECT_NE(it, m.end());
}

TEST(concurrent_skip_list_tests, move_iterator)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = concurrent_map<int, int>::iterator;

	concurrent_map<int, int> m;
	bool inserted = false;
	iterator it, it2;

	std::tie(it, inserted) = m.insert(std::make_pair(20, 30));
	EXPECT_TRUE(inserted);

	it2 = std::move(it);
	EXPECT_EQ(it, m.end());
	EXPECT_NE(it2, it);
}

TEST(lf_concurrent_skip_list_tests, insert_two_no_replication)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = lock_free_concurrent_map<int, int>::iterator;

	lock_free_concurrent_map<int, int> m;
	bool inserted = false;
	iterator it, it2;

	std::tie(it, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_TRUE(inserted);
	std::tie(it2, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_EQ(it, it2);
	EXPECT_FALSE(inserted);
}

TEST(lf_concurrent_skip_list_tests, insert_two_remove_first)
{
	using namespace stk;
	using namespace stk::thread;
	using iterator = lock_free_concurrent_map<int, int>::iterator;

	lock_free_concurrent_map<int, int> m;
	bool inserted = false;
	iterator it;

	std::tie(it, inserted) = m.insert(std::make_pair(10, 20));
	EXPECT_TRUE(inserted);
	std::tie(it, inserted) = m.insert(std::make_pair(20, 30));
	EXPECT_TRUE(inserted);

	auto it2 = m.erase(10);
	EXPECT_EQ(it, it2);
}

template <typename Pool>
void bash_map(Pool& pool, const char* name)
{
	using namespace ::testing;
	using namespace stk;

	concurrent_map<int, int> m;

	auto nItems = 10000;
	for (auto i = 0; i < nItems; ++i)
	{
		m.insert(std::make_pair(i, i * 10));
	}

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	unsigned nsubwork = 10;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			fs.emplace_back(pool.send([&m, nsubwork, i]() -> void
			{
				for (unsigned int q = 0; q < nsubwork; ++q)
				{
					m[i] = i * 20;
					m.erase(i);
					m[i] = i * 20;
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
		m.quiesce();
	}

	for (auto i = 0; i < 100000; ++i)
	{
		auto r = m.find(i);
		EXPECT_TRUE(r != m.end());
		EXPECT_EQ(i * 20, r->second);
	}
}

template <typename Pool>
void bash_lf_concurrent_map(Pool& pool, const char* name)
{
	using namespace ::testing;
	using namespace stk;

	lock_free_concurrent_map<int, int> m;

	auto nItems = 10000;
	for (auto i = 0; i < nItems; ++i)
	{
		m.insert(std::make_pair(i, i * 10));
	}

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	unsigned nsubwork = 10;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			fs.emplace_back(pool.send([&m, nsubwork, i]() -> void
			{
				for (unsigned int q = 0; q < nsubwork; ++q)
				{
					m[i] = i * 20;
					m.erase(i);
					m[i] = i * 20;
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
		m.quiesce();
	}

	for (auto i = 0; i < 100000; ++i)
	{
		auto r = m.find(i);
		EXPECT_TRUE(r != m.end());
		EXPECT_EQ(i * 20, r->second);
	}
}

#include <stk/thread/concurrentqueue.h>
#include <stk/thread/concurrentqueue_queue_info.h>
int nTimingRuns = 5;
TEST(concurrent_skip_list_tests, bash_map_work_stealing)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool{ 5 };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_map(pool, "work_stealing_thread_pool moody-concurrent");
		EXPECT_TRUE(true) << "Finished: " << i << "\n";
	}

	EXPECT_TRUE(true);
}

TEST(lf_concurrent_skip_list_tests, bash_lock_free_concurrent_map_work_stealing)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool{ 5 };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_lf_concurrent_map(pool, "work_stealing_thread_pool moody-/lock_free_concurrent");
		EXPECT_TRUE(true) << "Finished: " << i << "\n";
	}

	EXPECT_TRUE(true);
}

template <typename Pool>
void bash_lf_concurrent_map_remove_odd(Pool& pool, const char* name)
{
	using namespace ::testing;
	using namespace stk;

	lock_free_concurrent_map<int, int> m;

	auto nItems = 10000;
	for (auto i = 0; i < nItems; ++i)
	{
		m.insert(std::make_pair(i, i * 10));
	}

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	unsigned nsubwork = 10;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			fs.emplace_back(pool.send([&m, nsubwork, i]() -> void
			{
				for (unsigned int q = 0; q < nsubwork; ++q)
				{
					m.insert_or_update(i, [i](bool isNew, std::pair<const int, int>& item) { item.second = i * 20; });
					m.erase(i);
					m.insert_or_update(i, [i](bool isNew, std::pair<const int, int>& item) { item.second = i * 20; });
					if (i % 2)
						m.erase(i);
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
		m.quiesce();
	}

	for (auto i = 0; i < 100000; ++i)
	{
		auto r = m.find(i);
		if (i % 2 == 0)
		{
			EXPECT_TRUE(r != m.end());
			EXPECT_EQ(i * 20, r->second);
		}
		else
			EXPECT_TRUE(r == m.end());
	}
}

TEST(lf_concurrent_skip_list_tests, bash_lock_free_concurrent_map_work_stealing_remove_odd)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool{ 5 };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_lf_concurrent_map_remove_odd(pool, "work_stealing_thread_pool moody-remove_odd/lock_free_concurrent");
		EXPECT_TRUE(true) << "Finished: " << i << "\n";
	}

	EXPECT_TRUE(true);
}

#include <cds/container/skip_list_map_rcu.h>
template <typename Pool>
void bash_cds_lf_concurrent_map_remove_odd(Pool& pool, const char* name)
{
	using namespace ::testing;
	using namespace stk;

	cds::container::SkipListMap<rcu_gpb, int, int> m;

	auto nItems = 10000;
	for (auto i = 0; i < nItems; ++i)
	{
		m.insert(i, i * 10);
	}

	using future_t = typename Pool::template future<void>;
	std::vector<future_t> fs;
	unsigned nsubwork = 10;
	fs.reserve(100000);
	{
		GEOMETRIX_MEASURE_SCOPE_TIME(name);
		for (unsigned i = 0; i < 100000; ++i) {
			fs.emplace_back(pool.send([&m, nsubwork, i]() -> void
			{
				for (unsigned int q = 0; q < nsubwork; ++q)
				{
					m.update(i, [i](bool isNew, std::pair<const int, int>& item) { item.second = i * 20; });
					m.erase(i);
					m.update(i, [i](bool isNew, std::pair<const int, int>& item) { item.second = i * 20; });
					if (i % 2)
						m.erase(i);
				}
			}));
		}
		boost::for_each(fs, [](const future_t& f) { f.wait(); });
	}

	for (auto i = 0; i < 100000; ++i)
	{
		int v = 0;
		auto value = [&v](std::pair<const int, int>& value) { v = value.second; };
		auto r = m.find(i, value);
		if (i % 2 == 0)
		{
			EXPECT_TRUE(r);
			EXPECT_EQ(i * 20, v);
		} else
			EXPECT_FALSE(r);
	}
}

inline void cds_enter_thread()
{
	cds::threading::Manager::attachThread();
}

inline void cds_exit_thread()
{
	cds::threading::Manager::detachThread();
}

TEST(lf_concurrent_skip_list_tests, bash_cds_lock_free_concurrent_map_work_stealing_remove_odd)
{
	using namespace ::testing;
	using namespace stk;
	using namespace stk::thread;
	
	work_stealing_thread_pool<moodycamel_concurrent_queue_traits> pool{ cds_enter_thread, cds_exit_thread, 5 };
	for (int i = 0; i < nTimingRuns; ++i)
	{
		bash_cds_lf_concurrent_map_remove_odd(pool, "cds_work_stealing_thread_pool moody-remove_odd/lock_free_concurrent");
		EXPECT_TRUE(true) << "Finished: " << i << "\n";
	}

	EXPECT_TRUE(true);
}