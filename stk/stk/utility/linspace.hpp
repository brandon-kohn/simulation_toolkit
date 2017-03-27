//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_LINSPACE_HPP
#define STK_LINSPACE_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <vector>

template <typename T>
inline T linstep(T start, T stop, unsigned int num, bool endpoint = true)
{
    if (endpoint) {
        if (num == 1) {
            return stop - start;
        }

        return (stop - start) / (static_cast<double>(num) - 1.0);
    }
    else {
        return (stop - start) / static_cast<double>(num);
    }
}

template <typename T>
inline std::vector<T> linspace(T start, T stop, unsigned int num, bool endpoint = true)
{
    if (num <= 0) {
        return std::vector<T>{};
    }

    auto step = linstep(start, stop, num, endpoint);
    auto result = std::vector<T>{};
    T x = start;
    for (unsigned int i = 0; i < num; ++i, x += step) {
        result.push_back(x);
    }
    return result;
}

#endif//STK_LINSPACE_HPP
