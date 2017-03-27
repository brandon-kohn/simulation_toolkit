//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_LINEAR_DATA_INTERPOLATOR_HPP
#define STK_LINEAR_DATA_INTERPOLATOR_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include "line_function.hpp"

template <typename X, typename Y>
struct linear_data_interpolator
{
    using x_type = X;
    using y_type = Y;
    using slope_type = decltype(std::declval<Y>() / std::declval<X>());
    
    linear_data_interpolator(const std::vector<x_type>& x, const std::vector<y_type>& y)
        : mXData(x)
        , mYData(y)
    {
        GEOMETRIX_ASSERT(x.size() > 1);
        GEOMETRIX_ASSERT(x.size() == y.size());
    }

    y_type operator()(const x_type& x) const
    {
        auto it = std::upper_bound(mXData.begin(), mXData.end(), x);
        std::size_t index = 
            it > mXData.begin() ?
                ( (it + 1) < mXData.end() ?
                    std::distance(mXData.begin(), (it-1)) :
                    mXData.size() - 2) :
            0;

        x_type xmin = mXData[index];
        x_type xmax = mXData[index + 1];
        y_type ymin = mYData[index];
        y_type ymax = mYData[index + 1];
        slope_type slope = (ymax - ymin) / (xmax - xmin);
        y_type yIntercept = ymin;
        
        return line_function<x_type, y_type>(as_slope(slope), as_yintercept(yIntercept))(x-xmin);
    }

private:
    
    const std::vector<x_type>   mXData;
    const std::vector<y_type>   mYData;

};

#endif//STK_LINEAR_DATA_INTERPOLATOR_HPP
