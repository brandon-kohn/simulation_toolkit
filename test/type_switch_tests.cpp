//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <stk/utility/type_switch.hpp>
#include <iostream>
#include <array>

struct base_type
{
	virtual ~base_type()
	{}
};
struct base1_type : base_type{};
struct base2_type : base_type{};
struct d1_type : base1_type {};
struct d2_type : base1_type {};
struct d3_type : base2_type {};
struct d4_type : base2_type {};
TEST(memoization_device_suite, test)
{
	using namespace stk;
	auto t1 = d1_type{};
	auto t2 = d2_type{};
	auto t3 = d3_type{};
	auto t4 = d4_type{};
	std::array<int, 4> count = { 0, 0, 0, 0 };
	auto sw = make_switch(
	    type_case([&](const d1_type* p)
	    {
			++count[0];
			EXPECT_EQ(&t1, p);
	    })
	  , type_case([&](d2_type* p)
	    {
			++count[1];
			EXPECT_EQ(&t2, p);
	    })
	  , type_case([&](d3_type* p)
	    {
			++count[2];
			EXPECT_EQ(&t3, p);
	    })
	  , type_case([&](d4_type* p)
	    {
			++count[3];
			EXPECT_EQ(&t4, p);
	    })
	);

	sw((base_type*)&t1);
	sw((base_type*)&t2);
	sw((base_type*)&t3);
	sw((base_type*)&t2);
	sw((base_type*)&t4);

	EXPECT_EQ(1, count[0]);
	EXPECT_EQ(2, count[1]);
	EXPECT_EQ(1, count[2]);
	EXPECT_EQ(1, count[3]);
}
