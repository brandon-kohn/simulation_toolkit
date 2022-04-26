//
//! Copyright © 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <geometrix/numeric/constants.hpp>

#include <GTE/Mathematics/ACosEstimate.h>
#include <GTE/Mathematics/ASinEstimate.h>
#include <GTE/Mathematics/ATanEstimate.h>
#include <GTE/Mathematics/CosEstimate.h>
#include <GTE/Mathematics/SinEstimate.h>
#include <GTE/Mathematics/TanEstimate.h>
#include <GTE/Mathematics/ExpEstimate.h>
#include <GTE/Mathematics/LogEstimate.h>

#include <stk/math/detail/atan2.hpp>

#include <type_traits>
#include <limits>

namespace stk {
    
    template <typename T>
    inline auto sqrt(T&& v) -> auto
    {
        using std::sqrt;
        return sqrt(v);
    }
   
    template <typename T>
	inline auto pow(T&& a, T&& b) -> auto
    {
        using std::pow;
        return pow(std::forward<T>(a), std::forward<T>(b));
    }

    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
    inline BOOST_CONSTEXPR T sin( T v )
	{
		return gte::SinEstimate<T>::DegreeRR<11>( v );
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    inline BOOST_CONSTEXPR double sin( T v )
	{
		return gte::SinEstimate<double>::DegreeRR<11>( v );
	}
    
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
    inline BOOST_CONSTEXPR T asin( T v )
	{
		return gte::ASinEstimate<T>::Degree<8>( v );
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    inline BOOST_CONSTEXPR double asin( T v )
	{
		return gte::ASinEstimate<double>::Degree<8>( v );
	}

    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
    inline BOOST_CONSTEXPR T cos( T v )
	{
		return gte::CosEstimate<T>::DegreeRR<8>( v );
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline BOOST_CONSTEXPR double cos( T v )
	{
		return gte::CosEstimate<double>::DegreeRR<8>( v );
	}

    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline BOOST_CONSTEXPR T acos( T v )
	{
		return gte::ACosEstimate<T>::Degree<8>( v );
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline BOOST_CONSTEXPR double acos( T v )
	{
		return gte::ACosEstimate<double>::Degree<8>( v );
	}
    
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline BOOST_CONSTEXPR T tan( T v )
	{
		return gte::TanEstimate<T>::DegreeRR<13>( v );
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline BOOST_CONSTEXPR double tan( T v )
	{
		return gte::TanEstimate<double>::DegreeRR<13>( v );
	}
	
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline BOOST_CONSTEXPR T exp(T v)
	{
		return gte::ExpEstimate<T>::DegreeRR<7>( v );
	}
	
    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline BOOST_CONSTEXPR double exp(T v)
	{
		return gte::ExpEstimate<double>::DegreeRR<7>( v );
	}
	
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline BOOST_CONSTEXPR T log(T v)
	{
		return gte::LogEstimate<T>::DegreeRR<8>( v );
	}
	
    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline BOOST_CONSTEXPR double log(T v)
	{
		return gte::LogEstimate<double>::DegreeRR<8>( v );
	}
	
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline BOOST_CONSTEXPR T log10(T v)
	{
		return gte::Log2Estimate<T>::DegreeRR<8>( v ) * static_cast<T>(0.30102999566);//(1.0 / 3.32192809489);
	}
	
    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline BOOST_CONSTEXPR double log10(T v)
	{
		return gte::Log2Estimate<double>::DegreeRR<8>( v ) * 0.30102999566;//(1.0 / 3.32192809489);
	}

	inline double atan( double v )
	{
		return ::stk::math::detail::atan( v );
	}
	
	inline double atan2( double y, double x )
	{
		if( x > 0.0 ) 
			return stk::atan( y / x );
		if( y > 0.0 )
			return geometrix::constants::half_pi<double>() - stk::atan( x / y );
		if( y < 0.0 )
			return -geometrix::constants::half_pi<double>() - stk::atan( x / y );
		if( x < 0.0 )
			return geometrix::constants::pi<double>() + stk::atan( y / x );
		return std::numeric_limits<double>::quiet_NaN();
	}

}//! namespace stk;
