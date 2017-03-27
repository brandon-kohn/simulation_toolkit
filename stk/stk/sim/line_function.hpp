//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_LINE_FUNCTION_HPP
#define STK_LINE_FUNCTION_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/utility/tagged_quantity.hpp>

GEOMETRIX_STRONG_TYPEDEF_TPL(Slope);
GEOMETRIX_STRONG_TYPEDEF_TPL(YIntercept);

template <typename T>
inline Slope<T> as_slope(T&& t) { return Slope<T>(std::forward<T>(t)); }

template <typename T>
inline YIntercept<T> as_yintercept(T&& t) { return YIntercept<T>(std::forward<T>(t)); }

template <typename X = double, typename Y = X>
struct line_function
{
    using x_type = X;
    using y_type = Y;
    using slope_type = decltype(std::declval<Y>() / std::declval<X>());
    
    line_function(const Slope<slope_type>& slope, const YIntercept<y_type>& intercept)
        : slope(slope)
        , intercept(intercept)
    {}

    y_type operator()(const x_type& x) const
    {
        return (slope * x + intercept).value();
    }

private:
    
    const Slope<slope_type>  slope;
    const YIntercept<y_type> intercept;

};

#endif//STK_LINE_FUNCTION_HPP
