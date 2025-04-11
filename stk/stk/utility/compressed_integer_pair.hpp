//
//! Copyright © 2017
//! Brandon Kohn
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_UTILITY_COMPRESSED_INTEGER_PAIR_HPP
#define STK_UTILITY_COMPRESSED_INTEGER_PAIR_HPP
#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace stk {

	template <typename A, typename B>
	struct compressed_pair
	{
		A first;
		B second;

		// Ensure both types are 4 bytes each.
		static_assert( sizeof( A ) == 4, "Type A must be exactly 4 bytes." );
		static_assert( sizeof( B ) == 4, "Type B must be exactly 4 bytes." );

		compressed_pair() = default;

		compressed_pair( A a, B b )
			: first( a )
			, second( b )
		{}

		explicit compressed_pair( std::uint64_t packed )
		{
			// Unpack the 64-bit integer into two 32-bit integers.
			std::uint32_t first_bits = static_cast<std::uint32_t>( packed >> 32 );
			std::uint32_t second_bits = static_cast<std::uint32_t>( packed & 0xFFFFFFFF );
			// Use memcpy to safely copy the memory representation.
			std::memcpy( &first, &first_bits, sizeof( first ) );
			std::memcpy( &second, &second_bits, sizeof( second ) );
		}

		compressed_pair( const std::pair<A,B>& other )
			: first( other.first )
			, second( other.second )
		{}

		auto operator<=>( const compressed_pair& rhs ) const = default;

		// Pack the bitwise representations of both values into a single std::uint64_t.
		inline std::uint64_t to_uint64() const
		{
			// Buffers to hold the bit representations.
			std::uint32_t first_bits;
			std::uint32_t second_bits;

			// Use memcpy to safely copy the memory representation.
			std::memcpy( &first_bits, &first, sizeof( first ) );
			std::memcpy( &second_bits, &second, sizeof( second ) );

			// Pack into one 64-bit integer: 'first' is the high part, 'second' the low part.
			return ( static_cast<std::uint64_t>( first_bits ) << 32 ) | second_bits;
		}

		explicit operator bool() const
		{
			return to_uint64() != 0;
		}
	};

	// Specialization for compressed_pair<uint32_t, uint32_t>
	template <>
	struct compressed_pair<std::uint32_t, std::uint32_t>
	{
		std::uint32_t first;
		std::uint32_t second;

		inline bool operator<( const compressed_pair& rhs ) const
		{
			return to_uint64() < rhs.to_uint64();
		}

		inline std::uint64_t to_uint64() const
		{
			// Direct combination without memcpy overhead.
			return ( static_cast<std::uint64_t>( first ) << 32 ) | second;
		}

		explicit operator bool() const
		{
			return to_uint64() != 0;
		}
	};

	using compressed_integer_pair = compressed_pair<std::uint32_t, std::uint32_t>;

	static_assert( std::is_standard_layout_v<compressed_integer_pair> && std::is_trivial_v<compressed_integer_pair>,
		"compressed_integer_pair should be a POD of size 8 bytes." );
	static_assert( sizeof( compressed_integer_pair ) == 8, "compressed_integer_pair should be a POD of size 8 bytes." );

	inline bool operator==( const compressed_integer_pair& lhs, const compressed_integer_pair& rhs )
	{
		return lhs.to_uint64() == rhs.to_uint64();
	}

	inline bool operator!=( const compressed_integer_pair& lhs, const compressed_integer_pair& rhs )
	{
		return lhs.to_uint64() != rhs.to_uint64();
	}

} // namespace stk

namespace std {
	template <>
	struct hash<stk::compressed_integer_pair>
	{
		std::size_t operator()( const stk::compressed_integer_pair& p ) const
		{
			return std::hash<std::uint64_t>()( p.to_uint64() );
		}
	};

	// If you wish to support hash for any compressed_pair instance (for types exactly 4 bytes),
	// you could add further specialization or overloads.
} // namespace std

#endif // STK_UTILITY_COMPRESSED_INTEGER_PAIR_HPP
