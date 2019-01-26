#pragma once

#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <boost/math/distributions/normal.hpp>
#include <random>
#include <cmath>

namespace stk {

    template<typename T = double>
	class truncated_normal_distribution
	{	
    public:
	    
		using result_type = T;

	    struct param_type
		{
            using distribution_type = truncated_normal_distribution<T>;

            explicit param_type(T mean, T sigma, T lower, T upper)
                : m_mean(mean)
                , m_sigma(sigma)
                , m_min(lower)
                , m_max(upper)
            {
                boost::math::normal_distribution<T> dist(mean, sigma);
                m_min_quantile = boost::math::cdf(dist, m_min);
                m_max_quantile = boost::math::cdf(dist, m_max);
            }

			bool operator ==(const param_type& rhs) const
            {	
                return m_mean == m_right.m_mean && m_sigma == m_right.m_sigma;
            }

            bool operator !=(const param_type& m_right) const
            {
                return !(*this == m_right);
            }

            T mean() const { return m_mean; }
            T sigma() const { return m_sigma; }
            T lower() const { return m_min; }
            T upper() const { return m_max; }
            T lower_quantile() const { return m_min_quantile; }
            T upper_quantile() const { return m_max_quantile; }
            
            bool is_standard_normal() const { return is_standard_normal(m_mean, m_sigma); }
            T scale_to_general(T x) const { return scale_to_general_normal(x, m_mean, m_sigma); }
            T standard_normal_lower() const { return scale_to_standard_normal(m_min, m_mean, m_sigma); }
            T standard_normal_upper() const { return scale_to_standard_normal(m_max, m_mean, m_sigma); }

        private:

			static bool is_standard_normal(T m, T s)
			{
				using namespace geometrix;
				return m == constants::zero<T>() && s == constants::one<T>();
			}

			static T scale_to_standard_normal(T x, T m, T s)
			{
				return (x - m) / s;
			}

			static T scale_to_general_normal(T x, T m, T s)
			{
				return x * s + m;
			}

            T m_mean;
            T m_sigma;
            T m_min;
            T m_max;
            T m_min_quantile;
            T m_max_quantile;

		};

    	explicit truncated_normal_distribution(T lower, T upper, T mean = 0, T sigma = 1) 
		    : m_parameters(mean, sigma, lower, upper)
		{}

	    explicit truncated_normal_distribution(const param_type& params)
	    	: m_parameters(params)
		{}

	    T mean() const { return m_parameters.mean(); }

	    T stddev() const { return m_parameters.sigma(); }

	    param_type param() const { return m_parameters; }

	    void param(const param_type& params) { m_parameters = params; }

	    result_type (min)() const {	return numeric_limits<result_type>::denorm_min(); }

    	result_type (max)() const { return (numeric_limits<result_type>::max)(); }

	    void reset() { }

    	template<typename Engine>
		result_type operator()(Engine& e) 
        {
			return this->operator()(e, m_parameters);
		}

	    template<typename Engine>
		result_type operator()(Engine& e, const param_type& params)
		{
            std::uniform_real_distribution<T> U(params.lower_quantile(), params.upper_quantile());
            auto u = U(e);
			boost::math::normal_distribution<T> dist;
            auto r = boost::math::quantile(dist, u);
			if (m_parameters.is_standard_normal())
				return r;
			return params.scale_to_general(r);
		}

	    template<typename CharT, typename Traits>
		std::basic_istream<CharT, Traits>& read(std::basic_istream<CharT, Traits>& str)
		{
            T mean;
            T sigma;
            T a, b;
            str >> mean; 
            str >> sigma;
            str >> a;
            str >> b;
            m_parameters = param_type(mean, sigma, a, b);
            return str;
		}

	    template<typename CharT, typename Traits>
		std::basic_ostream<CharT, Traits>& write(std::basic_ostream<CharT, Traits>& str) const
		{
            str << m_parameters.m_mean;
            str << m_parameters.m_sigma;
            str << m_parameters.m_min;
            str << m_parameters.m_max;
		    return str;
		}

    private:

	    param_type m_parameters;

	};

    template<typename T>
	inline bool operator ==(const truncated_normal_distribution<T>& l, const truncated_normal_distribution<T>& r)
	{
	    return (l.param() == r.param());
	}

    template<typename T>
	inline bool operator !=(const truncated_normal_distribution<T>& l, const truncated_normal_distribution<T>& r)
	{	
	    return !(l == r);
	}

    template<typename CharT, typename Traits, typename T>
	inline std::basic_ostream<CharT, Traits>& operator <<(std::basic_ostream<CharT, Traits>& str, const truncated_normal_distribution<T>& dist)
	{
	    return dist.write(str);
	}

    template<typename CharT, typename Traits, typename T>
	inline std::basic_istream<CharT, Traits>& operator >>( std::basic_istream<CharT, Traits>& str, truncated_normal_distribution<T>& dist)
	{
	    return dist.read(str);
	}

}//! namespace stk;

