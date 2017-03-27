//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_NULL_DELETER_HPP
#define STK_NULL_DELETER_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

struct null_deleter
{
    template <typename T>
    void operator()(T) const {}
};

#endif//STK_NULL_DELETER_HPP
