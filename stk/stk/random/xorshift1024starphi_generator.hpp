//
//! Copyright © 2008-2011
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_XORSHIFT1024STARPHI_GENERATOR_HPP
#define STK_XORSHIFT1024STARPHI_GENERATOR_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <geometrix/utility/assert.hpp>
#include <boost/config.hpp>
#include <array>
#include <random>
#include <cstdint>

namespace stk {

    //! PRNG            Period      Failures (std)  Failures (rev)  Overall Systematic  ns/64 bits  cycles/B
    //! xoroshiro128+   2^128 − 1   31              27              58      —           0.81        0.36
    //! xorshift128+    2^128 − 1   38              32              70      —           1.02        0.46
    //! xorshift1024*φ  2^1024 − 1  37              39              76      —           1.21        0.55
    //! MT19937-64      2^19937 − 1 258             258             516     LinearComp  2.55        1.15
    class xorshift1024starphi_generator
    {
    public:

        xorshift1024starphi_generator(std::uint64_t seed = 42)
        {
            set_seed(seed);
        }

        using result_type = std::uint64_t;

        static BOOST_CONSTEXPR result_type min BOOST_PREVENT_MACRO_SUBSTITUTION(){ return 0; }
        static BOOST_CONSTEXPR result_type max BOOST_PREVENT_MACRO_SUBSTITUTION(){ return (std::numeric_limits<result_type>::max)(); }

        BOOST_FORCEINLINE result_type operator()()
        {
            const auto s0 = m_state[m_index];
            auto s1 = m_state[m_index = (m_index + 1) & 15];
            s1 ^= s1 << 31; // a
            m_state[m_index] = s1 ^ s0 ^ (s1 >> 11) ^ (s0 >> 30); // b,c
            const auto r = m_state[m_index] * 0x9e3779b97f4a7c13;
            GEOMETRIX_ASSERT(r >= (min)() && r <= (max)());
            return r;
        }
        
        void set_seed(std::uint64_t seed = 42)
        {
            std::seed_seq seq{seed};
            seq.generate(m_state.begin(), m_state.end());
        }

    private:

        std::array<std::uint64_t, 16> m_state;
        unsigned int m_index{0};

    };

}//! stk;

#endif//! STK_XORSHIFT1024STARPHI_GENERATOR_HPP
