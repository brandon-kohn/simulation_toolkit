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
	using T = base_type;
	auto p1 = [](T* x) { return dynamic_cast<d1_type*>(x); };
	auto p2 = [](T* x) { return dynamic_cast<d2_type*>(x); };
	auto p3 = [](T* x) { return dynamic_cast<d3_type*>(x); };

	auto s1 = [](T* x) { std::cout << "s1 was run.\n"; };
	auto s2 = [](T* x) { std::cout << "s2 was run.\n"; };
	auto s3 = [](T* x) { std::cout << "s3 was run.\n"; };
	auto vtbl = [](const T* x)
	{
		static_assert(std::is_polymorphic<T>::value, "T must be a polymorphic type.");
		return *reinterpret_cast<const std::intptr_t*>(x);
	};

	auto dswitch = [&](T* x)
	{
		using raw_type = std::decay<T>::type;
		struct type_switch_info
		{
			type_switch_info(std::uint64_t d)
				: data(d)
			{}
			
			type_switch_info(std::int32_t o, std::uint32_t c)
				: offset(o)
				, case_n(c)
			{}

			union
			{
				std::int32_t offset;
				std::uint32_t case_n;
				std::uint64_t data;
			};
		};
		static stk::concurrent_numeric_unordered_map<std::intptr_t, std::uint64_t> jump_targets;
		auto key = vtbl(x);
		const void* tptr;
		type_switch_info info(jump_targets.insert(key, std::uint64_t{}).first);
		switch (info.case_n) 
		{
			default:
				if (tptr = p1(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), 1).data); case 1: s1(x); } else
				if (tptr = p2(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), 2).data); case 2: s2(x); } else
				if (tptr = p3(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), 3).data); case 3: s3(x); } else
					case 4: break;//! no true predicates.
		};
	};

	auto t1 = d1_type{};
	auto t2 = d2_type{};
	auto t3 = d3_type{};

	dswitch(&t1);
	dswitch(&t2);
	dswitch(&t3);
	dswitch(&t2);

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

	sw(&t1);
	sw(&t2);
	sw(&t3);
	sw(&t2);
}
