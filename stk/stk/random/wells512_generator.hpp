//
//! Copyright © 2008-2011
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_WELLS512_GENERATOR_HPP
#define STK_WELLS512_GENERATOR_HPP
#pragma once

#include <geometrix/utility/assert.hpp>
#include <array>
#include <random>
#include <cstdint>

namespace stk {

    class wells512_generator
    {
    public:

        wells512_generator(unsigned int seed = 42)
        {
            this->seed(seed);
        }

        using result_type = std::uint32_t;

        static BOOST_CONSTEXPR result_type min BOOST_PREVENT_MACRO_SUBSTITUTION (){ return 0; }
        static BOOST_CONSTEXPR result_type max BOOST_PREVENT_MACRO_SUBSTITUTION (){ return (std::numeric_limits<result_type>::max)(); }

        BOOST_FORCEINLINE result_type operator()()
        {
           std::uint32_t a = m_state[index];
           std::uint32_t c = m_state[(index+13)&15];
           std::uint32_t b = a^c^(a<<16)^(c<<15);
           c = m_state[(index+9)&15];
           c ^= (c>>11);
           a = m_state[index] = b^c;
           std::uint32_t d = a^((a<<5)&0xDA442D24UL);
           index = (index + 15)&15;
           a = m_state[index];
           m_state[index] = a^b^d^(a<<2)^(b<<18)^(c<<28);
           
           auto r = m_state[index];
           GEOMETRIX_ASSERT(r >= well512_generator::min() && r <= well512_generator::max());
           return r;
        }
        
        void seed(unsigned int seed = 42)
        {
            std::seed_seq seq{seed};
            seq.generate(m_state.begin(), m_state.end());
        }

    private:
    
        std::array<std::uint32_t, 16> m_state;
        unsigned int                  m_index{0};
    };

}//! stk;

#endif//! STK_WELLS512_GENERATOR_HPP
