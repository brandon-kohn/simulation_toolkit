#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <geometrix/numeric/constants.hpp>
#include <cmath>
#include <cstdint>
#include <optional>

#include <stk/sim/derivative.hpp>
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
	{
		derivative_expr<boost::proto::terminal<x_var>::type> x;
		derivative_grammar                                   derivative;

		auto f = []( double x )
		{
			return x * x * cos( x );
		};
		auto d0 = derivative( pow<2>( x ) * cos( x ) );

		auto guess = 3.14;

		auto r = newton_raphson_method( guess, 100, 1e-14, 1e-10, f, d0 );
	}

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

	template <typename T, typename EnableIf=void>
	struct floating_point_traits;

	template <>
	struct floating_point_traits<double>
	{
		BOOST_STATIC_CONSTEXPR std::uint8_t mantissa = 52;
		BOOST_STATIC_CONSTEXPR std::uint8_t exponent = 11;
		BOOST_STATIC_CONSTEXPR std::uint8_t signbit  = 1;
		BOOST_STATIC_CONSTEXPR std::uint16_t exponent_bias = 1023;
	};
	
	template <>
	struct floating_point_traits<float>
	{
		BOOST_STATIC_CONSTEXPR std::uint8_t mantissa = 23;
		BOOST_STATIC_CONSTEXPR std::uint8_t exponent = 8;
		BOOST_STATIC_CONSTEXPR std::uint8_t signbit  = 1;
		BOOST_STATIC_CONSTEXPR std::uint8_t exponent_bias = 127;
	};

	//! Assumes little-endian.
	template <typename T>
	struct floating_point_components
	{
		using value_type = typename std::decay<T>::type;
		constexpr floating_point_components( value_type v = value_type{} )
			: value{ v }
		{}

		bool get_sign_bit() const { return signbit; }
		value_type get_exponent() const { return (int)( exponent - floating_point_traits<value_type>::exponent_bias ); }
		value_type get_mantissa() const 
		{
			auto v = value_type{1};
			std::bitset<floating_point_traits<value_type>::mantissa> mbits( mantissa );
			for( int i = floating_point_traits<value_type>::mantissa; i > 0; --i ) 
				if(mbits[floating_point_traits<value_type>::mantissa - i])
					v += std::pow( 2.0, static_cast<value_type>( -i ) );
			return v;
		}

		void print_mantissa(std::ostream& os) const
		{
			std::bitset<floating_point_traits<value_type>::mantissa> v( mantissa );
			os << "{" << v << "} ";
			os << "(" << mantissa << ")";
		}
		
		void print_exponent(std::ostream& os) const
		{
			std::bitset<floating_point_traits<value_type>::exponent> v( exponent );
			os << "{" << v << "} ";
			os << "(" << (int)exponent - floating_point_traits<value_type>::exponent_bias << ")";
		}
		
		void print_signbit(std::ostream& os) const
		{
			std::bitset<floating_point_traits<value_type>::signbit> v( signbit );
			os << "{" << v << "} ";
			os << "(" << signbit << ")";
		}
		
		void print_bits(std::ostream& os) const
		{
			std::bitset<sizeof(std::uint64_t)*8> v( bits_value );
			os << "{" << v << "} ";
			os << "(" << bits_value << ")";
		}
		
		void print_reconstituted(std::ostream& os) const
		{
			auto v = value_type{};

			if (mantissa || exponent) {
				v = std::pow( 2.0, get_exponent() ) * get_mantissa(); 
			}

			if (get_sign_bit()) {
				v *= -1;
			}

			os << "model: [" << v << "]";
		}

		void print(std::ostream& os) const
		{
			os.precision( ofPrecision );
			os << "value: " << value;
			os << "\nsign bit: ";
			print_signbit( os );
			os << "\nexponent: ";
			print_exponent( os );
			os << "\nmantissa: ";
			print_mantissa( os );
			os << "\nbits: ";
			print_bits( os );
			os << "\n";
			print_reconstituted( os );
			os << "\n";
		}

		union
		{
			value_type value;
			std::uint64_t bits_value;
			struct
			{
				std::uint64_t mantissa : floating_point_traits<value_type>::mantissa;
				std::uint64_t exponent : floating_point_traits<value_type>::exponent;
				std::uint64_t  signbit : floating_point_traits<value_type>::signbit;
			};
		};
	};

	template <typename T>
	constexpr T truncate_mask_bugged( std::uint64_t Bit, T v )
	{
		GEOMETRIX_ASSERT(Bit + 2 < floating_point_traits<T>::mantissa);
		//! This is faster, but can't handle the MSB of the mantissa.
		auto fp = floating_point_components<T>{ v };
		fp.mantissa &= ~( ( 1ULL << (Bit + 1ULL) ) - 1ULL );
		return fp.value;
	}
	
	template <typename T>
	constexpr T truncate_shift( std::uint64_t Bit, T v )
	{
		//! Slower but handles all bits of mantissa.
		auto fp = floating_point_components<T>{ v };
		fp.mantissa = (fp.mantissa >> Bit) << Bit;
		return fp.value;
	}
	
	template <typename T>
	constexpr T truncate( std::uint64_t Bit, T v )
	{
		return truncate_shift( Bit, v );
	}

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
TEST( derivative_grammarTestSuite, time_grammar_evaluation )
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
