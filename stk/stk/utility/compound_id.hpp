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
#include <array>

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

	template <typename U, std::uint8_t ... MaskIndices>
	class compound_id_impl
	{
		static_assert( std::is_integral<U>::value, "U must be an integral type." );

		static constexpr std::uint8_t NumberBits = std::numeric_limits<U>::digits;
		static constexpr std::size_t NumberElements = sizeof...( MaskIndices );

		static constexpr std::uint8_t get_index( std::size_t i )
		{
			return std::array<std::uint8_t, NumberElements>{ MaskIndices... }[i];
		}
		
		template <std::uint8_t ... Indices>
		static constexpr std::bitset<NumberBits> make_mask( const std::integer_sequence<std::uint8_t, Indices...>& _ )
		{
			return ( ( static_cast<U>( 1 ) << Indices ) | ... | static_cast<U>( 0 ) );
		}
		
		template <std::uint8_t ... Indices>
		static constexpr std::bitset<NumberBits> make_inv_mask( const std::integer_sequence<std::uint8_t, Indices...>& _ )
		{
			return ~make_mask( _ );
		}

	public:

		compound_id_impl() = default;
		template <typename T>
		compound_id_impl(T v)
			: m_data(v)
		{
			static_assert( std::is_convertible<T, U>::value, "Cannot construct compound_id from type T." );
		}
	
		/*
		template <typename... Args>
		compound_id_impl(Args... a)
			: m_data(v)
		{
			static_assert( sizeof...(Args) == NumberElements + 1, "Cannot construct compound_id; too few inputs." );
		}
		*/

		template <std::size_t I>
		U get() const
		{
			static_assert( I < NumberElements + 1, "index is out of bounds" );
			if constexpr( I == 0 )
			{
				auto x = ( m_data & make_mask( make_integer_range<std::uint8_t, 0, get_index( 0 )>() ) );
				return static_cast<U>(x.to_ullong());
			}
			else if constexpr( I == NumberElements ) 
			{
				auto x = ( m_data & make_mask( make_integer_range<std::uint8_t, get_index( I - 1 ), NumberBits>() ) ) >> get_index( I - 1 );
				return static_cast<U>(x.to_ullong());
			}
			else
			{
				auto x = ( m_data & make_mask( make_integer_range<std::uint8_t, get_index( I - 1 ), get_index( I )>() ) ) >> get_index( I - 1 );
				return static_cast<U>(x.to_ullong());
			}
		}
		
		template <std::size_t I>
		void set(U v)
		{
			static_assert( I < NumberElements + 1, "index is out of bounds" );
			if constexpr( I == 0 )
			{
				auto m = make_mask( make_integer_range<std::uint8_t, 0, get_index( 0 )>() );
				m_data &= ~m;
				m_data |= ( std::bitset<NumberBits>{ v } & m );
			}
			else if constexpr( I == NumberElements ) 
			{
				auto m = make_mask( make_integer_range<std::uint8_t, get_index( I - 1 ), NumberBits>() );
				m_data &= ~m;
				m_data |= ( std::bitset<NumberBits>{ v } << get_index( I - 1 ) & m );
			}
			else
			{
				auto m = make_mask( make_integer_range<std::uint8_t, get_index( I - 1 ), get_index( I )>() );
				m_data &= ~m;
				m_data |= ( std::bitset<NumberBits>{ v } << get_index( I - 1 ) & m );
			}
		}

	private:

		std::bitset<NumberBits> m_data;

	};

	template <std::size_t I, typename U, std::uint8_t... M>
	inline auto get(const compound_id_impl<U, M...>& id)
	{
		return id.template get<I>();
	}
	
	template <std::size_t I, typename T, typename U, std::uint8_t... M>
	inline auto set(T value, compound_id_impl<U, M...>& id)
	{
		id.template set<I>(value);
	}

}//! namespace stk;
