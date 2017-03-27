//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_BOOST_UNIQUE_PTR_HPP
#define STK_BOOST_UNIQUE_PTR_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <boost/move/unique_ptr.hpp>
#include <boost/move/make_unique.hpp>

namespace boost
{
    //! boost::unique_ptr makes more sense for maintenence and eventually moving to std::unique_ptr. Hoisted it.
    using movelib::unique_ptr;
    using movelib::make_unique;
}

#endif//STK_BOOST_UNIQUE_PTR_HPP
