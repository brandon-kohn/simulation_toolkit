//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/hana.hpp>
#include <cstdint>
#include <type_traits>
#include <limits>
#include <bitset>

namespace stk {

	detail{
		template <typename U, unsigned char ... MaskIndices>
		constexpr U make_mask()
		{
			static_assert( std::is_integral<U>::value, "U must be an integral type." )
			return ( ( static_cast<U>( 1 ) << MaskIndices ) | ... | static_cast<U>( 0 ) );
		}

		template<typename IntegralType, IntegralType Begin, IntegralType... N>
		struct integer_range_impl
		{
			using type = std::integer_sequence<IntegralType, N + Begin...>;
		};
	}//! namespace detail;

	template<typename IntegralType, IntegralType Begin, IntegralType End>
	using make_integer_range = typename detail::integer_range_impl<IntegralType, Begin, std::make_integer_sequence<IntegralType, End - Begin>>::type;

	template <typename U, unsigned char ... MaskIndices>
	class compound_id_impl
	{
		static_assert( std::is_integral<U>::value, "U must be an integral type." );

		static constexpr std::size_t NumberBits = std::numeric_limits<U>::digits;
		static constexpr std::size_t NumberElements = sizeof( MaskIndices )...;

		static constexpr std::size_t get_index( std::size_t i )
		{
			return std::array<unsigned char, NumberElements>{ MaskIndices... }[i];
		}

	public:

		compound_id_impl() = default;

		template <typename T>
		compound_id_impl(T v)
			: m_data(v)
		{
			static_assert( std::is_convertible<T, U>::value, "Cannot construct compound_id from type T." );
		}

		template <std::size_t I>
		constexpr U get() const
		{
			constexpr U x = m_data & make_mask<U, make_integer_range<unsigned char, get_index(I), get_index(I+1)> >();
			return x;
		}

	private:

		std::bitset<NumberBits> m_data;

	};

	template <std::size_t I, typename U, unsigned char... M>
	constexpr auto get(const compound_id_impl<U, M...>& id)
	{
		return id.template get<I>();
	}

}//! namespace stk;
