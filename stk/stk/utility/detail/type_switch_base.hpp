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

}}//! namespace stk::detail;

