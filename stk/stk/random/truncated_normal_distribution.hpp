#pragma once

#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <boost/math/distributions/normal.hpp>
#include <stk/geometry/tolerance_policy.hpp>
#include <random>
#include <cmath>

//#define STK_DEBUG_TRUNCATED_DIST

//! From Simulation from the Normal Distribution Truncated to an Interval in the Tail, Botev, Zdravko and L'Ecuyer, Pierre.
//! And Fast simulation of truncated Gaussian distributions, Nicolas Chopin
namespace stk {

    namespace detail {

		template <typename T>
        inline T erf(T z0, T z1)
        {
            return boost::math::erf(z1) - boost::math::erf(z0);
        }

		//! Get the cdf in the range [z0, z1].
		template <typename T>
        inline T normal_cdf(T z0, T z1)
        {
            const auto invsqrt2 = 0.70710678118;
            return 0.5 * (boost::math::erf(z1 * invsqrt2) - boost::math::erf(z0 * invsqrt2));
        }

        //! integral of normal dist from [-inf, x].
		template <typename T>
        inline T normal_cdf(T x, T m, T s)
        {
            const auto invsqrt2 = 0.70710678118;
            return 0.5 * (1.0 + boost::math::erf(invsqrt2*(x - m) / s));
        }

		template <typename T>
        inline T normal_cdf(T z)
        {
            const auto invsqrt2 = 0.70710678118;
            return 0.5 * (1.0 + boost::math::erf(invsqrt2*z));
        }

		template <typename T>
        inline T normal_pdf(T z)
        {
			using std::exp;
            auto invsqrt2pi = 0.3989422804;
            return invsqrt2pi * exp(-0.5 * z * z);
        }

		template <typename T>
		inline T normal_quantile(T proportion)
		{
			boost::math::normal_distribution<T> dist;
			return boost::math::quantile(dist, proportion);
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

        template <typename Generator, typename T>
        inline T normal_trunc_reject(Generator&& gen, T a, T b)
        {
            std::normal_distribution<T> N;
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
        
		template <typename Gen, typename UniformDist, typename Real>
        inline bool devroye_normal_trunc_single_step(Gen&& gen, UniformDist&& U, Real a, Real b, Real& r)
        {
            GEOMETRIX_ASSERT(a < b);
            using namespace geometrix;
            using std::sqrt;
            using std::log;
            using std::exp;

            auto K = a * a * constants::two<Real>();
            auto q = constants::one<Real>() - exp(-(b - a)*a);
            auto u = U(gen);
            auto v = U(gen);
            auto x = -log(constants::one<Real>() - q * u);
            auto e = -log(v);
			if (x*x <= K * e) 
			{
				r = a + x / a;
				return true;
			}
			return false;
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

        template <typename T, unsigned int N>
        struct truncated_normal_helper
        {
            static const unsigned int num_splits = 2 * N + 1;

            truncated_normal_helper()
            {
                generate_tables();
            }

            //! This assumes a and b are limits in a standard normal distribution (i.e. mean = 0 and sigma = 1).
            template <typename Generator>
            T operator()(Generator&& gen, T a, T b) const
            {
				using std::abs;
                GEOMETRIX_ASSERT(a < b);

                //! In the general case, run the Chopin algorithm.
                if(a >= xmin && a <= xmax)
                {
                    auto ia = j[get_i(a)];
                    auto ib = b < xmax ? j[get_i(b)] : x.size() - 1;

                    if(abs((int)(ib - ia)) < 5)
                        return devroye_normal_trunc(std::forward<Generator>(gen), a, b);

                    GEOMETRIX_ASSERT(ia < ib);
                    std::uniform_int_distribution<std::uint32_t> idist(ia, ib);
                    std::uniform_real_distribution<> U;
#ifdef STK_DEBUG_TRUNCATED_DIST
					struct returns
					{
						std::size_t calls = 0;
						std::size_t rightmost = 0;
						std::size_t boundary = 0;
						std::size_t general_0 = 0;
						std::size_t general_1 = 0;
					};
					static returns sReturns{};
					auto pReturns = &sReturns;
					static stk::histogram_1d<T> Rhist(1000, a, b);
					static stk::histogram_1d<T> Bhist(1000, a, b);
					static stk::histogram_1d<T> G0hist(1000, a, b);
					static stk::histogram_1d<T> G1hist(1000, a, b);
					if (sReturns.calls++ % 10000 == 0) 
					{
						auto write_hist = [](std::string name, const stk::histogram_1d<T>& hist)
						{
							std::ofstream os(name);
							os << "x, y\n";
							for (std::size_t i = 0; i < hist.get_number_bins(); ++i) {
								os << hist.get_bin_center(i + 1) << "," << hist.get_bin_content(i + 1) << "\n";
							}

							os << std::endl;
						};
						write_hist("e:/RHist.csv", Rhist);
						write_hist("e:/BHist.csv", Bhist);
						write_hist("e:/G0Hist.csv", G0hist);
						write_hist("e:/G1Hist.csv", G1hist);
						std::ofstream ofs("e:/truncdata.txt");
						ofs << "R: " << sReturns.rightmost << std::endl;
						ofs << "B: " << sReturns.boundary << std::endl;
						ofs << "G0: " << sReturns.general_0 << std::endl;
						ofs << "G1: " << sReturns.general_1 << std::endl;
					}

#endif

                    while (true)
                    {
                        auto i = idist(gen);
                        if (i+1 == x.size())//! rightmost region
                        {
                            GEOMETRIX_ASSERT(x.back() < b);
							T r;
							if (devroye_normal_trunc_single_step(std::forward<Generator>(gen), U, x.back(), b, r))
								return r;
							else
								continue;
                        }

						//! If it's on the two boundary need to check if result is within bounds.
                        if (i <= ia + 1 || (i >= ib - 1 && b < xmax))//! two extreme/boundary regions
                        {
							GEOMETRIX_ASSERT(i+1 != x.size());
                            auto u = U(gen);
                            auto xi = x[i] + dx(i) * u;
                            if (xi >= a && xi <= b) 
                            {
                                auto v = U(gen);
                                auto yi = y[i] * v;
								if (yi < y_(i) || yi <= normal_pdf(xi)) 
								{
#ifdef STK_DEBUG_TRUNCATED_DIST
									++sReturns.boundary;
									Bhist.fill(xi);
#endif
									return xi;
								}
                            }
                        }
                        else
                        {
                            auto u = U(gen);
                            auto yi = u * y[i];
							if (yi <= y_(i))//! occurs with high probability
							{
#ifdef STK_DEBUG_TRUNCATED_DIST
								++sReturns.general_0;
								G0hist.fill(x[i] + u * d(i));
#endif
								return x[i] + u * d(i);
							}
                            else 
                            {
                                auto v = U(gen);
                                auto xi = x[i] + dx(i) * v;
								if (yi < normal_pdf(xi)) 
								{
#ifdef STK_DEBUG_TRUNCATED_DIST
									++sReturns.general_1;
									G1hist.fill(xi);
#endif
									return xi;
								}
                            }
                        }
                    }
                }
               
				//! If abs(a) > abs(b), then try reversing to run in the larger range provided by xmax.
				if (abs(a) > abs(b))
					return -(*this)(std::forward<Generator>(gen), -b, -a);

                //! Use the direct normal with rejection out of the bounds for a < xmin.
                if (a < xmin)
                    return normal_trunc_reject(std::forward<Generator>(gen), a, b);
               
                //! For a > xmax use the Devroye with exponential proposal and truncation.
                GEOMETRIX_ASSERT(a > xmax);
                return devroye_normal_trunc(std::forward<Generator>(gen), a, b);
            }

#ifndef STK_DEBUG_TRUNCATED_DIST
        private:
#endif
            
            void generate_tables()
            {
				T ltailClip = -2.0;
				x.resize(num_splits);
				y.resize(num_splits - 1);
				
				//! Find the extreme value and calculate from the max value to the turning point (dy/dx == 0 bin). Then mirror around 0 for the other side.
				//! Split into equal areas.
				auto ai = 1.0 / (2.0 * N + 2);
				auto ar = ai;
				auto al = ai;

				boost::math::normal_distribution<T> dist;
				xmin = boost::math::quantile(dist, ar);
				xmax = -xmin;
	            auto q = al;
				auto xi = xmin;
				x[0] = xi;
				x[x.size() - 1] = xmax;
				for (auto i = 1UL; i <= N; ++i)
				{
					q = al + i * ai;
					xi = boost::math::quantile(dist, q);
					x[i] = xi;
					x[x.size() - 1 - i] = -xi;

					auto yi = normal_pdf(xi);
					auto yi_ = normal_pdf(x[i - 1]);
					y[i-1] = std::max(yi, yi_);
					y[y.size() - i] = y[i - 1];//! NOTE: y.size() - 1 - (i + 1) == y.size() - i.
					if (i == 1) 
					{
						yl_ = std::min(yi, yi_);
						yr_ = yl_;
					}
				}
				
				GEOMETRIX_ASSERT(make_tolerance_policy().equals(q, 0.5));
				GEOMETRIX_ASSERT(make_tolerance_policy().equals(xmax, x.back()));

				//! Clip to bounds.
                auto h = std::numeric_limits<T>::infinity();
				auto clip = 0UL;
				for (auto i = 1UL; i <= N; ++i)
				{
					if (x[i + 1] > ltailClip)
					{
						xmin = x[i];
						clip = i;
						auto yi = normal_pdf(x[i]);
						auto yi_ = normal_pdf(x[i+1]);
						yl_ = std::min(yi, yi_);
						x.erase(x.begin(), x.begin() + i);
						y.erase(y.begin(), y.begin() + i);
						break;
					}
				}

				auto nOffset = N - clip;
				for (auto i = 1UL; i < x.size(); ++i)
				{
					auto hi = x[i] - x[i - 1];
					if (hi < h)
						h = hi;
				}

                inv_h = 1.0 / h;
                auto nk = std::ceil((xmax - xmin) * inv_h);
                auto find_max = [&](std::size_t is, T kh)
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
				auto k = 0UL;
                for(auto kh = xmin;kh <= xmax;++k,kh = xmin + k*h)
                {
                    j.emplace_back(find_max(last, kh));
                    last = j.back();
                }

                for (k = 0UL; k < j.size(); ++k) 			
                {
                    if (j[k] == nOffset)
                    {
                        ioffset = k;
                        break;
                    }
                }

                GEOMETRIX_ASSERT(stk::make_tolerance_policy().equals(0.0, x[j[ioffset]]));
            }

            std::uint32_t get_i(T x) const
            {
                auto i = ioffset + std::floor(x * inv_h);
                GEOMETRIX_ASSERT(i < j.size());
                return i;
            }

            T y_(std::uint32_t i) const
            {
                if (i > 0 && (i + 1 < y.size())) 
                {
					auto i0 = j[ioffset];
                    return i < i0 ? y[i - 1] : y[i + 1];
                }

				return i ? yr_ : yl_;
            }

            T dx(std::uint32_t i) const
            {
                GEOMETRIX_ASSERT(i < x.size() - 1);
                return x[i + 1] - x[i];
            }
            
            T d(std::uint32_t i) const
            {
                return dx(i) * y[i] / y_(i);
            }

            T xmin, xmax;
            T inv_h;
            T yr_;
            T yl_;
            std::uint16_t ioffset;
            std::vector<std::uint16_t> j;
            std::vector<T> x;
            std::vector<T> y;
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

		//! Convenience method to generate the singleton helper in a single thread context. This makes subsequent calls thread safe.
		void make_thread_safe() const
		{
			get_helper();
		}

#ifndef STK_DEBUG_TRUNCATED_DIST
    private:
#endif

		static const std::uint16_t num_splits = 2050;
	    static detail::truncated_normal_helper<T, num_splits>& get_helper()
        {
			using namespace detail;
            static auto instance = truncated_normal_helper<T, num_splits>();
            return instance;
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

