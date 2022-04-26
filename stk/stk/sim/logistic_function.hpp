//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_LOGISTIC_FUNCTION_HPP
#define STK_LOGISTIC_FUNCTION_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/utility/tagged_quantity.hpp>
#include <geometrix/arithmetic/boost_units_arithmetic.hpp>
#include <geometrix/arithmetic/math_kernel.hpp>
#include <stk/units/boost_units.hpp>
#include <stk/units/probability.hpp>

namespace stk {

GEOMETRIX_STRONG_TYPEDEF(units::probability, LowerAsymptote);
GEOMETRIX_STRONG_TYPEDEF(units::probability, UpperAsymptote);
GEOMETRIX_STRONG_TYPEDEF(units::dimensionless, GrowthSkew);
GEOMETRIX_STRONG_TYPEDEF(units::dimensionless, InterceptCoef);

struct GrowthRateTag;
template <typename T>
using GrowthRate = geometrix::tagged_quantity<GrowthRateTag, T>;

template <typename X, typename GrowthRateType = typename units::inverse<X>::type, typename Math = geometrix::std_math_kernel>
struct logistic_function
{
    logistic_function()
    {}

    logistic_function(const LowerAsymptote& A, const UpperAsymptote& K, const GrowthRate<GrowthRateType>& B, const GrowthSkew& nu, const InterceptCoef& Q)
        : A(A)
        , K(K)
        , B(B)
        , v(1.0 / nu)
        , Q(Q)
    {}

    units::probability operator()(const X& x) const
    {
        auto result = A + (K - A) / Math::pow( 1.0 + Q * Math::exp(-B*x), v);
        return result.value();
    }

    const LowerAsymptote A{ 0.0 * units::proportion };//! lower asymptote
    const UpperAsymptote K{ 1.0 * units::proportion };//! upper asymptote
    const GrowthRate<GrowthRateType> B{ 0.001 };//! growth rate
    const GrowthSkew v{ 1.0 };//! near which asymptote maximum growth happens (inversed for performance v == 1.0 / nu)
    const InterceptCoef Q{ 0.1 };//! y-intercept coefficient
};

}//! namespace stk;

#endif//STK_LOGISTIC_FUNCTION_HPP
