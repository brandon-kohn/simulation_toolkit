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

#include <stk/math/detail/exp.hpp>
#include <stk/math/detail/pow.hpp>
#include <stk/math/detail/log.hpp>
#include <stk/math/detail/log10.hpp>
#include <stk/math/detail/sin.hpp>
#include <stk/math/detail/asin.hpp>
#include <stk/math/detail/cos.hpp>
#include <stk/math/detail/acos.hpp>
#include <stk/math/detail/tan.hpp>
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
    inline T sin( T v )
	{
		return static_cast<T>(stk::math::detail::sin(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    inline double sin( T v )
	{
		return static_cast<double>(stk::math::detail::sin(static_cast<double>(v)));
	}
    
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
    inline T asin( T v )
	{
		return static_cast<T>(stk::math::detail::asin(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    inline double asin( T v )
	{
		return static_cast<double>(stk::math::detail::asin(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
    inline T cos( T v )
	{
		return static_cast<T>(stk::math::detail::cos(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double cos( T v )
	{
		return static_cast<double>(stk::math::detail::cos(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline T acos( T v )
	{
		return static_cast<T>(stk::math::detail::acos(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double acos( T v )
	{
		return static_cast<double>(stk::math::detail::acos(static_cast<double>(v)));
	}
    
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline T tan( T v )
	{
		return static_cast<T>(stk::math::detail::tan(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double tan( T v )
	{
		return static_cast<double>(stk::math::detail::tan(static_cast<double>(v)));
	}
	
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline T exp(T v)
	{
        return static_cast<T>(stk::math::detail::exp(static_cast<double>(v)));
	}
	
    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double exp(T v)
	{
        return static_cast<double>(stk::math::detail::exp(static_cast<double>(v)));
	}
	
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline T log(T v)
	{
        return static_cast<T>(stk::math::detail::log(static_cast<double>(v)));
	}
	
    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double log(T v)
	{
        return static_cast<double>(stk::math::detail::log(static_cast<double>(v)));
	}
	
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline T log10(T v)
	{
        return static_cast<T>(stk::math::detail::log10(static_cast<double>(v)));
	}
	
    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double log10(T v)
	{
        return static_cast<double>(stk::math::detail::log10(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline T atan( T v )
	{
        return static_cast<T>(stk::math::detail::atan(static_cast<double>(v)));
	}

    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double atan( T v )
	{
        return static_cast<double>(stk::math::detail::atan(static_cast<double>(v)));
	}
	
    template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
	inline T atan2( T y, T x )
	{
		return static_cast<T>(::stk::math::detail::atan2( static_cast<double>(y), static_cast<double>(x) ));
	}
    
    template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	inline double atan2( T y, T x )
	{
		return static_cast<double>(::stk::math::detail::atan2( static_cast<double>(y), static_cast<double>(x) ));
	}

}//! namespace stk;
