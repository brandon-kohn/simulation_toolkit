#pragma once

#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <boost/math/distributions/poisson.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <random>
#include <cmath>
#include <istream>
#include <ostream>

namespace stk {

    template<typename ResultType = int, typename T = double>
	class zero_truncated_poisson_distribution
	{	
    public:
	    
		using result_type = ResultType;

	    struct param_type
		{
            using distribution_type = zero_truncated_poisson_distribution<T>;

            explicit param_type(T mean)
                : m_mean(mean)
            {
				GEOMETRIX_ASSERT(mean > 0.0);
            }

			bool operator ==(const param_type& rhs) const
            {	
                return m_mean == rhs.m_mean;
            }

            bool operator !=(const param_type& right) const
            {
                return !(*this == right);
            }

            T mean() const { return m_mean; }

        private:

            T m_mean;

		};

    	explicit zero_truncated_poisson_distribution(T mean = 1)
		    : m_parameters(mean)
		{
        }

	    explicit zero_truncated_poisson_distribution(const param_type& params)
	    	: m_parameters(params)
		{
        }

	    T mean() const { return m_parameters.mean(); }
	   
	    param_type param() const { return m_parameters; }

	    void param(const param_type& params) { m_parameters = params; }

	    result_type (min)() const {	return std::numeric_limits<result_type>::denorm_min(); }

    	result_type (max)() const { return (std::numeric_limits<result_type>::max)(); }

	    void reset() { }

    	template<typename Engine>
		result_type operator()(Engine& e) 
        {
			return this->operator()(e, m_parameters);
		}

	    template<typename Engine>
		result_type operator()(Engine& e, const param_type& params)
		{
			using std::exp;//! TODO platform dependence here.
			using std::log;//! TODO platform dependence here.
			auto rp = exp(-params.mean());
            boost::random::uniform_real_distribution<T> U(rp, 1.0);
            auto u = U(e);
			auto t = -log(u);
			boost::random::poisson_distribution<ResultType, T> dist(params.mean()-t);
			return 1 + dist(e);
		}

	    template<typename CharT, typename Traits>
		std::basic_istream<CharT, Traits>& read(std::basic_istream<CharT, Traits>& str)
		{
            T mean;
            str >> mean; 
            m_parameters = param_type(mean);
            return str;
		}

	    template<typename CharT, typename Traits>
		std::basic_ostream<CharT, Traits>& write(std::basic_ostream<CharT, Traits>& str) const
		{
            str << m_parameters.m_mean;
		    return str;
		}

    private:

	    param_type m_parameters;

	};

    template<typename T>
	inline bool operator ==(const zero_truncated_poisson_distribution<T>& l, const zero_truncated_poisson_distribution<T>& r)
	{
	    return (l.param() == r.param());
	}

    template<typename T>
	inline bool operator !=(const zero_truncated_poisson_distribution<T>& l, const zero_truncated_poisson_distribution<T>& r)
	{	
	    return !(l == r);
	}

    template<typename CharT, typename Traits, typename T>
	inline std::basic_ostream<CharT, Traits>& operator <<(std::basic_ostream<CharT, Traits>& str, const zero_truncated_poisson_distribution<T>& dist)
	{
	    return dist.write(str);
	}

    template<typename CharT, typename Traits, typename T>
	inline std::basic_istream<CharT, Traits>& operator >>( std::basic_istream<CharT, Traits>& str, zero_truncated_poisson_distribution<T>& dist)
	{
	    return dist.read(str);
	}

}//! namespace stk;

