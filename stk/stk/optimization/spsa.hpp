//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_SPSA_HPP
#define STK_SPSA_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <cmath>
#include <geometrix/tensor/vector.hpp>

namespace spsa_detail
{
    template<typename T, T... Ints> 
    struct integer_sequence{};

    template<typename S> 
    struct next_integer_sequence;

    template<typename T, T... Ints> 
    struct next_integer_sequence<integer_sequence<T, Ints...>>
    {
        using type = integer_sequence<T, Ints..., sizeof...(Ints)>;
    };

    template<typename T, T I, T N>
    struct make_int_seq_impl;

    template<typename T, T N>
    using make_integer_sequence = typename make_int_seq_impl<T, 0, N>::type;

    template<typename T, T I, T N> 
    struct make_int_seq_impl
    {
        using type = typename next_integer_sequence<typename make_int_seq_impl<T, I + 1, N>::type>::type;
    };

    template<typename T, T N> 
    struct make_int_seq_impl<T, N, N>
    {
        using type = integer_sequence<T>;
    };

    template<std::size_t... Ints>
    using index_sequence = integer_sequence<std::size_t, Ints...>;

    template<std::size_t N>
    using make_index_sequence = make_integer_sequence<std::size_t, N>;

    template<typename State, typename Gen, std::size_t... I>
    inline State generate_impl(Gen gen, index_sequence<I...>)
    {
        return geometrix::construct<State>(gen(I)...);
    }

    template<typename State, typename Gen, typename Indices = make_index_sequence<geometrix::dimension_of<State>::value>>
    inline State generate(Gen gen)
    {
        return generate_impl<State>(gen, Indices());
    }

}//! spsa_detail;

// Simulated annealing ripped off from stack exchange.
template<std::size_t N, typename CostFunction, typename BernoulliGenerator>
inline geometrix::vector<double, N> runtime_spsa(geometrix::vector<double, N> theta, std::size_t k, const CostFunction& cost, BernoulliGenerator& rng, double A = 0.1, double a = 0.1, double c = 0.1, double alpha = 0.602, double gamma = 0.101, const geometrix::vector<double, N>& scale = spsa_detail::generate<geometrix::vector<double, N>>([](int) {return 1.0; }))
{
    using namespace geometrix;
    using std::pow;
    using State = geometrix::vector<double, N>;
    static_assert(std::is_same<decltype(rng()), bool>::value, "BernoulliGenerator must generate boolean values!");
    
    for (int step = 0; step < k; ++step) {
        auto ak = a / pow(step + 1.0 + A, alpha);
        auto ck = c / pow(step + 1.0, gamma);
        auto delta = spsa_detail::generate<State>([&rng, &scale](int i) { return (rng() ? 1.0 : -1.0) * scale[i]; });
        State ckdelta = ck * delta;
        State thetaPlus = theta + ckdelta;
        State thetaMinus = theta - ckdelta;
        auto yplus = cost(thetaPlus);
        auto yminus = cost(thetaMinus);     
        auto akydiff = ak * (yplus - yminus);
        theta = theta - akydiff * reciprocal((2.0 * ck) * delta);
    }

    return theta;
}

#endif//STK_SPSA_HPP
