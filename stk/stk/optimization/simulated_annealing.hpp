//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_SIMULATED_ANNEALING_HPP
#define STK_SIMULATED_ANNEALING_HPP
#pragma once

#include <cmath>

namespace simulated_annealing_detail
{
    template <int Step>
    struct anneal_step
    {
        template<typename State, typename Cost, typename CostFunction, typename TemperatureSchedule, typename NeighborFn, typename UniformRandomRealGenerator>
        static State apply(State currS, Cost lastCost, State& sMin, Cost& minCost, const CostFunction& cost, const TemperatureSchedule& temp, const NeighborFn& neighbor, UniformRandomRealGenerator& rng)
        {
            using std::exp;
            State sNext = neighbor(currS);
            auto nextCost = cost(sNext);

            if (nextCost < minCost) {//! Always accept something better.
                sMin = sNext;
                minCost = nextCost;
                currS = sNext;
                lastCost = nextCost;
            }
            else {
                auto t = temp(Step);
                auto delta = nextCost - lastCost;

                if (nextCost < lastCost || exp((-1.0 * delta) / t) > rng()) {
                    currS = sNext;
                    lastCost = nextCost;
                }
            }
            
            return anneal_step<Step-1>::apply(currS, lastCost, sMin, minCost, cost, temp, neighbor, rng);
        }
    };

    template <>
    struct anneal_step<0>
    {
        template<typename State, typename Cost, typename CostFunction, typename TemperatureSchedule, typename NeighborFn, typename UniformRandomRealGenerator>
        static State apply(State, Cost, State& sMin, Cost&, const CostFunction&, const TemperatureSchedule&, const NeighborFn&, UniformRandomRealGenerator&)
        {
            return sMin;
        }
    };
}

template<int K, typename State, typename CostFunction, typename TemperatureSchedule, typename NeighborFn, typename UniformRandomRealGenerator>
inline State simulated_annealing(State s0, const CostFunction& cost, const TemperatureSchedule& temp, const NeighborFn& neighbor, UniformRandomRealGenerator& rng)
{
    auto lastCost = cost(s0);
    State sMin = s0;
    auto minCost = lastCost;

    return simulated_annealing_detail::anneal_step<K>::apply(s0, lastCost, sMin, minCost, cost, temp, neighbor, rng);
}

// Simulated annealing ripped off from stack exchange.
template<typename State, typename CostFunction, typename TemperatureSchedule, typename NeighborFn, typename UniformRandomRealGenerator>
inline State runtime_simulated_annealing(State currS, std::size_t k, const CostFunction& cost, const TemperatureSchedule& temp, const NeighborFn& neighbor, UniformRandomRealGenerator& rng) 
{
    using std::exp;

    auto lastCost = cost(currS);
    State sMin = currS;
    auto minCost = lastCost;

    for (; k > 0; --k) {
        State sNext = neighbor(currS);
        auto nextCost = cost(sNext);

        if (nextCost < minCost) {//! Always accept something better.
            sMin = sNext;
            minCost = nextCost;
            currS = sNext;
            lastCost = nextCost;
            continue;
        }

        auto t = temp(k);
        auto delta = nextCost - lastCost;
        
        if (nextCost < lastCost || exp((-1.0 * delta) / t) > rng()) {
            currS = sNext;
            lastCost = nextCost;
        }
    }

    return sMin;
}

#endif//STK_SIMULATED_ANNEALING_HPP
