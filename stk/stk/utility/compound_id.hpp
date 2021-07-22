//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/utility/make_integer_range.hpp>
#include <cstdint>
#include <type_traits>
#include <limits>
#include <bitset>
#include <array>

namespace stk {

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
		static constexpr std::bitset<NumberBits> make_mask( [[maybe_unused]] const std::integer_sequence<std::uint8_t, Indices...>& _ )
		{
			return ( ( static_cast<U>( 1 ) << Indices ) | ... | static_cast<U>( 0 ) );
		}
		
		template <std::size_t I, typename Tuple>
		static void assign( std::bitset<NumberBits>& bits, Tuple&& values )
		{
			using std::get;

			static_assert( I < NumberElements + 1, "index is out of bounds" );
			if constexpr( I == 0 )
			{
				auto m = make_mask( make_integer_range<std::uint8_t, 0, get_index( 0 )>() );
				bits &= ~m;
				bits |= ( std::bitset<NumberBits>( get<I>(values) ) & m );
			}
			else if constexpr( I == NumberElements ) 
			{
				auto m = make_mask( make_integer_range<std::uint8_t, get_index( I - 1 ), NumberBits>() );
				bits &= ~m;
				bits |= ( std::bitset<NumberBits>( get<I>(values) ) << get_index( I - 1 ) & m );
			}
			else
			{
				auto m = make_mask( make_integer_range<std::uint8_t, get_index( I - 1 ), get_index( I )>() );
				bits &= ~m;
				bits |= ( std::bitset<NumberBits>( get<I>(values) ) << get_index( I - 1 ) & m );
			}
		}

		template <typename Tuple, std::size_t... I>
		static std::bitset<NumberBits> create(Tuple&& values, std::index_sequence<I...>)
		{
			auto bits = std::bitset<NumberBits>{};

			((void)assign<I>( bits, values ),...);

			return bits;
		}


	public:
	
		template <typename... Args>
		compound_id_impl(Args... a)
			: m_data{ create( std::make_tuple( a... ), std::index_sequence_for<Args...>{} ) }
		{

		}

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

		U value() const { return static_cast<U>( m_data.to_ullong() ); }
		std::bitset<NumberBits> const& data() const { return m_data; }

	private:

		friend bool operator<=( const compound_id_impl<U, MaskIndices...>& x, const compound_id_impl<U, MaskIndices...>& y ) { return x.m_data.to_ullong() <= y.m_data.to_ullong(); }
		friend bool operator>=( const compound_id_impl<U, MaskIndices...>& x, const compound_id_impl<U, MaskIndices...>& y ) { return x.m_data.to_ullong() >= y.m_data.to_ullong(); }
		friend bool operator>( const compound_id_impl<U, MaskIndices...>& x, const compound_id_impl<U, MaskIndices...>& y ) { return x.m_data.to_ullong() > y.m_data.to_ullong(); }
		friend bool operator<( const compound_id_impl<U, MaskIndices...>& x, const compound_id_impl<U, MaskIndices...>& y ) { return x.m_data.to_ullong() < y.m_data.to_ullong(); }
		friend bool operator==( const compound_id_impl<U, MaskIndices...>& x, const compound_id_impl<U, MaskIndices...>& y ) { return x.m_data.to_ullong() == y.m_data.to_ullong(); }
		friend bool operator!=( const compound_id_impl<U, MaskIndices...>& x, const compound_id_impl<U, MaskIndices...>& y ) { return x.m_data.to_ullong() != y.m_data.to_ullong(); }

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

	template <typename std::uint8_t ... Indices>
	using compound_id = compound_id_impl<std::uint64_t, Indices...>;

}//! namespace stk;
