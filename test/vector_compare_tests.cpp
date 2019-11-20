#include <gtest/gtest.h>

#include <cstdint>
#include <stk/container/icl/interval_set.hpp>
#include <geometrix/numeric/constants.hpp>
#include <geometrix/numeric/boost_units_quantity.hpp>
#include <boost/units/cmath.hpp>
#include <geometrix/algebra/expression.hpp>
#include <stk/units/boost_units.hpp>
#include <stk/geometry/tolerance_policy.hpp>
#include <geometrix/utility/vector_angle_compare.hpp>
#include <geometrix/primitive/point.hpp>
#include <geometrix/tensor/vector.hpp>
#include <geometrix/tensor/is_null.hpp>
#include <geometrix/numeric/interval.hpp>

template <typename T>
struct v_compare
{
	bool operator()(const T&lhs, const T& rhs)
	{
		return pseudo_angle(lhs) < pseudo_angle(rhs);
	}
};

struct comparable_vector : geometrix::vector_double_2d
{
	template <typename ...Args>
	comparable_vector(Args&&... a)
		: geometrix::vector_double_2d{std::forward<Args>(a)...}
	{}

	bool operator <(const geometrix::vector_double_2d& v) const
	{
		using namespace geometrix;
		return pseudo_angle((const geometrix::vector_double_2d&)(*this)) < pseudo_angle(v);
	}
};

struct comparable_vector_access_policy 
{
	template <unsigned int Index>
	struct type_at
	{
		typedef double type;
	};

	template <unsigned int Index>
	static double get(const comparable_vector& p){ return p[Index]; }

	template <unsigned int Index>
	static void set(comparable_vector& p, double v) { p[Index] = v; }
};

//GENERATIVE_GEOMETRY_DEFINE_SEQUENCE_TRAITS( GKVector, (double), 2 );//
// Here is a macro declaration that turns this simple struct into a GGA enabled point type with
// a cartesian reference frame and a preference for compile time access semantics.
GEOMETRIX_DEFINE_VECTOR_TRAITS
(
	  comparable_vector                                                  // The real type
	, (double)                                               // The underlying coordinate type
	, 2                                                                  // The dimensionality of the point type
	, double// dimensionless type
	, double // arithmetic type
	, geometrix::neutral_reference_frame_2d                              // The default reference frame
	, comparable_vector_access_policy// The preferred index access policy
);

//! Define how to construct the point type.

namespace geometrix {

    template <>
    struct construction_policy< comparable_vector >
    {    
        static comparable_vector construct(const double& x, const double& y)
        {
            return comparable_vector( x,y );
        }

        template <typename NumericSequence>
        static comparable_vector construct( const NumericSequence& args )
        {
            return comparable_vector( geometrix::get<0>( args ), geometrix::get<1>( args ) );
        }
    };

}//namespace geometrix;

namespace stk { namespace icl {
    template<>
    struct interval_traits<geometrix::interval<comparable_vector>>
    {
        using type = interval_traits;
        using domain_type = comparable_vector;
        using domain_compare = v_compare<domain_type>;
        using interval_type = geometrix::interval<domain_type>;

        static interval_type construct(const domain_type& lo, const domain_type& up)
        {
            return interval_type(lo, up);
        }

        static domain_type lower(const interval_type& inter_val){ return inter_val.lower(); };
        static domain_type upper(const interval_type& inter_val){ return inter_val.upper(); };
    };

    template<>
    struct dynamic_interval_traits<geometrix::interval<comparable_vector>>
    {
        using type = dynamic_interval_traits;
        using domain_type = comparable_vector;
        using interval_type = geometrix::interval<domain_type>;
        using domain_compare = v_compare<domain_type>;

        static interval_type construct(const domain_type& lo, const domain_type& up, interval_bounds bounds)
        {
            return interval_type(lo, up, bounds);
        }

        static interval_type construct_bounded(const bounded_value<domain_type>& lo, const bounded_value<domain_type>& up)
        {
            return interval_type(lo.value(), up.value(), lo.bound().left() | up.bound().right());
        }
    };

    template<typename T, std::size_t D>
    struct interval_traits<geometrix::interval<geometrix::vector<T, D>>>
    {
        using type = interval_traits;
        using domain_type = geometrix::vector<T, D>;
        using domain_compare = v_compare<domain_type>;
        using interval_type = geometrix::interval<domain_type>;

        static interval_type construct(const domain_type& lo, const domain_type& up)
        {
            return interval_type(lo, up);
        }

        static domain_type lower(const interval_type& inter_val){ return inter_val.lower(); };
        static domain_type upper(const interval_type& inter_val){ return inter_val.upper(); };
    };

    template<typename T, std::size_t D>
    struct dynamic_interval_traits<geometrix::interval<geometrix::vector<T, D>>>
    {
        using type = dynamic_interval_traits;
        using domain_type = geometrix::vector<T, D>;
        using interval_type = geometrix::interval<domain_type>;
        using domain_compare = v_compare<domain_type>;

        static interval_type construct(const domain_type& lo, const domain_type& up, interval_bounds bounds)
        {
            return interval_type(lo, up, bounds);
        }

        static interval_type construct_bounded(const bounded_value<domain_type>& lo, const bounded_value<domain_type>& up)
        {
            return interval_type(lo.value(), up.value(), lo.bound().left() | up.bound().right());
        }
    };
}}//! stk::icl;

#include <boost/container/flat_set.hpp>
struct flat_set_generator 
{
    template <typename Key, typename Compare = std::less<Key>, typename Alloc = std::allocator<Key> >
    struct generate_type
    {
        typedef typename boost::container::flat_set<Key,Compare,Alloc> type;
    };

};

template <typename Vector>
using vector_interval = geometrix::interval<Vector>;
TEST(interval_set_test_suit, vector_interval_construct)
{
	using namespace stk::icl;
	using namespace geometrix;
	using vector2 = vector_double_2d;
	using point2 = point_double_2d;
    using interval_t = vector_interval<comparable_vector>;
    static_assert(is_continuous_interval<interval_t>::value, "should be true!!!!!!!!!!!!!!!!!!!!");
    static_assert(is_interval<interval_t>::value, "should be true!!!!!!!!!!!!!!!!!!!!");
    static_assert(std::is_same<interval_traits<interval_t>::domain_type, comparable_vector>::value, "should be true!!!!!!!!!!!!!!!!!!!!");
    static_assert(stk::icl::interval_bound_type<interval_t>::value == stk::icl::interval_bounds::dynamic, "should be true!!!!!!!!!!!!!!!!!!!!");
    static_assert(stk::icl::has_dynamic_bounds<interval_t>::value, "should be true!!!!!!!!!!!!!!!!!!!!");
	auto v1 = comparable_vector{ 1.0, 1.0 };
	auto v2 = comparable_vector{ 1.0, 0.0 };
    EXPECT_TRUE(stk::icl::domain_less<interval_t>(v2, v1));
    auto i1 = interval_t{v1, v2};
	auto v3 = comparable_vector{ -1.0, 1.0 };
	auto v4 = comparable_vector{ -1.0, 0.0 };
    auto i2 = interval_t{v3, v4};
    lower_less(i1, i2);

	using iset = stk::icl::interval_set<comparable_vector, v_compare, interval_t>;

    auto sut = iset{};
	sut.insert(i1);
	sut.insert(i2);
}

TEST(interval_set_test_suit, vector_interval_set)
{
	using namespace geometrix;
	using point2 = point_double_2d;
	std::vector<point2> points;
	std::vector<comparable_vector> vectors;
	std::vector<double> pangles;

    static_assert(stk::icl::is_continuous_interval<vector_interval<comparable_vector>>::value, "should be true!!!!!!!!!!!!!!!!!!!!");

	auto p = point2{ 0., 0. };
	auto v = comparable_vector{ 1.0, 0.0 };
	
	using namespace stk::icl;
	using iset = stk::icl::interval_set<comparable_vector, v_compare, vector_interval<comparable_vector>>;
	using interval_t = iset::interval_type;
	auto sut = iset{};

	auto s = 2.0 * constants::pi<double>() / 100.0;
	for (auto i = 0UL; i < 101; ++i)
	{
		points.emplace_back(p + v);
		vectors.emplace_back(v);
		pangles.emplace_back(pseudo_angle(v));
		auto lv = comparable_vector{ s * normalize(perp(v)) };
		auto vnew = normalize(v + lv);
		if(i % 25)
			sut.insert(interval_t::right_open(v, vnew));
		v = vnew;
	}

	EXPECT_EQ(4, sut.iterative_size());

	EXPECT_TRUE(stk::icl::contains(sut, interval_t::closed(comparable_vector{ 1,1 })));
	EXPECT_FALSE(stk::icl::contains(sut, interval_t::closed(comparable_vector{ 0,1 })));
}

TEST(interval_set_test_suit, vector_interval_set_vector2)
{
	using namespace geometrix;
	using point2 = point_double_2d;
	using vector2 = vector_double_2d;
	std::vector<point2> points;
	std::vector<vector2> vectors;
	std::vector<double> pangles;

    static_assert(stk::icl::is_continuous_interval<vector_interval<vector2>>::value, "should be true!!!!!!!!!!!!!!!!!!!!");

	auto p = point2{ 0., 0. };
	auto v = vector2{ 1.0, 0.0 };
	
	using namespace stk::icl;
	using iset = stk::icl::interval_set<vector2, v_compare, vector_interval<vector2>, std::allocator, flat_set_generator>;
	using interval_t = iset::interval_type;
	auto sut = iset{};

	auto s = 2.0 * constants::pi<double>() / 100.0;
	for (auto i = 0UL; i < 101; ++i)
	{
		points.emplace_back(p + v);
		vectors.emplace_back(v);
		pangles.emplace_back(pseudo_angle(v));
		auto lv = vector2{ s * normalize(perp(v)) };
		auto vnew = normalize(v + lv);
		if(i % 25)
			sut.insert(interval_t::right_open(v, vnew));
		v = vnew;
	}

	EXPECT_EQ(4, sut.iterative_size());

	EXPECT_TRUE(stk::icl::contains(sut, interval_t::closed(vector2{ 1., 1. })));
	EXPECT_FALSE(stk::icl::contains(sut, interval_t::closed(vector2{ 0., 1. })));
}

#include <geometrix/primitive/sphere.hpp>
#include <geometrix/primitive/segment.hpp>
using namespace geometrix;
using point2 = geometrix::point<double, 2>;
using segment2 = geometrix::segment<point2>;
using vector2 = geometrix::vector<double, 2>;
using velocity2 = geometrix::vector<double, 2>;
using circle2 = geometrix::sphere<2, point2>;

struct collision_cone
{
	point2 m_apex;
	vector2 m_left;
	segment2 m_ls;
	vector2 m_right;
	segment2 m_rs;
};

#include <geometrix/algorithm/intersection/sphere_sphere_intersection.hpp>
std::pair<point2, point2> calculate_tangent_points(const circle2& c, const point2& p)
{
	auto cp = vector2{ c.get_center() - p };

	auto cOP = circle2{ p + 0.5 * cp, 0.5 * magnitude(cp) };
	auto r = sphere_sphere_intersection(c, cOP, direct_comparison_policy{});
	return std::make_pair(*r.IntersectionPoint0, *r.IntersectionPoint1);
}

struct velocity_obstacle
{
	velocity_obstacle(const circle2& a, const velocity2& /*va*/, const circle2& b, const velocity2& vb)
	{
		auto Rb = a.get_radius() + b.get_radius();
		//auto ab = vector2{ b.get_center() - a.get_center() };

		point2 tl, tr;
		std::tie(tl, tr) = calculate_tangent_points(circle2{ b.get_center(), Rb }, a.get_center());
		auto dl = magnitude(tl - a.get_center());

		m_cc.m_left = vector2{ (tl - a.get_center()) / dl };
		auto dr = magnitude(tr - a.get_center());
		m_cc.m_right = vector2{ (tr - a.get_center()) / dr };
		m_cc.m_apex = a.get_center() + 1.0 * vb;

		m_cc.m_ls = segment2{ m_cc.m_apex, m_cc.m_apex + dl * m_cc.m_left };
		m_cc.m_rs = segment2{ m_cc.m_apex, m_cc.m_apex + dr * m_cc.m_right };
	}

	collision_cone m_cc;
};

#include <geometrix/algorithm/intersection/moving_sphere_sphere_intersection.hpp>
#include <geometrix/algorithm/intersection/segment_segment_intersection.hpp>

TEST(velocity_obstacle_test_suite, test_construct)
{
	auto p1 = point2{ 0.0, 0.0 };
	auto r1 = double{ 0.3 };
	auto c1 = circle2{ p1, r1 };
	auto v1 = velocity2{ 0.0, 1.0 };
	
	auto p2 = point2{ 1.0, 1.0 };
	auto r2 = double{ 0.3 };
	auto c2 = circle2{ p2, r2 };
	auto v2 = velocity2{ -1.0, 0.0 };

	auto vob = velocity_obstacle{c1, v1, c2, v2};

	point2 xpoint[2];
	auto r = segment_segment_intersection(vob.m_cc.m_rs, segment2{ p1, p1 + 1.0 * v1 }, xpoint, absolute_tolerance_comparison_policy<double>{});
	EXPECT_TRUE(r == e_crossing);

	auto ss = moving_sphere_sphere_intersection(c1, c2, v1, v2, absolute_tolerance_comparison_policy<double>{});
	EXPECT_TRUE(ss);

	auto vs = velocity2{ 0.00000000000000000, 0.36191 /*0.36191420548134096*/ };
	ss = moving_sphere_sphere_intersection(c1, c2, vs, v2, absolute_tolerance_comparison_policy<double>{});
	EXPECT_FALSE(ss);
}

TEST(interval_test_suite, construct)
{
	using namespace geometrix;
	auto sut = interval<double>{};
	EXPECT_TRUE(sut.is_empty());
	sut.expand(0.0);
	EXPECT_EQ(0.0, sut.lower());
	EXPECT_EQ(0.0, sut.upper());
	sut = sut + 1.0;
	EXPECT_EQ(1.0, sut.lower());
	EXPECT_EQ(1.0, sut.upper());
	sut = sut * 2.0;
	EXPECT_EQ(2.0, sut.lower());
	EXPECT_EQ(2.0, sut.upper());
	sut = sut - 1.0;
	EXPECT_EQ(1.0, sut.lower());
	EXPECT_EQ(1.0, sut.upper());
	sut = sut / 2.0;
	EXPECT_EQ(0.5, sut.lower());
	EXPECT_EQ(0.5, sut.upper());
}
