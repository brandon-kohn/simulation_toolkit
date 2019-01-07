

namespace stk {

    template<typename T = double>
	class truncated_normal_distribution
	{	
    public:
	    typedef T result_type;

	    struct param_type
		{
            using distribution_type = truncated_normal_distribution<T>;

            explicit param_type(T mean, T sigma, T lower, T upper)
                : m_mean(mean)
                , m_sigma(sigma)
                , m_min(lower)
                , m_max(upper)
            {}

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

            T stddev() const { return m_sigma; }

            T m_mean;
            T m_sigma;
            T m_min;
            T m_max;
		};

    	explicit truncated_normal_distribution(T mean, T sigma, T lower, T upper) 
		    : m_parameters(mean, sigma, lower, upper), m_valid(false), m_x2(0)
		{}

	    explicit truncated_normal_distribution(const param_type& params)
	    	: m_parameters(params), m_valid(false), m_x2(0)
		{}

	    T mean() const { return m_parameters.mean(); }

	    T sigma() const { return m_parameters.sigma(); }

	    T stddev() const { return m_parameters.sigma(); }

	    param_type param() const { return m_parameters; }

	    void param(const param_type& params) { m_parameters = params; reset(); }

	    result_type (min)() const {	return numeric_limits<result_type>::denorm_min(); }

    	result_type (max)() const { return (numeric_limits<result_type>::max)(); }

	    void reset() { m_valid = false; }

    	template<typename Engine>
		result_type operator()(Engine& e) 
        {
		    return eval(e, m_parameters);
		}

	    template<typename _Engine>
		result_type operator()(_Engine& _Eng, const param_type& m_parameters0)
		{	// return next value, given parameter package
		reset();
		return (_Eval(_Eng, m_parameters0, false));
		}

	template<typename _Elem,
		typename _Traits>
		basic_istream<_Elem, _Traits>& _Read(
			basic_istream<_Elem, _Traits>& _Istr)
		{	// read state from _Istr
		T m_mean0;
		T m_sigma0;
		_In(_Istr, m_mean0);
		_In(_Istr, m_sigma0);
		m_parameters._Init(m_mean0, m_sigma0);

		_Istr >> m_valid;
		_In(_Istr, m_x2);
		return (_Istr);
		}

	template<typename _Elem,
		typename _Traits>
		basic_ostream<_Elem, _Traits>& _Write(
			basic_ostream<_Elem, _Traits>& _Ostr) const
		{	// write state to _Ostr
		_Out(_Ostr, m_parameters.m_mean);
		_Out(_Ostr, m_parameters.m_sigma);

		_Ostr << ' ' << m_valid;
		_Out(_Ostr, m_x2);
		return (_Ostr);
		}

private:
	template<typename _Engine>
		result_type _Eval(_Engine& _Eng, const param_type& m_parameters0,
			bool _Keep = true)
		{	// compute next value
			// Knuth, vol. 2, p. 122, alg. P
		T _Res;
		if (_Keep && m_valid)
			{	// return stored value
			_Res = m_x2;
			m_valid = false;
			}
		else
			{	// generate two values, store one, return one
			double _V1, _V2, _Sx;
			for (; ; )
				{	// reject bad values
				_V1 = 2 * _NRAND(_Eng, T) - 1.0;
				_V2 = 2 * _NRAND(_Eng, T) - 1.0;
				_Sx = _V1 * _V1 + _V2 * _V2;
				if (_Sx < 1.0)
					break;
				}
			double _Fx = _CSTD sqrt(-2.0 * _CSTD log(_Sx) / _Sx);
			if (_Keep)
				{	// save second value for next call
				m_x2 = _Fx * _V2;
				m_valid = true;
				}
			_Res = _Fx * _V1;
			}
		return (_Res * m_parameters0.m_sigma + m_parameters0.m_mean);
		}

	param_type m_parameters;
	bool m_valid;
	T m_x2;
	};

template<typename T>
	bool operator==(const truncated_normal_distribution<T>& m_left,
		const truncated_normal_distribution<T>& m_right)
	{	// test for equality
	return (m_left.param() == m_right.param());
	}

template<typename T>
	bool operator!=(const truncated_normal_distribution<T>& m_left,
		const truncated_normal_distribution<T>& m_right)
	{	// test for inequality
	return (!(m_left == m_right));
	}

template<typename _Elem,
	typename _Traits,
	typename T>
	basic_istream<_Elem, _Traits>& operator>>(
		basic_istream<_Elem, _Traits>& _Istr,
		truncated_normal_distribution<T>& _Dist)
	{	// read state from _Istr
	return (_Dist._Read(_Istr));
	}

template<typename _Elem,
	typename _Traits,
	typename T>
	basic_ostream<_Elem, _Traits>& operator<<(
		basic_ostream<_Elem, _Traits>& _Ostr,
		const truncated_normal_distribution<T>& _Dist)
	{	// write state to _Ostr
	return (_Dist._Write(_Ostr));
	}
}//! namespace stk;
