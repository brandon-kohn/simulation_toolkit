#pragma once

#include <geometrix/utility/assert.hpp>
#include <random>
#include <cmath>
#include <istream>
#include <ostream>

namespace stk {

    template<typename T = double>
    class maxwell_boltzmann_distribution
    {
    public:

        using result_type = T;

        struct param_type
        {
            using distribution_type = maxwell_boltzmann_distribution<T>;

            explicit param_type(T a)
                : m_a{a}
            {
            }

            bool operator ==(const param_type& rhs) const
            {
                return m_a == rhs.m_a;
            }

            bool operator !=(const param_type& right) const
            {
                return !(*this == right);
            }

            T a() const { return m_a; }
        private:

            T m_a;

        };

        explicit maxwell_boltzmann_distribution(T a)
            : m_parameters(a)
        {

        }

        explicit maxwell_boltzmann_distribution(const param_type& params)
            : m_parameters(params)
        {
        }

        T a() const { return m_parameters.a(); }
        param_type param() const { return m_parameters; }

        void param(const param_type& params) { m_parameters = params; }

        result_type(min)() const { return 0; }

        result_type(max)() const { return (std::numeric_limits<result_type>::max)(); }
        void reset() { }

        template<typename Engine>
        result_type operator()(Engine& e)
        {
            return this->operator()(e, m_parameters);
        }

        template<typename Engine>
        result_type operator()(Engine& e, const param_type& params)
        {
            using std::sqrt;
            using std::log;
            auto U = std::uniform_real_distribution<T>();
            
            #ifdef STK_USE_NADER_BOLTZMANN_DISTRIBUTION //! - NOTE: I have something wrong here.
            //! Mohamed, Nader. (2011). Efficient Algorithm for Generating Maxwell Random Variables. Journal of Statistical Physics. 145. 10.1007/s10955-011-0364-y. 
            auto g2 = 1.647 * 1.647;
            while(true)
            {
                auto r1 = U(e);
                auto r2 = U(e);
                auto y = -2.0 * log(r1);
                auto q = r1/r2;
                if(g2*y >= q*q)
                    return params.a() * sqrt(2.0 * y);
            }
            #else
            //! Use Johnk's algorithm from handbook on statistical distribution for experimentalists.
            auto r = -log(U(e));
            while (true) 
            {
                auto r1 = U(e);
                auto r2 = U(e);
                auto w1 = r1 * r1;
                auto w2 = r2 * r2;
                auto w = w1 + w2;
                if (w <= 1.0)
                {
                    r = r - (w1 / w) * log(U(e));
                    return params.a() * sqrt(2.0 * r);
                }
            }
            #endif
        }

        template<typename CharT, typename Traits>
        std::basic_istream<CharT, Traits>& read(std::basic_istream<CharT, Traits>& str)
        {
            T a;
            str >> a;
            m_parameters = param_type(a);
            return str;
        }

        template<typename CharT, typename Traits>
        std::basic_ostream<CharT, Traits>& write(std::basic_ostream<CharT, Traits>& str) const
        {
            str << m_parameters.m_a;
            return str;
        }

    private:

        param_type m_parameters;

    };

    template<typename T>
    inline bool operator ==(const maxwell_boltzmann_distribution<T>& l, const maxwell_boltzmann_distribution<T>& r)
    {
        return (l.param() == r.param());
    }

    template<typename T>
    inline bool operator !=(const maxwell_boltzmann_distribution<T>& l, const maxwell_boltzmann_distribution<T>& r)
    {
        return !(l == r);
    }

    template<typename CharT, typename Traits, typename T>
    inline std::basic_ostream<CharT, Traits>& operator <<(std::basic_ostream<CharT, Traits>& str, const maxwell_boltzmann_distribution<T>& dist)
    {
        return dist.write(str);
    }

    template<typename CharT, typename Traits, typename T>
    inline std::basic_istream<CharT, Traits>& operator >>( std::basic_istream<CharT, Traits>& str, maxwell_boltzmann_distribution<T>& dist)
    {
        return dist.read(str);
    }

}//! namespace stk;

