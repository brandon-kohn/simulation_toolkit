//
//! Copyright © 2025
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdint>
#include <functional>

namespace stk {

	//! A fast, high-quality integer mixing hash function based on SplitMix64.
	struct split_mix_hash
	{
		std::uint64_t seed = 0x9E3779B97F4A7C15ULL; //! Default seed: golden ratio constant.

		//! Default constructor (uses fixed golden ratio seed).
		constexpr split_mix_hash() noexcept = default;

		//! Explicit constructor to allow deterministic variation between hashers.
		constexpr explicit split_mix_hash( std::uint64_t s ) noexcept
			: seed( s )
		{
		}

		template <typename T>
		constexpr std::size_t operator()( const T& v ) const noexcept
		{
			std::uint64_t z = static_cast<std::uint64_t>( std::hash<T>{}( v ) ) + seed;
			z = ( z ^ ( z >> 30 ) ) * 0xbf58476d1ce4e5b9ULL;
			z = ( z ^ ( z >> 27 ) ) * 0x94d049bb133111ebULL;
			z ^= ( z >> 31 );
			return static_cast<std::size_t>( z );
		}
	};

} //! namespace stk;

