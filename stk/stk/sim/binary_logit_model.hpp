//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_BINARY_LOGIT_MODEL_HPP
#define STK_BINARY_LOGIT_MODEL_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/units/probability.hpp>

template <typename UtilityModel>
struct binary_logit_model
{
    binary_logit_model()
    {}

    binary_logit_model(const UtilityModel& utility)
        : m_utility(utility)
    {}

    template <typename... Args>
    units::probability operator()(const Args&... a) const
    {
        using std::exp;

        auto result = 1.0 / ( 1.0 + exp(-m_utility(a...)));
        
        return result.value() * units::proportion;
    }

    UtilityModel m_utility;
};

template <typename Utility>
inline binary_logit_model<Utility> make_binary_logit_model(const Utility& u) 
{
    return binary_logit_model<Utility>(u); 
}

#endif//STK_BINARY_LOGIT_MODEL_HPP
