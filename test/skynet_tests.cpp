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

#include <stk/thread/fiber_thread_system.hpp>

#include "pool_work_stealing.hpp"
#include <boost/context/stack_traits.hpp>
#include <geometrix/utility/scope_timer.ipp>

#include <thread>
#include <chrono>
#include <exception>
#include <iostream>

using allocator_type = boost::fibers::fixedsize_stack;
std::uint64_t skynet(allocator_type& salloc, std::uint64_t num, std::uint64_t size, std::uint64_t div)
{
	if (size != 1)
	{
		size /= div;

		std::vector<boost::fibers::future<std::uint64_t>> results;
		results.reserve(div);

		for (std::uint64_t i = 0; i != div; ++i)
		{
			std::uint64_t sub_num = num + i * size;
			results.emplace_back(boost::fibers::async(boost::fibers::launch::dispatch, std::allocator_arg, salloc, skynet, std::ref(salloc), sub_num, size, div));
		}
		
		std::uint64_t sum = 0;
		for (auto& f : results)
			sum += f.get();

		return sum;
	}

	return num;
}

TEST(timing, boost_fibers_skynet_raw)
{
	using namespace stk;
	using namespace stk::thread;

	// Windows 10 and FreeBSD require a fiber stack of 8kb
	// otherwise the stack gets exhausted
	// stack requirements must be checked for other OS too
#if BOOST_OS_WINDOWS || BOOST_OS_BSD
	allocator_type salloc{ 2 * allocator_type::traits_type::page_size() };
#else
	allocator_type salloc{ allocator_type::traits_type::page_size() };
#endif

	std::vector<boost::intrusive_ptr<boost::fibers::algo::pool_work_stealing>> schedulers(std::thread::hardware_concurrency());
	
	auto schedulerPolicy = [&schedulers](unsigned int id) 
    {
		boost::fibers::use_scheduling_algorithm<boost::fibers::algo::pool_work_stealing>(id, &schedulers, false);
	};
	fiber_thread_system<allocator_type> fts(salloc, std::thread::hardware_concurrency(), schedulerPolicy);
	std::uint64_t r;
	{
		GEOMETRIX_MEASURE_SCOPE_TIME("boost_fibers_skynet_raw");
		auto f = fts.async([&salloc]() 
		{
			return skynet(salloc, 0, 1000000, 10);			
		});
		r = f.get();		
	}
	EXPECT_EQ(499999500000, r);
}
