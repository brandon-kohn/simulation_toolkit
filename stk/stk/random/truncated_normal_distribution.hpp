#pragma once

#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <boost/math/distributions/normal.hpp>
#include <stk/geometry/tolerance_policy.hpp>
#include <random>
#include <cmath>

//! From Simulation from the Normal Distribution Truncated to an Interval in the Tail, Botev, Zdravko and L'Ecuyer, Pierre.
//! And Fast simulation of truncated Gaussian distributions, Nicolas Chopin
namespace stk {

    namespace detail {

        inline double erf(double z0, double z1)
        {
            return std::erf(z1) - std::erf(z0);
        }

        inline double phi(double z0, double z1)
        {
            const auto invsqrt2 = 0.70710678118;
            return 0.5 * (std::erf(z1 * invsqrt2) - std::erf(z0 * invsqrt2));
        }

        //! integral of normal dist from [-inf, x].
        inline double normal_cdf(double x, double m, double s)
        {
            const auto invsqrt2 = 0.70710678118;
            return 0.5 * (1.0 + std::erf(invsqrt2*(x - m) / s));
        }

        inline double normal_cdf(double z)
        {
            const auto invsqrt2 = 0.70710678118;
            return 0.5 * (1.0 + std::erf(invsqrt2*z));
        }

        inline double normal_pdf(double z)
        {
            double invsqrt2pi = 0.3989422804;
            return invsqrt2pi * std::exp(-0.5 * z * z);
        }

        template <typename Real>
        inline bool is_standard_normal(Real m, Real s)
        {
            using namespace geometrix;
            return m == constants::zero<Real>() && s == constants::one<Real>();
        }

        template <typename Real>
        inline Real scale_to_standard_normal(Real x, Real m, Real s)
        {
            return (x - m) / s;
        }

        template <typename Real>
        inline Real scale_to_general_normal(Real x, Real m, Real s)
        {
            return x * s + m;
        }

        template <typename Generator>
        double normal_trunc_reject(Generator&& gen, double a, double b)
        {
            std::normal_distribution<> N;
            while (true) 
            {
                auto r = N(gen);
                if (r >= a && r <= b)
                    return r;
            }
        }

        template <typename Gen, typename Real>
        inline Real devroye_normal_trunc(Gen&& gen, Real a, Real b)
        {
            GEOMETRIX_ASSERT(a < b);
            using namespace geometrix;
            using std::sqrt;
            using std::log;
            using std::exp;

            std::uniform_real_distribution<Real> U;

            auto K = a * a * constants::two<Real>();
            auto q = constants::one<Real>() - exp(-(b - a)*a);
            while(true)
            {
                auto u = U(gen);
                auto v = U(gen);
                auto x = -log(constants::one<Real>() - q * u);
                auto e = -log(v);
                if (x*x <= K * e)
                    return a + x / a;
            }
        }

        template <typename Gen, typename UniformRealDist, typename Real>
        inline Real rayleigh_normal_trunc(Gen&& gen, UniformRealDist&& U, Real a, Real b)
        {
            GEOMETRIX_ASSERT(a < b);
            using namespace geometrix;
            using std::sqrt;
            using std::log;
            using std::exp;

            auto c = a * a * constants::one_half<Real>();
            auto b2 = b * b;
            auto q = constants::one<Real>() - exp(c - b2 * constants::one_half<Real>());
            while(true)
            {
                auto u = U(gen);
                auto v = U(gen);
                auto x = c - log(constants::one<Real>() - q * u);
                if (v*v*x <= a) 
                {
                    auto two_x = constants::two<Real>() * x;
                    return sqrt(two_x);
                }
            }
        }

        template <typename Gen, typename UniformRealDist, typename Real>
        inline Real rayleigh_normal_reject(Gen&& gen, UniformRealDist&& U, Real a, Real b)
        {
            GEOMETRIX_ASSERT(a < b);
            using namespace geometrix;
            using std::sqrt;
            using std::log;

            auto c = a * a * constants::one_half<Real>();
            auto b2 = b * b;
            while(true)
            {
                auto u = U(gen);
                auto v = U(gen);
                auto x = c - log(u);
                auto two_x = constants::two<Real>() * x;
                if (v*v*x <= a && two_x <= b2)
                    return sqrt(two_x);
            }
        }

        template <typename Gen, typename UniformRealDist, typename Real>
        inline Real uniform_normal_trunc(Gen&& gen, UniformRealDist&& U, Real a, Real b)
        {
            GEOMETRIX_ASSERT(a < b);
            using namespace geometrix;
            using std::log;

            auto a2 = a * a;
            while(true)
            {
                auto u = U(gen);
                auto v = U(gen);
                auto x = a + (b - a)*u;
                if (constants::two<Real>() * log(v) <= a2 - x * x)
                    return x;
            }
        }

        template <unsigned int N>
        struct truncated_normal_helper
        {
            static const unsigned int n_regions = 2 * N + 2;

            truncated_normal_helper()
            {
                generate_tables();
            }

            //! This assumes a and b are limits in a standard normal distribution (i.e. mean = 0 and sigma = 1).
            template <typename Generator>
            double operator()(Generator&& gen, double a, double b) const
            {
                GEOMETRIX_ASSERT(a < b);

                if (a < xmin)
                    return normal_trunc_reject(std::forward<Generator>(gen), a, b);

                if (a > xmax)
                    return devroye_normal_trunc(std::forward<Generator>(gen), a, b);

                auto ia = j[get_i(a)];
                auto ib = b < xmax ? j[get_i(b)] : x.size() - 1;

                if(std::abs((int)(ib - ia)) < 5)
                    return devroye_normal_trunc(std::forward<Generator>(gen), a, b);

                GEOMETRIX_ASSERT(ia < ib);
                std::uniform_int_distribution<std::uint32_t> idist(ia, ib);
                std::uniform_real_distribution<> U;
                while (true)
                {
                    auto i = idist(gen);
                    if (i+1 == x.size())//! rightmost region
                    {
                        GEOMETRIX_ASSERT(x.back() < b);
                        return devroye_normal_trunc(std::forward<Generator>(gen), x.back(), b);
                    }

                    if (i <= (ia + 1) || (i >= (ib - 1) && (ib+1) != x.size()))//! two extreme/boundary regions
                    {
                        auto u = U(gen);
                        auto xi = x[i] + dx(i) * u;
                        if (xi >= a && xi <= b) 
                        {
                            auto v = U(gen);
                            auto yi = y[i] * v;
                            if (yi < y_(i) || yi <= normal_pdf(xi))
                                return xi;
                        }
                    }
                    else
                    {
                        auto u = U(gen);
                        auto yi = u * y[i];
                        if (yi <= y_(i))//! occurs with high probability
                            return x[i] + u * d(i);
                        else 
                        {
                            auto v = U(gen);
                            auto xi = x[i] + dx(i) * v;
                            if (yi < normal_pdf(xi))
                                return xi;
                        }
                    }
                }
            }

        private:
            
            void generate_tables()
            {
                boost::math::normal_distribution<double> dist;
                
                auto h = std::numeric_limits<double>::infinity();
                auto ai = 1.0 / n_regions;

                auto q = ai;
                auto xi = boost::math::quantile(dist, ai);
                xmin = xi;
                x[0] = xi;
                auto ylast = normal_pdf(xi);
                for (auto i = 1UL; i < n_regions; ++i) 
                {
                    q += ai;
                    xi = boost::math::quantile(dist, q);

                    x[i] = xi;
                    xmax = xi;

                    auto yi = normal_pdf(xi);
                    auto yi_ = normal_pdf(x[i - 1]);
                    y[i - 1] = std::max(yi, yi_);
                    if (i == 1)
                        yl_ = std::min(yi, yi_);
                    if (i + 1 == x.size())
                        yr_ = std::min(yi, yi_);

                    auto hi = xi - x[i - 1];
                    if (hi < h)
                        h = hi;
                }

                auto nk = std::ceil((xmax - xmin) / h);
                auto find_max = [&](std::size_t is, double kh)
                {
                    auto maxi = 0;
                    for (auto i = is; i < x.size(); ++i)
                    {
                        if (x[i] <= kh)
                            maxi = i;
                        else
                            break;
                    }

                    return maxi;
                };

                auto last = 0UL;
                for(auto k = 0.0, kh = xmin;kh <= xmax;k+=1.0,kh += h)
                {
                    j.emplace_back(find_max(last, kh));
                    last = j.back();
                }

                for (auto k = 0UL; k < j.size(); ++k) 			
                {
                    if (j[k] == N)
                    {
                        ioffset = k;
                        break;
                    }
                }

                GEOMETRIX_ASSERT(stk::make_tolerance_policy().equals(0.0, x[j[ioffset]]));

                inv_h = 1.0 / h;
            }

            std::uint32_t get_i(double x) const
            {
                return ioffset + std::floor(x * inv_h);
            }

            double y_(std::uint32_t i) const
            {
                if (i > 0 && (i + 1 < x.size())) 
                {
                    auto i0 = get_i(0.0);
                    if (i < i0)
                        return y[i - 1];
                    else
                        return y[i + 1];
                }

                return i ? yr_ : yl_;
            }

            double dx(std::uint32_t i) const
            {
                GEOMETRIX_ASSERT(i);
                return x[i] - x[i - 1];
            }
            
            double d(std::uint32_t i) const
            {
                GEOMETRIX_ASSERT(i);
                return dx(i - 1) * y[i - 1] / y_(i - 1);
            }

            double xmin, xmax;
            double inv_h;
            double yl_, yr_;//! The lower y value for the extremal rectangles.
            std::uint32_t ioffset;
            std::vector<std::uint16_t> j;
            std::array<double, n_regions> x;
            std::array<double, n_regions - 1> y;
        };
    }//! namespace detail;

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
            T lower() const { return m_min; }
            T upper() const { return m_max; }
            
            bool is_standard_normal() const { return detail::is_standard_normal(m_mean, m_sigma); }
            T scale_to_general(T x) const { return detail::scale_to_general_normal(x, m_mean, m_sigma); }
            T standard_normal_lower() const { return detail::scale_to_standard_normal(m_min, m_mean, m_sigma); }
            T standard_normal_upper() const { return detail::scale_to_standard_normal(m_max, m_mean, m_sigma); }

        private:

            T m_mean;
            T m_sigma;
            T m_min;
            T m_max;

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
            if(!m_parameters.is_standard_normal())
                return m_parameters.scale_to_general(get_helper()(e, m_parameters.standard_normal_lower(), m_parameters.standard_normal_upper()));
		    return get_helper()(e, m_parameters.lower(), m_parameters.upper());
		}

	    template<typename Engine>
		result_type operator()(Engine& e, const param_type& params)
		{
            if(!m_parameters.is_standard_normal())
                return params.scale_to_general(get_helper()(e, params.standard_normal_lower(), params.standard_normal_upper()));
		    return get_helper()(e, params.lower(), params.upper());
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
            m_parameters.init(mean, sigma, a, b);
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

        static detail::truncated_normal_helper<2000> const& get_helper()
        {
			using namespace detail;
            static auto instance = truncated_normal_helper<2000>();
            return instance;
        }

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

