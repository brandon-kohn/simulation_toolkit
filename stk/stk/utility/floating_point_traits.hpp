//
//! Copyright © 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/config.hpp>

#include <cstdint>
#include <cmath>
#include <ostream>
#include <bitset>

namespace stk {

	template <typename T, typename EnableIf=void>
	struct floating_point_traits;

	template <>
	struct floating_point_traits<double>
	{
		BOOST_STATIC_CONSTEXPR std::uint8_t mantissa = 52;
		BOOST_STATIC_CONSTEXPR std::uint8_t exponent = 11;
		BOOST_STATIC_CONSTEXPR std::uint8_t signbit  = 1;
		BOOST_STATIC_CONSTEXPR std::uint16_t exponent_bias = 1023;
	};
	
	template <>
	struct floating_point_traits<float>
	{
		BOOST_STATIC_CONSTEXPR std::uint8_t mantissa = 23;
		BOOST_STATIC_CONSTEXPR std::uint8_t exponent = 8;
		BOOST_STATIC_CONSTEXPR std::uint8_t signbit  = 1;
		BOOST_STATIC_CONSTEXPR std::uint8_t exponent_bias = 127;
	};

	//! Assumes little-endian.
	template <typename T>
	struct floating_point_components
	{
		using value_type = typename std::decay<T>::type;
		constexpr floating_point_components( value_type v = value_type{} )
			: value{ v }
		{}

		bool get_sign_bit() const { return signbit; }
		value_type get_exponent() const { return (int)( exponent - floating_point_traits<value_type>::exponent_bias ); }
		value_type get_mantissa() const 
		{
			auto v = value_type{1};
			std::bitset<floating_point_traits<value_type>::mantissa> mbits( mantissa );
			for( int i = floating_point_traits<value_type>::mantissa; i > 0; --i ) 
				if(mbits[floating_point_traits<value_type>::mantissa - i])
					v += std::pow( 2.0, static_cast<value_type>( -i ) );
			return v;
		}

		void print_mantissa(std::ostream& os) const
		{
			std::bitset<floating_point_traits<value_type>::mantissa> v( mantissa );
			os << "{" << v << "} ";
			os << "(" << mantissa << ")";
		}
		
		void print_exponent(std::ostream& os) const
		{
			std::bitset<floating_point_traits<value_type>::exponent> v( exponent );
			os << "{" << v << "} ";
			os << "(" << (int)exponent - floating_point_traits<value_type>::exponent_bias << ")";
		}
		
		void print_signbit(std::ostream& os) const
		{
			std::bitset<floating_point_traits<value_type>::signbit> v( signbit );
			os << "{" << v << "} ";
			os << "(" << signbit << ")";
		}
		
		void print_bits(std::ostream& os) const
		{
			std::bitset<sizeof(std::uint64_t)*8> v( bits_value );
			os << "{" << v << "} ";
			os << "(" << bits_value << ")";
		}
		
		void print_reconstituted(std::ostream& os) const
		{
			auto v = value_type{};

			if (mantissa || exponent) {
				v = std::pow( 2.0, get_exponent() ) * get_mantissa(); 
			}

			if (get_sign_bit()) {
				v *= -1;
			}

			os << "model: [" << v << "]";
		}

		void print(std::ostream& os) const
		{
			os.precision( ofPrecision );
			os << "value: " << value;
			os << "\nsign bit: ";
			print_signbit( os );
			os << "\nexponent: ";
			print_exponent( os );
			os << "\nmantissa: ";
			print_mantissa( os );
			os << "\nbits: ";
			print_bits( os );
			os << "\n";
			print_reconstituted( os );
			os << "\n";
		}

		union
		{
			value_type value;
			std::uint64_t bits_value;
			struct
			{
				std::uint64_t mantissa : floating_point_traits<value_type>::mantissa;
				std::uint64_t exponent : floating_point_traits<value_type>::exponent;
				std::uint64_t  signbit : floating_point_traits<value_type>::signbit;
			};
		};
	};

	template <typename T>
	constexpr T truncate_mask( std::uint64_t Bit, T v )
	{
		GEOMETRIX_ASSERT(Bit + 2 < floating_point_traits<T>::mantissa);
		//! This is faster, but can't handle the MSB of the mantissa.
		auto fp = floating_point_components<T>{ v };
		fp.mantissa &= ~( ( 1ULL << (Bit + 1ULL) ) - 1ULL );
		return fp.value;
	}
	
	template <typename T>
	constexpr T truncate_shift( std::uint64_t Bit, T v )
	{
		//! Slower but handles all bits of mantissa.
		auto fp = floating_point_components<T>{ v };
		fp.mantissa = (fp.mantissa >> Bit) << Bit;
		return fp.value;
	}
	
	template <typename T>
	constexpr T truncate( std::uint64_t Bit, T v )
	{
		return truncate_shift( Bit, v );
	}

}//! namespace stk;
