//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_RANDOM_SEARCH_HPP
#define STK_RANDOM_SEARCH_HPP
#pragma once

#include <cmath>

template<typename State, typename CostFunction, typename NeighborFn>
inline State random_search(State currS, std::size_t k, const CostFunction& cost, const NeighborFn& neighbor) 
{   
    State sMin = currS;
    auto minCost = cost(currS);

    for (; k > 0; --k) {
        State sNext = neighbor(currS);
        auto nextCost = cost(sNext);

        if (nextCost < minCost) {//! Always accept something better.
            sMin = sNext;
            minCost = nextCost;
            currS = sNext;
        }
    }

    return sMin;
}

#endif//STK_RANDOM_SEARCH_HPP
