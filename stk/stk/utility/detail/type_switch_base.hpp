//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdint>
#include <type_traits>

namespace stk { namespace detail {

	template <typename Derived, std::size_t N>
	struct type_switch_base;

    template <typename U>
    inline std::intptr_t vtbl(const U* x)
    {
        static_assert(std::is_polymorphic<U>::value, "U must be a polymorphic type.");
        return *reinterpret_cast<const std::intptr_t*>(x);
    };
 
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

}}//! namespace stk::detail;

