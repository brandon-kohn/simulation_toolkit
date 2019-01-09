
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <random>

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
		auto two_x = constants::two<Real>() * x;
		if (v*v*x <= a)
			return sqrt(two_x);
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

auto nruns = 1000000ULL;

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
