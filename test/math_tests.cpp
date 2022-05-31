#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <geometrix/numeric/constants.hpp>
#include <stk/utility/floating_point_traits.hpp>
#include <stk/math/math_kernel.hpp>
#include <optional>

//#include <stk/sim/derivative.hpp>
#include <geometrix/utility/scope_timer.ipp>
#include <boost/units/systems/si.hpp>
#include <geometrix/utility/preprocessor.hpp>
#include <geometrix/utility/assert.hpp>
#include <fstream>
#include <bitset>

 //////////////////////////////////////////////////////////////////////////
//!
//! Tests
//!

constexpr auto ofPrecision = std::numeric_limits<double>::max_digits10;
std::ofstream sLogger;
void           set_logger( const std::string& fname )
{
	sLogger.close();
	sLogger.open( fname );
}
std::ostream& get_logger()
{
	sLogger.precision( ofPrecision );
	return sLogger;
}

template <typename T, typename Expr, typename Derivative>
inline std::optional<T> newton_raphson_method( T initialGuess, std::size_t maxIterations, T epsilon, T tolerance, const Expr& f, const Derivative& fPrime )
{
	using std::abs;
	auto x0 = initialGuess;
	auto x1 = T{};

	get_logger() << "x0: " << x0 << "\n";

	for( auto i = std::size_t{}; i < maxIterations; ++i )
	{
		auto y = f( x0 );
		auto yprime = fPrime( x0 );
	
		get_logger() << "y: " << y << "\n";
		get_logger() << "y': " << yprime << "\n";

		if( abs( yprime ) < epsilon )
			break;

		x1 = x0 - y / yprime;

		get_logger() << "x1: " << x1 << "\n";

		if( abs( x1 - x0 ) <= tolerance )
			return x1;

		x0 = x1;
	}

	return std::nullopt;
}

template <typename Fn>
void log_evaluate(Fn&& f, double xmin, double xmax, double step, const std::string& fnName)
{
	get_logger() << "Evaluating " << fnName << "\n";
	for (auto x = xmin; x <= xmax; x += step)
	{
		auto y = f( x );
		get_logger() << "f(" << x << ")=" << y << "\n"; 
	}
}

#define STK_LOG_EVAL(FName, xmin, xmax, step) \
	log_evaluate( []( double x ) { return FName( x ); }, xmin, xmax, step, BOOST_PP_STRINGIZE(FName) ) \
///

TEST(portable_math_test_suite, report)
{
	using namespace stk;
	set_logger( "math_report.txt" );

	STK_LOG_EVAL( std::sqrt, 0., 100.0, 0.01 );
	STK_LOG_EVAL( std::cos, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
	STK_LOG_EVAL( std::sin, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
	STK_LOG_EVAL( std::exp, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
	STK_LOG_EVAL( std::log, 0., 100.0, 0.01 );

	{
		double x = 50178230318.0;
		double y = 100000000000.0;
		double ratio = x / y;
		get_logger() << x << " / " << y << " == " << ratio << "\n";
		EXPECT_EQ( ratio, 0.50178230318000 );
	}
}

namespace stk {

	template <unsigned int i, unsigned int N, typename Fn, typename ... Args>
	void invoke_range(Fn&& fn, Args&& ... args)
	{
		fn( i, std::forward<Args>( args )... );
		if constexpr( i < N ) 
		{
			invoke_range<i + 1, N>( std::forward<Fn>( fn ), std::forward<Args>( args )... );
		}
	}

	template <typename T>
	void report_number(T v, std::ostream& os)
	{
		auto fp = floating_point_components<T>{ v };
		fp.print( os );
		os << "\n";
	}
	
	template <typename T>
	class running_averager
	{
	public:
		running_averager() = default;
		running_averager( T x )
			: m_oldM{ x }
			, m_newM{ x }
			, m_n{ 1 }
		{}

		void clear()
		{
			m_oldM = m_newM = T{};
			m_n = 0;
		}

		void set( T x )
		{
			m_oldM = m_newM = x;
			m_n = 1;
		}

		BOOST_FORCEINLINE void update( T x )
		{
			if( BOOST_LIKELY( ++m_n != 1 ) )
			{
				m_newM = m_oldM + ( x - m_oldM ) / (double)m_n;
				m_oldM = m_newM;
			}
			else
			{
				m_oldM = m_newM = x;
			}
		}

		BOOST_FORCEINLINE running_averager& operator+=( T x )
		{
			update( x );
			return *this;
		}

		std::uint64_t counts() const
		{
			return m_n;
		}

		T value() const
		{
			return m_newM;
		}

	private:
		std::uint64_t m_n = 0;
		T             m_oldM = T{};
		T             m_newM = T{};
	};

	template <typename A, typename B>
	void extract_stats(A&& est, B&& ref, double& avgRelErr, double& maxRelErro, double& avgAbsErr, double& maxAbsErro)
	{
		running_averager<double> relErrAvger;
		running_averager<double> absErrAvger;
		auto                     maxRelErr = -std::numeric_limits<double>::infinity();
		auto                     maxAbsErr = -std::numeric_limits<double>::infinity();

		for (auto i = 0ULL; i < est.size(); ++i)
		{
			auto e = est[i];
			auto r = ref[i];
			auto absErr = std::fabs( e - r );
			auto relErr = std::fabs(( e - r ) / r);
			relErrAvger.update( relErr );
			absErrAvger.update( absErr );
			maxRelErr = std::max( maxRelErr, relErr );
			maxAbsErr = std::max( maxAbsErr, absErr );
		}

		avgRelErr = relErrAvger.value();
		maxRelErro = maxRelErr;

		avgAbsErr = absErrAvger.value();
		maxAbsErro = maxAbsErr;
	}

}//! namespace stk;

TEST( floating_point_components_test_suite, DISABLED_time_truncation)
{
	using namespace stk;
	using namespace ::testing;

	std::size_t nRuns = 100;
	std::vector<double> src( nRuns );
	for( auto i = 0ULL; i < nRuns; ++i )
		src[i] = double( i );
	auto                nSize = nRuns * (floating_point_traits<double>::mantissa - 2);
	std::vector<double> results( nSize, 0 ), results1( nSize, 0 );

	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "truncation_shift" );
		auto q = 0ULL;
		for( auto i = 0ULL; i < nRuns; ++i )
			for( auto B = 0ULL; B + 2 < floating_point_traits<double>::mantissa; ++B )
				results[q++] = truncate_shift( B, src[i] );
	}
	/*
	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "truncation_shift_mask" );
		auto q = 0ULL;
		for( auto i = 0ULL; i < nRuns; ++i )
			for( auto B = 0ULL; B + 2 < floating_point_traits<double>::mantissa; ++B )
				results1[q++] = truncate_mask_bugged( B, src[i] );
	}
	*/

	//auto r0 = truncate_shift( 51, src[99] );
	//auto r1 = truncate( 51, src[99] );

	std::set<std::size_t> diffs;
	for( auto i = 0; i < results.size(); ++i )
		if( results[i] != results1[i] )
			diffs.insert( i );

	EXPECT_EQ( results, results1 );
}

TEST(floating_point_test_suite, test_component_view)
{
	using namespace stk;
	auto fp = floating_point_components<double>{ 1.0 };
	EXPECT_EQ( fp.bits_value, 0x3FF0000000000000 );
	fp = floating_point_components<double>{ -1.0 };
	EXPECT_EQ( fp.signbit, 1 );

	auto fn = []( unsigned int i )
	{
		double v = i;
		report_number( v, get_logger() );
	};
	invoke_range<0, 1000>( fn );

	set_logger( "decimal_test.txt" );
	auto fn2 = []( unsigned int i )
	{
		double v = std::pow(10.0, -(double)i);
		report_number( v, get_logger() );
	};
	invoke_range<0, 1000>( fn2 );
}

TEST(floating_point_test_suite, test_truncate)
{
	using namespace stk;
	auto fp = floating_point_components<float>{ 1.4142135381698608 };

	auto fn = []( unsigned int i, double v ) 
	{
		auto r = truncate( i, v );
		std::cout.precision( ofPrecision );
		std::cout << r << "\n";
	};
	invoke_range<0,51>( fn, fp.value );
}

/*
TEST( derivative_grammarTestSuite, testOptimizeAdd )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_expr<derivative_constant_terminal<0>>     zero;
	derivative_expr<derivative_constant_terminal<1>>     one;

	auto result0 = derivative_detail::optimize()( one * x + zero );
	static_assert( std::is_same<decltype( x ), decltype( result0 )>::value, "optimize not working" );
}

TEST( derivative_grammarTestSuite, testOptimizeSubtract )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_expr<derivative_constant_terminal<0>>     zero;

	auto result0 = derivative_detail::optimize()( x - zero );
	static_assert( std::is_same<decltype( x ), decltype( result0 )>::value, "optimize not working" );
}

TEST( derivative_grammarTestSuite, testOptimizeDivide )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_expr<derivative_constant_terminal<1>>     one;
	derivative_detail::optimize                          opt;

	auto result0 = opt( x / one );
	static_assert( std::is_same<decltype( x ), decltype( result0 )>::value, "optimize not working" );

	derivative_expr<derivative_constant_terminal<0>> zero;
	auto                                             result1 = opt( zero / x );
	static_assert( std::is_same<decltype( zero ), decltype( result1 )>::value, "optimize not working" );

	auto result2 = opt( ( x - x ) / ( x * x ) );
}

TEST( derivative_grammarTestSuite, testOptimizeProductDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_expr<derivative_constant_terminal<1>>     one;
	derivative_detail::optimize                          opt;

	const derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<x_var>, 0>>&                  a1 = x;
	const derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<x_var>, 0>>&                  a2 = x;
	const derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<derivative_constant<1>>, 0>>& da1 = one;
	const derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<derivative_constant<1>>, 0>>& da2 = one;

	auto result0 = opt( da1 * a2 + a1 * da2 );
	auto result1 = opt( opt( one * x ) + opt( x * one ) );
	static_assert( std::is_same<decltype( result1 ), decltype( result0 )>::value, "optimize not working" );
}

TEST( derivative_grammarTestSuite, testOptimizeQuotient )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_expr<derivative_constant_terminal<1>>     one;
	derivative_detail::optimize                          opt;

	const derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<x_var>, 0>>&                  a1 = x;
	auto                                                                                                                                             a2 = x * x;
	const derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<derivative_constant<1>>, 0>>& da1 = one;
	auto                                                                                                                                             da2 = x + x;
	auto                                                                                                                                             result0 = opt( ( da1 * a2 - da2 * a1 ) / ( a2 * a2 ) );
	auto                                                                                                                                             dresult = result0( 2.0 );
}

TEST( derivative_grammarTestSuite, testDerivativeGrammar )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto   d0 = derivative( x );
	double result = d0( 0 );
	EXPECT_EQ( 1.0, result );

	result = derivative( boost::proto::as_expr( 3 ) )( 0 );
	EXPECT_EQ( 0.0, result );

	auto d = derivative( x + 3 );
	result = d( 5.0 );
	EXPECT_EQ( 1.0, result );

	auto d2 = derivative( x + x );
	result = d2( 6.0 );
	EXPECT_EQ( 2.0, result );

	auto d3 = derivative( x * x );
	result = d3( 6 );
	EXPECT_EQ( 12.0, result );

	auto d4 = derivative( x / ( x * x ) );
	result = d4( 2.0 );
	EXPECT_EQ( -0.25, result );
}

TEST( derivative_grammarTestSuite, testConstantTimesDegree1Poly_ReturnsConstant )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	double constant = 4.0;
	auto   d0 = derivative( constant * x + 1.0 );
	double result = d0( 0 );

	EXPECT_EQ( constant, result );
}

TEST( derivative_grammarTestSuite, testConstantTimesDegree2Poly_ReturnsConstantTimes2TimesVariableEval )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	double constant = 4.0;
	auto   d0 = derivative( constant * x * x + 1.0 );
	double result = d0( 2.0 );

	EXPECT_EQ( 2.0 * 2.0 * constant, result );
}

TEST( derivative_grammarTestSuite, testConstantTimesDegree3Poly_Returns68 )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	double constant = 4.0;
	auto   d0 = derivative( constant * x * x * x + constant * x * x + constant * x + 1.0 );

	//! Derivative should be c*3x^2 + c*2x + c = 12.0 * x^2 + 8.0 * x + 4.0 = 12.0 * 4.0 + 8.0 * 2.0 + 4.0 = 48.0 + 16.0 + 4.0 = 68.0
	double result = d0( 2.0 );

	EXPECT_EQ( 68.0, result );
}

TEST( derivative_grammarTestSuite, testPowDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type>                                                                                      x;
	derivative_grammar                                                                                                                        derivative;
	pow_fun<2>                                                                                                                                fn;
	const derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<x_var>, 0>>            arg;
	derivative_expr<boost::proto::exprns_::expr<boost::proto::tagns_::tag::terminal, boost::proto::argsns_::term<derivative_constant<1>>, 0>> darg;

	auto d = chain_rule()( fn, arg, darg );
	auto d0 = derivative( pow<2>( x ) );

	double result = d0( 2.0 );

	EXPECT_EQ( 4.0, result );
}

TEST( derivative_grammarTestSuite, testPowDerivative4 )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( pow<4>( x ) );

	double result = d0( 2.0 );

	EXPECT_EQ( 32.0, result );
}

TEST( derivative_grammarTestSuite, testPowDerivativePolynomialDegree2 )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	//! = 2 * pow<1>(x*x + 2 * x) * 2x + 2
	//! = 2 * (x*x + 2*x) * 2x + 2 | x = 2
	//! = 2 * (4 + 4) * (4 + 2)
	//! = 2 * 8 * 6
	//! = 2 * 48
	//! = 96
	auto d0 = derivative( pow<2>( x * x + 2.0 * x ) );

	double result = d0( 2.0 );

	EXPECT_EQ( 96.0, result );
}

TEST( derivative_grammarTestSuite, testSinDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( sin( x ) );

	double result = d0( 2.0 );

	EXPECT_EQ( std::cos( 2. ), result );
}

TEST( derivative_grammarTestSuite, testCosDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( cos( x ) );

	double result = d0( 2.0 );

	EXPECT_EQ( -1.0 * std::sin( 2. ), result );
}

TEST( derivative_grammarTestSuite, testExpDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( exp( x ) );

	double result = d0( 2.0 );

	EXPECT_EQ( std::exp( 2. ), result );
}

TEST( derivative_grammarTestSuite, testLogDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( log( pow<3>( x ) ) );

	double result = d0( 4.0 );

	EXPECT_EQ( 0.75, result );
}

TEST( derivative_grammarTestSuite, testSqrtDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( sqrt( pow<3>( x ) ) );

	double result = d0( 4.0 );

	EXPECT_EQ( 3.0, result );
}

TEST( derivative_grammarTestSuite, testSqrtDerivativeWithOptimization )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type>                  x;
	derivative_expr<boost::proto::terminal<derivative_constant<0>>::type> zero;
	derivative_grammar                                                    derivative;

	auto   d0 = derivative( sqrt( pow<3>( x - zero ) ) );
	double result = d0( 4.0 );

	EXPECT_EQ( 3.0, result );
}

TEST( derivative_grammarTestSuite, testMulipleDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	//! 24 * x
	//! = 24 * 4
	//! = 96
	auto d0 = derivative( derivative( derivative( x * x * x * x ) ) );

	auto result = d0( 4.0 );

	EXPECT_EQ( 96.0, result );
}

TEST( derivative_grammarTestSuite, testUnitsSingleDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( x );

	auto result = d0( 4.0 * boost::units::si::meters );

	EXPECT_TRUE( 1.0 == result );
}

TEST( derivative_grammarTestSuite, testUnitsPolynomialDegree2Derivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( x * x );

	auto result = d0( 4.0 * boost::units::si::meters );

	EXPECT_TRUE( 8.0 * boost::units::si::meters == result );
}

TEST( derivative_grammarTestSuite, testUnitsPolynomialDegree3Derivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( x * x * x );

	auto result = d0( 4.0 * boost::units::si::meters );

	EXPECT_TRUE( 48.0 * boost::units::si::square_meters == result );
}

TEST( derivative_grammarTestSuite, testUnitsPolynomialDegree3Derivative2 )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( derivative( x * x * x ) );

	auto result = d0( 4.0 * boost::units::si::meters );

	EXPECT_TRUE( 24.0 * boost::units::si::meters == result );
}

TEST( derivative_grammarTestSuite, testComplexDerivative )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( sqrt( pow<2>( x ) * sin( x ) ) );

	//! d0 == (x*x * cos(x) + 2 * x * sin(x)) / (2 * sqrt(x*x*sin(x)))
	auto result = d0( 4.0 );

	double v = 4.0;
	double expected = ( v * v * std::cos( v ) + 2.0 * v * std::sin( v ) ) / ( 2.0 * std::sqrt( std::abs( v * v * std::sin( v ) ) ) );

	EXPECT_EQ( expected, result );
}

TEST( derivative_grammarTestSuite, testUnitsPolynomialDegree3Derivative2WithUnitConstant )
{
	using namespace stk;
	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;

	auto d0 = derivative( derivative( x * x * x + 2.0 * boost::units::pow<3>( boost::units::si::meters ) ) );

	auto result = d0( 4.0 * boost::units::si::meters );

	EXPECT_TRUE( 24.0 * boost::units::si::meters == result );
}

//! Timings...

//! Test the above mock registrations.
TEST( derivative_grammarTestSuite, DISABLED_time_grammar_evaluation )
{
	using namespace stk;
	using namespace ::testing;

	derivative_expr<boost::proto::terminal<x_var>::type> x;
	derivative_grammar                                   derivative;
#ifdef NDEBUG
	std::size_t nRuns = 100000000;
#else
	std::size_t nRuns = 100;
#endif
	std::vector<double> results( nRuns, 0 ), results1( nRuns, 0 );

	{
		//auto d0(x + pow<2>(x) + pow<3>(x) + pow<4>(x) + pow<5>(x) + pow<6>(x) + pow<7>(x) + pow<8>(x) + pow<9>(x));
		//auto d1(x + pow<2>(x) * (1.0 + x + pow<2>(x) + pow<3>(x) + pow<4>(x) + pow<5>(x) + pow<6>(x) + pow<7>(x)));
		//auto d2(x + pow<2>(x) * (1.0 + x + pow<2>(x) * (1.0 + x + pow<2>(x) + pow<3>(x) + pow<4>(x) + pow<5>(x))));
		//auto d3(x + pow<2>(x) * (1.0 + x + pow<2>(x) * (1.0 + x + pow<2>(x) * (1.0 + x + pow<2>(x) + pow<3>(x)))));
		auto d4 = derivative( x + pow<2>( x ) * ( 1.0 + x + pow<2>( x ) * ( 1.0 + x + pow<2>( x ) * ( 1.0 + x + pow<2>( x ) * ( 1.0 + x ) ) ) ) );

		auto d = derivative( x + pow<2>( x ) + pow<3>( x ) + pow<4>( x ) + pow<5>( x ) + pow<6>( x ) + pow<7>( x ) + pow<8>( x ) + pow<9>( x ) );
		//boost::proto::display_expr( d, std::cout );

		//boost::proto::display_expr( d4, std::cout );
		GEOMETRIX_MEASURE_SCOPE_TIME( "eval_grammar" );
		for( int i = 0; i < nRuns; ++i )
			results[i] = d( 7.7 );
	}

	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "eval_byhand" );
		double v = 7.7;
		for( int i = 0; i < nRuns; ++i )
			results1[i] = 1.0 + 2.0 * v + 3.0 * pow( v, 2 ) + 4.0 * pow( v, 3 ) + 5.0 * pow( v, 4 ) + 6.0 * pow( v, 5 ) + 7.0 * pow( v, 6 ) + 8.0 * pow( v, 7 ) + 9.0 * pow( v, 8 );
	}

	EXPECT_EQ( results, results1 );
}
*/

/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <emmintrin.h>
#ifdef __SSE4_1__
	#include <smmintrin.h>
#endif

// max. rel. error = 1.72863156e-3 on [-87.33654, 88.72283] 
__m128 fast_exp_sse( __m128 x )
{
	__m128  t, f, e, p, r;
	__m128i i, j;
	__m128  l2e = _mm_set1_ps( 1.442695041f ); // log2(e) 
	__m128  c0 = _mm_set1_ps( 0.3371894346f );
	__m128  c1 = _mm_set1_ps( 0.657636276f );
	__m128  c2 = _mm_set1_ps( 1.00172476f );

	// exp(x) = 2^i * 2^f; i = floor (log2(e) * x), 0 <= f <= 1 
	t = _mm_mul_ps( x, l2e ); // t = log2(e) * x 
#ifdef __SSE4_1__
	e = _mm_floor_ps( t );                                             // floor(t) 
	i = _mm_cvtps_epi32( e );                                          // (int)floor(t) 
#else                                                                  // __SSE4_1__
	i = _mm_cvttps_epi32( t );                       // i = (int)t 
	j = _mm_srli_epi32( _mm_castps_si128( x ), 31 ); // signbit(t) 
	i = _mm_sub_epi32( i, j );                       // (int)t - signbit(t) 
	e = _mm_cvtepi32_ps( i );                        // floor(t) ~= (int)t - signbit(t) 
#endif                                                                 // __SSE4_1__
	f = _mm_sub_ps( t, e );                                            // f = t - floor(t) 
	p = c0;                                                            // c0 
	p = _mm_mul_ps( p, f );                                            // c0 * f 
	p = _mm_add_ps( p, c1 );                                           // c0 * f + c1 
	p = _mm_mul_ps( p, f );                                            // (c0 * f + c1) * f 
	p = _mm_add_ps( p, c2 );                                           // p = (c0 * f + c1) * f + c2 ~= 2^f 
	j = _mm_slli_epi32( i, 23 );                                       // i << 23 
	r = _mm_castsi128_ps( _mm_add_epi32( j, _mm_castps_si128( p ) ) ); // r = p * 2^i
	return r;
}

TEST(exp_test_suite, fast_exp_sse)
{
	union
	{
		float        f[4];
		unsigned int i[4];
	} arg, res;
	double relerr, maxrelerr = 0.0;
	int    i, j;
	__m128 x, y;

	float start[2] = { -0.0f, 0.0f };
	float finish[2] = { -87.33654f, 88.72283f };

	for( i = 0; i < 2; i++ )
	{
		arg.f[0] = start[i];
		arg.i[1] = arg.i[0] + 1;
		arg.i[2] = arg.i[0] + 2;
		arg.i[3] = arg.i[0] + 3;
		do
		{
			memcpy( &x, &arg, sizeof( x ) );
			y = fast_exp_sse( x );
			memcpy( &res, &y, sizeof( y ) );
			for( j = 0; j < 4; j++ )
			{
				double ref = exp( (double)arg.f[j] );
				relerr = fabs( ( res.f[j] - ref ) / ref );
				if( relerr > maxrelerr )
				{
					printf( "arg=% 15.8e  res=%15.8e  ref=%15.8e  err=%15.8e\n",
						arg.f[j],
						res.f[j],
						ref,
						relerr );
					maxrelerr = relerr;
				}
			}
			arg.i[0] += 4;
			arg.i[1] += 4;
			arg.i[2] += 4;
			arg.i[3] += 4;
		} while( fabsf( arg.f[3] ) < fabsf( finish[i] ) );
	}
	printf( "maximum relative error = %15.8e\n", maxrelerr );
}
*/

TEST(Math_test_suite, test_sin)
{
	using namespace stk;
	set_logger( "sinestimate.txt" );
	STK_LOG_EVAL( stk::sin, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
	set_logger( "stdsin.txt" );
	STK_LOG_EVAL( std::sin, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
}

TEST(Math_test_suite, test_cos)
{
	using namespace stk;
	auto r = stk::acos( 0.97183309468480805 );
	set_logger( "cosestimate.txt" );
	STK_LOG_EVAL( stk::cos, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
	set_logger( "stdsin.txt" );
	STK_LOG_EVAL( std::cos, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
}

TEST(Math_test_suite, test_exp)
{
	using namespace stk;
	set_logger( "expestimate.txt" );
	STK_LOG_EVAL( stk::exp, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
	set_logger( "stdexp.txt" );
	STK_LOG_EVAL( std::exp, -geometrix::constants::pi<double>(), geometrix::constants::pi<double>(), 0.01 ); 
}

struct timing_harness : ::testing::Test
{
	timing_harness() = default;
	void SetUp()
	{

	}
	void TearDown()
	{

	}

	template <typename Fn>
	void do_timing(const char* fname, Fn&& timing)
	{
		GEOMETRIX_MEASURE_SCOPE_TIME( fname );
		timing();
	}
};

//! Test the above mock registrations.
TEST_F( timing_harness, time_sin )
{
	using namespace stk;
	using namespace ::testing;

#ifdef NDEBUG
	std::size_t nRuns = 100000;
#else
	std::size_t nRuns = 100;
#endif
	auto                nData = 100;
	auto                nResults = nData * nRuns;
	std::vector<double> results( nResults, 0 ), results1( nResults, 0 );

	std::vector<double> src( nData, 0.0 );
	auto xmin = -geometrix::constants::pi<double>(); 
	auto                xmax = -xmin;
	auto                step = ( xmax - xmin ) / nData;
	for( auto i = 0ULL; i < nData; ++i)
	{
		src[i] = xmin + i * step;
	}

	auto sinfn = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results[q++] = std::sin( src[j] );
	};
	do_timing( "std::sin", sinfn );

	auto sinExfn11 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results1[q++] = stk::sin( src[j] );
	};
	do_timing( "stk::sin", sinExfn11);
	double avgRelErr, maxRelErr;
	double avgAbsErr, maxAbsErr;
	extract_stats( results1, results, avgRelErr, maxRelErr, avgAbsErr, maxAbsErr );
	std::cout << "Sin avg. relative error: " << avgRelErr << " max: " << maxRelErr << std::endl;
	std::cout << "Sin avg. absolute error: " << avgAbsErr << " max: " << maxAbsErr << std::endl;
}

TEST_F( timing_harness, time_cos)
{
	using namespace stk;
	using namespace ::testing;

#ifdef NDEBUG
	std::size_t nRuns = 100000;
#else
	std::size_t nRuns = 100;
#endif
	auto                nData = 100;
	auto                nResults = nData * nRuns;
	std::vector<double> results( nResults, 0 ), results1( nResults, 0 );

	std::vector<double> src( nData, 0.0 );
	auto xmin = -geometrix::constants::pi<double>(); 
	auto                xmax = -xmin;
	auto                step = ( xmax - xmin ) / nData;
	for( auto i = 0ULL; i < nData; ++i)
	{
		src[i] = xmin + i * step;
	}

	auto fn1 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results[q++] = std::cos( src[j] );
	};
	do_timing( "std::cos", fn1 );
	
	auto fn2 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results1[q++] = stk::cos( src[j] );
	};
	do_timing( "stk::cos", fn2);
	double avgRelErr, maxRelErr;
	double avgAbsErr, maxAbsErr;
	extract_stats( results1, results, avgRelErr, maxRelErr, avgAbsErr, maxAbsErr );
	std::cout << "Cos avg. relative error: " << avgRelErr << " max: " << maxRelErr << std::endl;
	std::cout << "Cos avg. absolute error: " << avgAbsErr << " max: " << maxAbsErr << std::endl;
}
TEST_F( timing_harness, time_exp)
{
	using namespace stk;
	using namespace ::testing;

#ifdef NDEBUG
	std::size_t nRuns = 100000;
#else
	std::size_t nRuns = 100;
#endif
	auto                nData = 100;
	auto                nResults = nData * nRuns;
	std::vector<double> results( nResults, 0 ), results1( nResults, 0 );

	std::vector<double> src( nData, 0.0 );
	auto xmin = -geometrix::constants::pi<double>(); 
	auto                xmax = -xmin;
	auto                step = ( xmax - xmin ) / nData;
	for( auto i = 0ULL; i < nData; ++i)
	{
		src[i] = xmin + i * step;
	}

	auto fn1 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results[q++] = std::exp( src[j] );
	};
	do_timing( "std::exp", fn1 );

	auto fn2 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results1[q++] = stk::exp( src[j] );
	};
	do_timing( "stk::exp", fn2);

	double avgRelErr, maxRelErr;
	double avgAbsErr, maxAbsErr;
	extract_stats( results1, results, avgRelErr, maxRelErr, avgAbsErr, maxAbsErr );
	std::cout << "Exp avg. relative error: " << avgRelErr << " max: " << maxRelErr << std::endl;
	std::cout << "Exp avg. absolute error: " << avgAbsErr << " max: " << maxAbsErr << std::endl;
}

TEST_F(timing_harness, DISABLED_test_sqrt)//! std::sqrt is seemingly deterministic/portable.
{
	using namespace stk;
	using namespace ::testing;
	set_logger( "sqrtestimate.txt" );
	STK_LOG_EVAL( stk::sqrt, 0.0, geometrix::constants::pi<double>(), 0.01 ); 
	set_logger( "stdsqrt.txt" );
	STK_LOG_EVAL( std::sqrt, 0.0, geometrix::constants::pi<double>(), 0.01 ); 
#ifdef NDEBUG
	std::size_t nRuns = 100000;
#else
	std::size_t nRuns = 100;
#endif
	auto                nData = 100;
	auto                nResults = nData * nRuns;
	std::vector<double> results( nResults, 0 ), results1( nResults, 0 );

	std::vector<double> src( nData, 0.0 );
	auto                xmin = 0.0;
	auto                xmax = geometrix::constants::pi<double>(); 
	auto                step = ( xmax - xmin ) / nData;
	for( auto i = 0ULL; i < nData; ++i)
	{
		src[i] = xmin + i * step;
	}

	auto fn1 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results[q++] = std::sqrt( src[j] );
	};
	do_timing( "std::sqrt", fn1 );
	
	auto fn2 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				results1[q++] = stk::sqrt( src[j] );
	};
	do_timing( "stk::sqrt", fn2);
}

TEST(math_test_suite, test_zero)
{
	{
		auto szero = std::sin( 0.0 );
		auto sezero = stk::sin( 0.0 );
		EXPECT_EQ( szero, sezero );
	}
	{
		auto szero = std::cos( 0.0 );
		auto sezero = stk::cos( 0.0 );
		EXPECT_EQ( szero, sezero );
	}
	{
		auto szero = std::exp( 0.0 );
		auto sezero = stk::exp( 0.0 );
		EXPECT_EQ( szero, sezero );
	}
}

TEST_F( timing_harness, time_atan2)
{
	using namespace stk;
	using namespace ::testing;

	std::size_t nRuns = 100;
	auto                nData = 100;
	auto                nResults = nData * nData * nRuns;
	std::vector<double> results( nResults, 0 ), results1( nResults, 0 );

	std::vector<double> src( nData, 0.0 );
	auto xmin = -geometrix::constants::pi<double>(); 
	auto                xmax = -xmin;
	auto                step = ( xmax - xmin ) / nData;
	for( auto i = 0ULL; i < nData; ++i)
	{
		src[i] = xmin + i * step;
	}

	auto fn1 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				for( int k = 0; k < src.size(); ++k )
				results[q++] = std::atan2( src[j], src[k] );
	};
	do_timing( "std::atan2", fn1 );
	
	auto fn2 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				for( int k = 0; k < src.size(); ++k )
					results1[q++] = stk::atan2( src[j], src[k] );
	};
	do_timing( "stk::atan2", fn2);
	double avgRelErr, maxRelErr;
	double avgAbsErr, maxAbsErr;
	extract_stats( results1, results, avgRelErr, maxRelErr, avgAbsErr, maxAbsErr );
	std::cout << "atan2 avg. relative error: " << avgRelErr << " max: " << maxRelErr << std::endl;
	std::cout << "atan2 avg. absolute error: " << avgAbsErr << " max: " << maxAbsErr << std::endl;
}

TEST_F( timing_harness, time_pow)
{
	using namespace stk;
	using namespace ::testing;

	std::size_t nRuns = 100;
	auto                nData = 100;
	auto                nResults = nData * nData * nRuns;
	std::vector<double> results( nResults, 0 ), results1( nResults, 0 );

	std::vector<double> src( nData, 0.0 );
	auto xmin = -geometrix::constants::pi<double>(); 
	auto                xmax = -xmin;
	auto                step = ( xmax - xmin ) / nData;
	for( auto i = 0ULL; i < nData; ++i)
	{
		src[i] = xmin + i * step;
	}

	auto fn1 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				for( int k = 0; k < src.size(); ++k )
				results[q++] = std::pow( src[j], src[k] );
	};
	do_timing( "std::pow", fn1 );
	
	auto fn2 = [&]()
	{
		auto q = 0ULL;
		for( int i = 0; i < nRuns; ++i )
			for( int j = 0; j < src.size(); ++j )
				for( int k = 0; k < src.size(); ++k )
					results1[q++] = stk::pow( src[j], src[k] );
	};
	do_timing( "stk::pow", fn2);
	double avgRelErr, maxRelErr;
	double avgAbsErr, maxAbsErr;
	extract_stats( results1, results, avgRelErr, maxRelErr, avgAbsErr, maxAbsErr );
	std::cout << "pow avg. relative error: " << avgRelErr << " max: " << maxRelErr << std::endl;
	std::cout << "pow avg. absolute error: " << avgAbsErr << " max: " << maxAbsErr << std::endl;
}

TEST(stk_math_test_suite, test_pow_double_int)
{
	auto a = 1.0;
	auto b = 1LL;

	auto r = stk::pow( a, b );
	EXPECT_EQ( r, 1.0 );
}

