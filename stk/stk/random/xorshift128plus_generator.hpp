//
//! Copyright © 2017-2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
/*
Written in 2014-2016 by Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.
*/
#ifndef STK_XORSHIFT_GENERATOR_HPP
#define STK_XORSHIFT_GENERATOR_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/utility/assert.hpp>
#include <array>
#include <random>
#include <cstdint>

namespace stk {

    //! PRNG            Period      Failures (std)  Failures (rev)  Overall Systematic  ns/64 bits  cycles/B
    //! xoroshiro128+   2^128 − 1   31              27              58      —           0.81        0.36
    //! xorshift128+    2^128 − 1   38              32              70      —           1.02        0.46
    //! xorshift1024*φ  2^1024 − 1  37              39              76      —           1.21        0.55
    //! MT19937-64      2^19937 − 1 258             258             516     LinearComp  2.55        1.15
    class xorshift128plus_generator
    {
    public:

        static const std::uint64_t default_seed = 42ULL;

        xorshift128plus_generator(std::uint64_t seed = default_seed)
        {
            this->seed(seed);
        }

        using result_type = std::uint64_t;

        static BOOST_CONSTEXPR result_type min(){ return 0; }
        static BOOST_CONSTEXPR result_type max(){ return (std::numeric_limits<result_type>::max)(); }

        BOOST_FORCEINLINE result_type operator()
        {
            auto s1 = m_state[0];
            const auto s0 = m_state[1];
            const auto r = s0 + s1;
            m_state[0] = s0;
            s1 ^= s1 << 23; // a
            m_state[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5); // b, c
            GEOMETRIX_ASSERT(r >= (min)() && r <= (max)());
           return r;
        }
        
        void seed(std::uint64_t seed = 42)
        {
            std::uint32_t temp[4];
            std::seed_seq seq{seed};
            seq.generate(temp, temp + 4);
            m_state[0] = reinterpret_cast<std::uint64_t*>(temp)[0];
            m_state[1] = reinterpret_cast<std::uint64_t*>(temp)[1];
        }

    private:
    
        std::array<std::uint64_t, 2> m_state;
    };

}//! stk;

#endif//! STK_XORSHIFT_GENERATOR_HPP
