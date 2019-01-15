
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stk/random/truncated_normal_distribution.hpp>
#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <boost/math/distributions/normal.hpp>
#include <random>
#include <cmath>

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

inline double box_muller_transform_x(double u0, double u1)
{
	auto z0 = std::sqrt(-2.0 * std::log(u0)) * std::cos(geometrix::constants::two_pi<double>() * u1);
	return z0;
}

inline double polar_box_muller_transform_x(double u0, double u1)
{
	auto z0 = std::sqrt(-2.0 * std::log(u0)) * std::cos(geometrix::constants::two_pi<double>() * u1);
	return z0;
}

inline double inv_box_muller_transform_x(double z0, double z1)
{
	auto u0 = std::exp(-0.5 * (z0 * z0 + z1 * z1));
	return u0;
}

inline double inv_box_muller_transform_y(double z0, double z1)
{
	auto u1 = geometrix::constants::half_pi<double>() * std::atan2(z1, z0);
	return u1;
}

inline std::pair<double, double> box_muller_transform(double u0, double u1)
{
	auto z0 = std::sqrt(-2.0 * std::log(u0)) * std::cos(geometrix::constants::two_pi<double>() * u1);
	auto z1 = std::sqrt(-2.0 * std::log(u0)) * std::sin(geometrix::constants::two_pi<double>() * u1);
	return { z0, z1 };
}

template <typename Gen, typename Real>
inline Real box_muller_normal_trunc(Gen&& gen, Real a, Real b)
{
	GEOMETRIX_ASSERT(a < b);
	auto ua = inv_box_muller_transform_x(a, 0.0);
	auto ub = inv_box_muller_transform_x(b, 0.0);
	auto umin = (std::min)(ua, ub);
	std::uniform_real_distribution<> U(umin, 1.0);
	auto aNeg = std::signbit(a);
	auto bNeg = std::signbit(b);
	if (aNeg && bNeg) 
	{
		std::uniform_real_distribution<> V(0.25, 0.5);
		while (true) 
		{
			auto u = U(gen);
			auto v = V(gen);
			auto r = box_muller_transform_x(u, v);
			if (r >= a && r <= b)
				return r;
		}
	} 
	else if (!aNeg && !bNeg) 
	{
		std::uniform_real_distribution<> V(0.0, 0.25);
		while (true) 
		{
			auto u = U(gen);
			auto v = V(gen);
			auto r = box_muller_transform_x(u, v);
			if (r >= a && r <= b)
				return r;
		}
	}
	else
	{
		std::uniform_real_distribution<> V(0.0, 0.5);
		while (true) 
		{
			auto u = U(gen);
			auto v = V(gen);
			auto r = box_muller_transform_x(u, v);
			if (r >= a && r <= b)
				return r;
		}
	}
}

//! From Simulation from the Normal Distribution Truncated to an Interval in the Tail, Botev, Zdravko and L'Ecuyer, Pierre.
//! And Fast simulation of truncated Gaussian distributions, Nicolas Chopin
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

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/sim/histogram_1d.ipp>
#include <fstream>

template <typename T>
inline void write_hist(std::ostream& os, const stk::histogram_1d<T>& hist)
{
	os << "x, y\n";
	for (std::size_t i = 0; i < hist.get_number_bins(); ++i)
	{
		os << hist.get_bin_center(i+1) << "," << hist.get_bin_content(i+1) << "\n";
	}

	os << std::endl;
}

auto nruns = 1000000ULL;
TEST(truncated_normal_test_suite, chopin_test)
{
	stk::histogram_1d<double> hist(100, -3.1, 3.1);
	stk::truncated_normal_distribution<> dist(-4.0, -3.0);

	std::mt19937 gen(42UL);
	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = dist(gen);
		hist.fill(v);
	}

	hist.scale(1.0 / hist.integral());

	std::ofstream ofs("e:/data_chopin.csv");
	write_hist(ofs, hist);
	EXPECT_TRUE(true);
}
/*
TEST(truncated_normal_test_suite, measure_acceptance_simple_rejection)
{
	auto proposals = 0ULL;
	auto rejected = 0ULL;
	std::mt19937 gen{ 42U };
	auto sampler = [&](double a, double b)
	{
		std::normal_distribution<> N;
		while (true) 
		{
			auto r = N(gen);
			++proposals;
			if (r >= a && r <= b) 
				return r;
			++rejected;
		}
	};

	for (auto i = 0ULL; i < nruns; ++i)
		sampler(2., 3.);

	auto acceptance = 1.0 - (static_cast<double>(rejected) / proposals);

}
TEST(truncated_normal_test_suite, measure_acceptance)
{
	auto proposals = 0ULL;
	auto rejected = 0ULL;
	std::mt19937 gen{ 42U };
	auto sampler = [&](double a, double b)
	{
		auto ua = inv_box_muller_transform_x(a, 0.0);
		auto ub = inv_box_muller_transform_x(b, 0.0);
		std::uniform_real_distribution<> V(0.0, 0.5);
		auto umin = (std::min)(ua, ub);
		std::uniform_real_distribution<> U(umin, 1.0);
		while (true) 
		{
			auto u = U(gen);
			auto v = V(gen);
			auto r = box_muller_transform_x(u, v);
			++proposals;
			if (r >= a && r <= b) 
				return r;
			++rejected;
		}
	};

	for (auto i = 0ULL; i < nruns; ++i)
		sampler(2., 3.);

	auto acceptance = 1.0 - (static_cast<double>(rejected) / proposals);

}
*/

TEST(truncated_normal_test_suite, brute_normal_distribution)
{
	stk::histogram_1d<double> hist(100, -3.1, 3.1);
	std::normal_distribution<> dist;

	std::mt19937 gen(42UL);
	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = normal_trunc_reject(gen, -4.0, -3.0);// -0.46, 0.84);
		hist.fill(v);
	}

	hist.scale(1.0 / hist.integral());

	std::ofstream ofs("e:/data_control.csv");
	write_hist(ofs, hist);
	EXPECT_TRUE(true);
}

/*
TEST(truncated_normal_test_suite, brute_hueristic_box_muller)
{
	stk::histogram_1d<double> hist(100, -3.1, 3.1);
	std::uniform_real_distribution<> dist;

	std::mt19937 gen(42UL);
	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = box_muller_normal_trunc(gen, -3.0, -2.9);// -0.46, 0.84);
		hist.fill(v);
	}

	hist.scale(1.0 / hist.integral());

	std::ofstream ofs("e:/data.csv");
	write_hist(ofs, hist);
	EXPECT_TRUE(true);
}

TEST(truncated_normal_test_suite, brute_hueristic_box_muller_full)
{
	stk::histogram_1d<double> hist(100, -3.0, 3.0);
	std::uniform_real_distribution<> dist;

	std::mt19937 gen(42UL);
	for (auto i = 0ULL; i < nruns; ++i) {
		auto v = box_muller_normal_trunc(gen, -3., 3.);
		hist.fill(v);
	}

	hist.scale(1.0 / hist.integral());

	std::ofstream ofs("e:/data_full.csv");
	write_hist(ofs, hist);
	EXPECT_TRUE(true);
}
*/

TEST(truncated_normal_test_suite, brute_hueristic_uniform)
{
	std::mt19937 gen(42UL);
	std::uniform_real_distribution<> dist;

	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = uniform_normal_trunc(gen, dist, 7.0, 8.0);
		EXPECT_TRUE(v >= 7.0 && v <= 8.0);
	}
}

TEST(truncated_normal_test_suite, brute_hueristic_rayleigh_trunc)
{
	std::mt19937 gen(42UL);
	std::uniform_real_distribution<> dist;

	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = rayleigh_normal_trunc(gen, dist, 7.0, 8.0);
		EXPECT_TRUE(v >= 7.0 && v <= 8.0);
	}
}

TEST(truncated_normal_test_suite, brute_hueristic_rayleigh_reject)
{
	std::mt19937 gen(42UL);
	std::uniform_real_distribution<> dist;

	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = rayleigh_normal_reject(gen, dist, 7.0, 8.0);
		EXPECT_TRUE(v >= 7.0 && v <= 8.0);
	}
}

TEST(truncated_normal_test_suite, brute_hueristic_timing)
{
	std::mt19937 gen(42UL);
	std::uniform_real_distribution<> dist;

	auto sum = 0;
	{
		GEOMETRIX_MEASURE_SCOPE_TIME("uniform_normal_trunc");
		for (auto i = 0ULL; i < nruns; ++i)
		{
			auto v = uniform_normal_trunc(gen, dist, 7.0, 8.0);
			sum += v;
		}
	}

	EXPECT_TRUE(sum > 0.0);
}
