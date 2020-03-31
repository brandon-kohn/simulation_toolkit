#pragma once

#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <boost/math/distributions/normal.hpp>
#include <random>
#include <cmath>
#include <istream>
#include <ostream>

namespace stk {

    template<typename T = double>
    class linear_distribution
    {
    public:

        using result_type = T;

        struct param_type
        {
            using distribution_type = linear_distribution<T>;

            explicit param_type(T xmin, T xmax, T ymin, T ymax)
                : m_xmin(xmin)
                , m_xmax(xmax)
                , m_ymin(ymin)
                , m_ymax(ymax)
            {
                GEOMETRIX_ASSERT(xmin <= xmax);
                GEOMETRIX_ASSERT(ymin <= ymax);
            }

            bool operator ==(const param_type& rhs) const
            {
                return m_xmin == rhs.m_xmin && m_xmax == rhs.m_xmax;
                    && m_ymin == rhs.m_ymin && m_ymax == rhs.m_ymax;
            }

            bool operator !=(const param_type& right) const
            {
                return !(*this == right);
            }

            T xmin() const { return m_xmin; }
            T xmax() const { return m_xmax; }
            T ymin() const { return m_ymin; }
            T ymax() const { return m_ymax; }

        private:

            T m_xmin;
            T m_xmax;
            T m_ymin;
            T m_ymax;

        };

        explicit linear_distribution(T xmin, T xmax, T ymin, T ymax)
            : m_parameters(xmin, xmax, ymin, ymax)
        {

        }

        explicit linear_distribution(const param_type& params)
            : m_parameters(params)
        {
        }

        T xmin() const { return m_parameters.xmin(); }
        T xmax() const { return m_parameters.xmax(); }
        T ymin() const { return m_parameters.ymin(); }
        T ymax() const { return m_parameters.ymax(); }

        param_type param() const { return m_parameters; }

        void param(const param_type& params) { m_parameters = params; }

        result_type(min)() const { return m_parameters.xmin(); } 
        result_type(max)() const { return m_parameters.xmax(); } 
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

            std::uniform_real_distribution<T> U;
            auto x = U(e);
            auto y0 = params.ymin();
            auto y1 = params.ymax();

			if (y0 != y1)
				x = static_cast<T>((sqrt(y0 * y0 * (1.0 - x) + y1 * y1 * x) - y0) / (y1 - y0));

            auto x0 = params.xmin();
            auto x1 = params.xmax();
			return x0 + x * (x1 - x0);
        }

        template<typename CharT, typename Traits>
        std::basic_istream<CharT, Traits>& read(std::basic_istream<CharT, Traits>& str)
        {
            T xmin, xmax, ymin, ymax;
            str >> xmin;
            str >> xmax;
            str >> ymin;
            str >> ymax;
            m_parameters = param_type(xmin, xmax, ymin, ymax);
            return str;
        }

        template<typename CharT, typename Traits>
        std::basic_ostream<CharT, Traits>& write(std::basic_ostream<CharT, Traits>& str) const
        {
            str << m_parameters.m_xmin;
            str << m_parameters.m_xmax;
            str << m_parameters.m_ymin;
            str << m_parameters.m_ymax;
            return str;
        }

    private:

        param_type m_parameters;

    };

    template<typename T>
    inline bool operator ==(const linear_distribution<T>& l, const linear_distribution<T>& r)
    {
        return (l.param() == r.param());
    }

    template<typename T>
    inline bool operator !=(const linear_distribution<T>& l, const linear_distribution<T>& r)
    {
        return !(l == r);
    }

    template<typename CharT, typename Traits, typename T>
    inline std::basic_ostream<CharT, Traits>& operator <<(std::basic_ostream<CharT, Traits>& str, const linear_distribution<T>& dist)
    {
        return dist.write(str);
    }

    template<typename CharT, typename Traits, typename T>
    inline std::basic_istream<CharT, Traits>& operator >>( std::basic_istream<CharT, Traits>& str, linear_distribution<T>& dist)
    {
        return dist.read(str);
    }

}//! namespace stk;

