//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_TOLERANCE_POLICY_HPP
#define STK_TOLERANCE_POLICY_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/units/boost_units.hpp>

//! Create access to number comparison policy
#include <geometrix/numeric/number_comparison_policy.hpp>

namespace stk {
    
using tolerance_policy = geometrix::mapped_tolerance_comparison_policy<geometrix::absolute_tolerance_comparison_policy<double>, boost::fusion::pair<units::angle, geometrix::relative_tolerance_comparison_policy<double>>>;

inline tolerance_policy make_tolerance_policy(double generalTol = 1e-10, double angleTol = 1e-6)
{
    using namespace geometrix;
    return tolerance_policy{ absolute_tolerance_comparison_policy<double>(generalTol), boost::fusion::make_pair<units::angle>(relative_tolerance_comparison_policy<double>(angleTol)) };
}

}//! namespace stk;

#endif//STK_TOLERANCE_POLICY_HPP
