//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <utility>

namespace stk {

	namespace detail {

		template<typename IntegralType, IntegralType Begin, typename ISeq>
		struct integer_range_impl;

		template<typename IntegralType, IntegralType Begin, IntegralType... N>
		struct integer_range_impl<IntegralType, Begin, std::integer_sequence<IntegralType, N...>>
		{
			using type = std::integer_sequence<IntegralType, N + Begin...>;
		};
	}//! namespace detail;

	template<typename IntegralType, IntegralType Begin, IntegralType End>
	using make_integer_range = typename detail::integer_range_impl<IntegralType, Begin, std::make_integer_sequence<IntegralType, End - Begin>>::type;

}//! namespace stk;
