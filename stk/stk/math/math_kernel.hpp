//
//! Copyright © 2022
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/math/tagged_quantity_math_approx.hpp>
#include <stk/math/boost_units_math_approx.hpp>

namespace stk {

    struct math_kernel
    {
        template<typename T>
        BOOST_STATIC_CONSTEXPR auto exp(T&& q) -> auto
        {
            return stk::exp(std::forward<T>(q));
        }

        template<typename T>
        BOOST_STATIC_CONSTEXPR auto log(T&& q) -> auto
        {
            return stk::log(std::forward<T>(q));
        }

        template<typename T>
        BOOST_STATIC_CONSTEXPR auto log10(T&& q) -> auto
        {
            return stk::log10(std::forward<T>(q));
        }

        template <typename T>
        static auto sqrt(T&& q) -> auto
        {
            return stk::sqrt(std::forward<T>(q));
        }

        template<typename T>
        BOOST_STATIC_CONSTEXPR auto cos(T&& q) -> auto
        {
            return stk::cos(std::forward<T>(q));
        }

        template<typename T>
        BOOST_STATIC_CONSTEXPR auto sin(T&& q) -> auto
        {
            return stk::sin(std::forward<T>(q));
        }

        template<typename T>
        BOOST_STATIC_CONSTEXPR auto tan(T&& q) -> auto
        {
            return stk::tan(std::forward<T>(q));
        }

        template<typename T>
        BOOST_STATIC_CONSTEXPR auto acos(T&& q) -> auto
        {
            return stk::acos(std::forward<T>(q));
        }

        template<typename T>
        BOOST_STATIC_CONSTEXPR auto asin(T&& q) -> auto
        {
            return stk::asin(std::forward<T>(q));
        }

        template<typename T>
        static auto atan(T&& q) -> auto
        {
            return stk::atan(std::forward<T>(q));
        }

        template<typename T>
        static auto pow(T&& q1, T&& q2) -> auto
        {
            return stk::pow(std::forward<T>(q1), std::forward<T>(q2));
        }
        
        template<typename T>
        static auto atan2(T&& y, T&& x) -> auto
        {
            return stk::atan2(std::forward<T>(y), std::forward<T>(x));
        }
    };

}//! namespace stk;

