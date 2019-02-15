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
	auto sw = make_switch(
	    make_case<d1_type>([&](d1_type* p)
	    {
	        std::cout << "Type was d1_type.\n";
	    })
	  , make_case<d2_type>([&](d2_type* p)
	    {
	        std::cout << "Type was d2_type.\n";
	    })
	  , make_case<d3_type>([&](d3_type* p)
	    {
	        std::cout << "Type was d3_type.\n";
	    })
	);

	auto t1 = d1_type{};
	auto t2 = d2_type{};
	auto t3 = d3_type{};

	sw(&t1);
	sw(&t2);
	sw(&t3);
	sw(&t2);
}
