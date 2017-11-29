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

#include <stk/container/concurrent_skip_list.hpp>

TEST(concurrency_test_suite, test_skip_list)
{
    using namespace ::testing;
	using namespace stk;

	concurrent_map<int, int> map;

	map.insert(std::make_pair(10, 20));
}
