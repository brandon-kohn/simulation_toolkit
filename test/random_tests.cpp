
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stk/random/truncated_normal_distribution.hpp>
#include <stk/random/linear_distribution.hpp>
#include <geometrix/utility/assert.hpp>
#include <geometrix/numeric/constants.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <boost/math/distributions/normal.hpp>
#include <random>
#include <cmath>

#define STK_EXPORT_HISTS 0

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
#include <stk/sim/histogram_1d.hpp>
#include <fstream>

template <typename T>
inline void write_hist(std::ostream& os, const stk::histogram_1d<T>& hist)
{
	os << "x, y\n";
	for (std::size_t i = 0; i < hist.get_number_bins(); ++i)
	{
		os << hist.get_bin_center(i) << "," << hist.get_bin_weight(i) << "\n";
	}

	os << std::endl;
}

void write_vector(std::ostream& os, const std::vector<double>& v)
{
    auto first = reinterpret_cast<const char*>(&v[0]);
	auto last = first + v.size() * sizeof(double);
    std::copy(first, last, std::ostream_iterator<char>{os});
}

std::size_t nruns = 1000000ULL;

struct ks_test_fixture : ::testing::TestWithParam<std::pair<double, double>>{};
TEST_P(ks_test_fixture, compare_truncated_dist_against_normal_sampler)
{
	double l, h;
	std::tie(l, h) = GetParam();
	auto portion = 0.0;// 1e-3;
	auto nbins = 1000UL;
#ifdef NDEBUG
	std::size_t nruns = 1000000ULL;
#else
	std::size_t nruns = 1000000ULL;
#endif
	stk::histogram_1d<double> chist(nbins, l - portion * std::abs(l), h + portion * std::abs(h));
	stk::histogram_1d<double> nhist(nbins, l - portion * std::abs(l), h + portion * std::abs(h));
	stk::truncated_normal_distribution<> cdist(l, h);
	std::vector<double> ndata(nruns);
	std::vector<double> cdata(nruns);

	std::mt19937 gen(42UL);
	for (auto i = 0ULL; i < nruns; ++i) {
		{
			auto v = normal_trunc_reject(gen, l, h);
			GEOMETRIX_ASSERT(v >= l && v <= h);
			nhist.fill(v);
			ndata[i] = v;
		}
		{
			auto v = cdist(gen);
			GEOMETRIX_ASSERT(v >= l && v <= h);
			chist.fill(v);
			cdata[i] = v;
		}
	}

//#define STK_EXPORT_HISTS
#ifdef STK_EXPORT_HISTS
	{
		std::stringstream nname;
		nname << "e:/data_chopin" << l << "_" << h << ".csv";
		std::ofstream ofs(nname.str());
		write_hist(ofs, chist);
	}
	{
		std::stringstream nname;
		nname << "e:/data_chopin" << l << "_" << h << ".dat";
		std::ofstream ofs(nname.str(), std::ios::out | std::ios::binary);
		write_vector(ofs, cdata);
	}
	{
		std::stringstream nname;
		nname << "e:/data_control" << l << "_" << h << ".csv";
		std::ofstream ofs(nname.str());
		write_hist(ofs, nhist);
	}
	{
		std::stringstream nname;
		nname << "e:/data_control" << l << "_" << h << ".dat";
		std::ofstream ofs(nname.str(), std::ios::out | std::ios::binary);
		write_vector(ofs, ndata);
	}
#endif

	double d, p;
	std::tie(d, p) = nhist.chi_squared_test(chist);
	GTEST_MESSAGE("Test: ") << l << "_" << h << " Chi2: " << d << " P-value: " << p;
	EXPECT_GT(p, 0.05);
}

INSTANTIATE_TEST_CASE_P(validate_chopin, ks_test_fixture, ::testing::Values(
    std::make_pair(-4., 4.)
  , std::make_pair(-3., 2.)
  , std::make_pair(-3.0, -2.0)
  , std::make_pair(2.0, 3.0)
  , std::make_pair(-0.48, 0.1)
  , std::make_pair(-0.1, 0.48)
  //! Slow test below
  //, std::make_pair(3.49, 100.0)
  //, std::make_pair(-100.0, -3.49)
));


struct time_chopin_fixture : ::testing::TestWithParam<std::pair<double, double>>{};
TEST_P(time_chopin_fixture, truncated_chopin)
{
	double l, h;
	std::tie(l, h) = GetParam();
	stk::truncated_normal_distribution<> cdist(l, h);
	auto nruns = 1000000UL;

	std::vector<double> r(nruns);
	std::mt19937 gen(42UL);
	for (auto i = 0ULL; i < nruns; ++i) 
	{
		r[i] = cdist(gen);
	}

	EXPECT_TRUE(!r.empty());
}

INSTANTIATE_TEST_CASE_P(time_chopin, time_chopin_fixture, ::testing::Values(
    std::make_pair(-3., 2.)
  , std::make_pair(-4., 4.)
  , std::make_pair(-9.0, -2.0)
  , std::make_pair(2.0, 9.0)
  , std::make_pair(-0.48, 0.1)
  , std::make_pair(-0.1, 0.48)
  //! Slow test below
  , std::make_pair(3.49, 100.0)
  , std::make_pair(-100.0, -3.49)
));

TEST(truncated_normal_test_suite, DISABLED_brute_normal_distribution)
{
	stk::histogram_1d<double> hist(1000, -9.1, -1.8);
	std::normal_distribution<> dist;

	std::mt19937 gen(42UL);
	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = normal_trunc_reject(gen, -9.0, -2.0);// -0.46, 0.84);
		hist.fill(v);
	}

	hist.scale(1.0 / hist.integral());

	std::ofstream ofs("e:/data_control.csv");
	write_hist(ofs, hist);
	EXPECT_TRUE(true);
}

TEST(truncated_normal_test_suite, general_chopin_test)
{
	std::mt19937 gen(42UL);
	auto a = 1.0;
	auto b = 1.5;
	auto m = 1.4;
	auto s = 0.25;
	stk::truncated_normal_distribution<> cdist(a, b, m, s);

	for (auto i = 0ULL; i < nruns; ++i)
	{
		auto v = cdist(gen);
		EXPECT_TRUE(v >= 1.0 && v <= 1.5);
	}
}

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

/*
TEST(truncated_normal_test_suite, brute_hueristic_timing)
{
	std::mt19937 gen(42UL);
	std::uniform_real_distribution<> dist;

	auto sum = 0.0;
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
*/

#include <stk/random/xoroshiro128plus_generator.hpp>
TEST(xoroshiro128plus_generator_test_suite, construct)
{
	auto sut = stk::xoroshiro128plus_generator{};
	sut();
}

#include <stk/random/xorshift1024starphi_generator.hpp>
TEST(xorshift1024starphi_test_suite, construct)
{
	auto sut = stk::xorshift1024starphi_generator{};
	sut();
	auto l = -4.0;
	auto h = 4.0;
	stk::histogram_1d<double> chist(1000, l, h);
	stk::histogram_1d<double> nhist(1000, l, h);
	stk::truncated_normal_distribution<> cdist(l, h);

	std::size_t nruns = 10000000ULL;
	auto gen = stk::xoroshiro128plus_generator{};
	for (auto i = 0ULL; i < nruns; ++i) 
	{
		auto v = cdist(gen);
		GEOMETRIX_ASSERT(v >= l && v <= h);
		chist.fill(v);

		v = normal_trunc_reject(gen, l, h);
		GEOMETRIX_ASSERT(v >= l && v <= h);
		nhist.fill(v);
	}

#if(STK_EXPORT_HISTS)
	std::stringstream nname;
	nname << "e:/data_xoshiro_chopin" << l << "_" << h << ".csv";
	std::ofstream ofs(nname.str());
	write_hist(ofs, chist);
#endif
	double d, p;
	std::tie(d, p) = nhist.chi_squared_test(chist);
	GTEST_MESSAGE("Test: ") << l << "_" << h << " Chi2: " << d << " P-value: " << p;
	EXPECT_GT(p, 0.05);
}

TEST(linear_distribution_test_suite, verify_range)
{
	auto l = 5.0, h = 10.0;
	auto sut = stk::linear_distribution<>{ l, h, 0.0, 33.0 };

	std::mt19937 gen(42UL);
	
#if(STK_EXPORT_HISTS)
	stk::histogram_1d<double> chist(1000, l, h);
#endif

	for (auto i = 0UL; i < 1000000; ++i)
	{
		auto v = sut(gen);
		EXPECT_GT(v, 5.0);
		EXPECT_LT(v, 10.0);
#if(STK_EXPORT_HISTS)
		chist.fill(v);
#endif
	}
	
#if(STK_EXPORT_HISTS)
	std::stringstream nname;
	nname << "d:/linear_dist.csv";
	std::ofstream ofs(nname.str());
	write_hist(ofs, chist);
#endif
}

TEST(linear_distribution_test_suite, verify_range_neg_slope)
{
	auto l = 5.0, h = 10.0;
	auto sut = stk::linear_distribution<>{ l, h, 0.0, 33.0 };

	std::mt19937 gen(42UL);
	
#if(STK_EXPORT_HISTS)
	stk::histogram_1d<double> chist(1000, l, h);
#endif

	for (auto i = 0UL; i < 1000000; ++i)
	{
		auto v = sut(gen);
		EXPECT_GT(v, 5.0);
		EXPECT_LT(v, 10.0);
#if(STK_EXPORT_HISTS)
		chist.fill(v);
#endif
	}
	
#if(STK_EXPORT_HISTS)
	std::stringstream nname;
	nname << "d:/linear_dist2.csv";
	std::ofstream ofs(nname.str());
	write_hist(ofs, chist);
#endif
}

#include <stk/random/maxwell_boltzmann_distribution.hpp>
TEST(maxwell_boltzmann_distribution_test_suite, basic)
{
	auto l = 0.0, h = 20.0;
    auto sut = stk::maxwell_boltzmann_distribution<> { 2.0 };

	std::mt19937 gen(42UL);
	
#if(STK_EXPORT_HISTS)
	stk::histogram_1d<double> chist(1000, l, h);
#endif

	for (auto i = 0UL; i < 1000000; ++i)
	{
		auto v = sut(gen);
#if(STK_EXPORT_HISTS)
        if (v >= l && v <= h)
            chist.fill(v);
#endif
	}
	
#if(STK_EXPORT_HISTS)
	std::stringstream nname;
	nname << "d:/mb_dist2.0.csv";
	std::ofstream ofs(nname.str());
	write_hist(ofs, chist);
#endif
}


#define MODLUS 2147483647
#define MULT1 24112
#define MULT2 26143


double URand( int* iy )
{
	long zi, lowprd, hi31;

	zi = *iy;
	lowprd = ( zi & 65535 ) * MULT1;
	hi31 = ( zi >> 16 ) * MULT1 + ( lowprd >> 16 );
	zi = ( ( lowprd & 65535 ) - MODLUS ) + ( ( hi31 & 32767 ) << 16 ) + ( hi31 >> 15 );
	if( zi < 0 )
		zi += MODLUS;
	lowprd = ( zi & 65535 ) * MULT2;
	hi31 = ( zi >> 16 ) * MULT2 + ( lowprd >> 16 );
	zi = ( ( lowprd & 65535 ) - MODLUS ) + ( ( hi31 & 32767 ) << 16 ) + ( hi31 >> 15 );
	if( zi < 0 )
		zi += MODLUS;
	*iy = zi;
	return ( ( ( zi >> 7 | 1 ) + 1 ) / 16777216.0 );
}

#include <random>
#include <chrono>

TEST(random_timing_suite, time)
{
	std::mt19937 gen32;
	std::mt19937_64 gen64;
	stk::xoroshiro128plus_generator genBrandon;
	int seed = 13;
	gen32.seed(seed);
	gen64.seed(seed);
	genBrandon.seed(seed);

	std::uniform_real_distribution<double> uniform_dist(0, 1);
	boost::random::uniform_real_distribution<> uniform_dist_boost;

	int N = 1000000;
	std::vector<std::size_t> results( N, 0 );

	//Test 32
	auto startg32 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < N; i++) {
		//std::cout << uniform_dist(gen32) << std::endl;
		results[i] = uniform_dist(gen32);
	}
	auto stopg32 = std::chrono::high_resolution_clock::now();
	auto duration32 = std::chrono::duration_cast<std::chrono::microseconds>(stopg32 - startg32);
	std::cout << "GEN32 " << duration32.count() << " ms" << std::endl;

	//Test URand
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < N; i++) {
		//std::cout << URand(&seed) << std::endl;
		results[i] = URand(&seed);
	}
	auto stop = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
	std::cout << "URAND " << duration.count()<< " ms" << std::endl;

	//Test 64
	start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < N; i++) {
		//std::cout << uniform_dist(gen64) << std::endl;
		results[i] = uniform_dist(gen64);
	}
	stop = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
	std::cout << "GEN64 " << duration.count() << " ms" << std::endl;

	//Test Brandon
	start = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < N; i++) {
		//std::cout << uniform_dist(genBrandon) << std::endl;
		results[i] = uniform_dist_boost(genBrandon);
	}
	stop = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
	std::cout << "GenBrandon " << duration.count() << " ms" << std::endl;
	
	std::cout << "Hello Done." << std::endl;
}

