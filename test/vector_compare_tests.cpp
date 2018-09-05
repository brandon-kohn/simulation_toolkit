#include <gtest/gtest.h>

#include <cstdint>
#include <boost/icl/interval_set.hpp>
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

namespace boost{namespace icl
{

template <class DomainT, 
          ICL_COMPARE Compare = ICL_COMPARE_INSTANCE(ICL_COMPARE_DEFAULT, DomainT)>
class continuous_interval_custom_cmp
{
public:
    typedef continuous_interval_custom_cmp<DomainT,Compare> type;
    typedef DomainT domain_type;
    typedef ICL_COMPARE_DOMAIN(Compare,DomainT) domain_compare;
    typedef typename bounded_value<DomainT>::type bounded_domain_type;

public:
    //==========================================================================
    //= Construct, copy, destruct
    //==========================================================================
    /** Default constructor; yields an empty interval <tt>[0,0)</tt>. */
    continuous_interval_custom_cmp()
        : _lwb(identity_element<DomainT>::value()), _upb(identity_element<DomainT>::value())
        , _bounds(interval_bounds::right_open())
    {
        BOOST_CONCEPT_ASSERT((DefaultConstructibleConcept<DomainT>));
        //BOOST_CONCEPT_ASSERT((LessThanComparableConcept<DomainT>));
        BOOST_STATIC_ASSERT((icl::is_continuous<DomainT>::value)); 
    }

    //NOTE: Compiler generated copy constructor is used

    /** Constructor for a closed singleton interval <tt>[val,val]</tt> */
    explicit continuous_interval_custom_cmp(const DomainT& val)
        : _lwb(val), _upb(val), _bounds(interval_bounds::closed())
    {
        BOOST_CONCEPT_ASSERT((DefaultConstructibleConcept<DomainT>));
        //BOOST_CONCEPT_ASSERT((LessThanComparableConcept<DomainT>));
        BOOST_STATIC_ASSERT((icl::is_continuous<DomainT>::value));
    }

    /** Interval from <tt>low</tt> to <tt>up</tt> with bounds <tt>bounds</tt> */
    continuous_interval_custom_cmp(const DomainT& low, const DomainT& up, 
                      interval_bounds bounds = interval_bounds::right_open(),
                      continuous_interval_custom_cmp* = 0)
        : _lwb(low), _upb(up), _bounds(bounds)
    {
        BOOST_CONCEPT_ASSERT((DefaultConstructibleConcept<DomainT>));
        //BOOST_CONCEPT_ASSERT((LessThanComparableConcept<DomainT>));
        BOOST_STATIC_ASSERT((icl::is_continuous<DomainT>::value));
    }

    domain_type     lower()const { return _lwb; }
    domain_type     upper()const { return _upb; }
    interval_bounds bounds()const{ return _bounds; }

    static continuous_interval_custom_cmp open     (const DomainT& lo, const DomainT& up){ return continuous_interval_custom_cmp(lo, up, interval_bounds::open());      }
    static continuous_interval_custom_cmp right_open(const DomainT& lo, const DomainT& up){ return continuous_interval_custom_cmp(lo, up, interval_bounds::right_open());}
    static continuous_interval_custom_cmp left_open (const DomainT& lo, const DomainT& up){ return continuous_interval_custom_cmp(lo, up, interval_bounds::left_open()); }
    static continuous_interval_custom_cmp closed   (const DomainT& lo, const DomainT& up){ return continuous_interval_custom_cmp(lo, up, interval_bounds::closed());    }

private:
    domain_type     _lwb;
    domain_type     _upb;
    interval_bounds _bounds;
};


//==============================================================================
//=T continuous_interval_custom_cmp -> concept interval
//==============================================================================
template<class DomainT, ICL_COMPARE Compare>
struct interval_traits< icl::continuous_interval_custom_cmp<DomainT, Compare> >
{
    typedef interval_traits type;
    typedef DomainT domain_type;
    typedef ICL_COMPARE_DOMAIN(Compare,DomainT) domain_compare;
    typedef icl::continuous_interval_custom_cmp<DomainT, Compare> interval_type;

    static interval_type construct(const domain_type& lo, const domain_type& up)
    {
        return interval_type(lo, up);
    }

    static domain_type lower(const interval_type& inter_val){ return inter_val.lower(); };
    static domain_type upper(const interval_type& inter_val){ return inter_val.upper(); };
};


//==============================================================================
//=T continuous_interval_custom_cmp -> concept dynamic_interval
//==============================================================================
template<class DomainT, ICL_COMPARE Compare>
struct dynamic_interval_traits<boost::icl::continuous_interval_custom_cmp<DomainT,Compare> >
{
    typedef dynamic_interval_traits type;
    typedef boost::icl::continuous_interval_custom_cmp<DomainT,Compare> interval_type;
    typedef DomainT domain_type;
    typedef ICL_COMPARE_DOMAIN(Compare,DomainT) domain_compare;

    static interval_type construct(const domain_type lo, const domain_type up, interval_bounds bounds)
    {
        return icl::continuous_interval_custom_cmp<DomainT,Compare>(lo, up, bounds,
            static_cast<icl::continuous_interval_custom_cmp<DomainT,Compare>* >(0) );
    }

    static interval_type construct_bounded(const bounded_value<DomainT>& lo, 
                                           const bounded_value<DomainT>& up)
    {
        return  icl::continuous_interval_custom_cmp<DomainT,Compare>
                (
                    lo.value(), up.value(),
                    lo.bound().left() | up.bound().right(),
                    static_cast<icl::continuous_interval_custom_cmp<DomainT,Compare>* >(0) 
                );
    }
};

//==============================================================================
//= Type traits
//==============================================================================
template <class DomainT, ICL_COMPARE Compare> 
struct interval_bound_type< continuous_interval_custom_cmp<DomainT,Compare> >
{
    typedef interval_bound_type type;
    BOOST_STATIC_CONSTANT(bound_type, value = interval_bounds::dynamic);
};

template <class DomainT, ICL_COMPARE Compare> 
struct is_continuous_interval<continuous_interval_custom_cmp<DomainT,Compare> >
{
    typedef is_continuous_interval<continuous_interval_custom_cmp<DomainT,Compare> > type;
    BOOST_STATIC_CONSTANT(bool, value = true);
};

template <class DomainT, ICL_COMPARE Compare>
struct type_to_string<icl::continuous_interval_custom_cmp<DomainT,Compare> >
{
    static std::string apply()
    { return "cI<"+ type_to_string<DomainT>::apply() +">"; }
};

template<class DomainT> 
struct value_size<icl::continuous_interval_custom_cmp<DomainT> >
{
    static std::size_t apply(const icl::continuous_interval_custom_cmp<DomainT>&) 
    { return 2; }
};

	template <class DomainT,
		ICL_COMPARE Compare = ICL_COMPARE_INSTANCE(ICL_COMPARE_DEFAULT, DomainT)>
		class discrete_interval_custom_cmp
	{
	public:
		typedef discrete_interval_custom_cmp<DomainT, Compare> type;
		typedef DomainT domain_type;
		typedef ICL_COMPARE_DOMAIN(Compare, DomainT) domain_compare;
		typedef typename bounded_value<DomainT>::type bounded_domain_type;

	public:
		//==========================================================================
		//= Construct, copy, destruct
		//==========================================================================
		/** Default constructor; yields an empty interval <tt>[0,0)</tt>. */
		discrete_interval_custom_cmp()
			: _lwb(identity_element<DomainT>::value()), _upb(identity_element<DomainT>::value())
			, _bounds(interval_bounds::right_open())
		{
			BOOST_CONCEPT_ASSERT((DefaultConstructibleConcept<DomainT>));
			//BOOST_CONCEPT_ASSERT((LessThanComparableConcept<DomainT>));
			BOOST_STATIC_ASSERT((icl::is_discrete<DomainT>::value));
		}

		//NOTE: Compiler generated copy constructor is used

		/** Constructor for a closed singleton interval <tt>[val,val]</tt> */
		explicit discrete_interval_custom_cmp(const DomainT& val)
			: _lwb(val), _upb(val), _bounds(interval_bounds::closed())
		{
			BOOST_CONCEPT_ASSERT((DefaultConstructibleConcept<DomainT>));
			//BOOST_CONCEPT_ASSERT((LessThanComparableConcept<DomainT>));
			BOOST_STATIC_ASSERT((icl::is_discrete<DomainT>::value));
		}

		/** Interval from <tt>low</tt> to <tt>up</tt> with bounds <tt>bounds</tt> */
		discrete_interval_custom_cmp(const DomainT& low, const DomainT& up,
						  interval_bounds bounds = interval_bounds::right_open(),
						  discrete_interval_custom_cmp* = 0)
			: _lwb(low), _upb(up), _bounds(bounds)
		{
			BOOST_CONCEPT_ASSERT((DefaultConstructibleConcept<DomainT>));
			//BOOST_CONCEPT_ASSERT((LessThanComparableConcept<DomainT>));
			BOOST_STATIC_ASSERT((icl::is_discrete<DomainT>::value));
		}

		domain_type     lower()const
		{
			return _lwb;
		}
		domain_type     upper()const
		{
			return _upb;
		}
		interval_bounds bounds()const
		{
			return _bounds;
		}

		static discrete_interval_custom_cmp open(const DomainT& lo, const DomainT& up)
		{
			return discrete_interval_custom_cmp(lo, up, interval_bounds::open());
		}
		static discrete_interval_custom_cmp right_open(const DomainT& lo, const DomainT& up)
		{
			return discrete_interval_custom_cmp(lo, up, interval_bounds::right_open());
		}
		static discrete_interval_custom_cmp left_open(const DomainT& lo, const DomainT& up)
		{
			return discrete_interval_custom_cmp(lo, up, interval_bounds::left_open());
		}
		static discrete_interval_custom_cmp closed(const DomainT& lo, const DomainT& up)
		{
			return discrete_interval_custom_cmp(lo, up, interval_bounds::closed());
		}

	private:
		domain_type     _lwb;
		domain_type     _upb;
		interval_bounds _bounds;
	};

	//==============================================================================
	//=T discrete_interval_custom_cmp -> concept intervals
	//==============================================================================
	template<class DomainT, ICL_COMPARE Compare>
	struct interval_traits< icl::discrete_interval_custom_cmp<DomainT, Compare> >
	{
		typedef interval_traits type;
		typedef DomainT domain_type;
		typedef ICL_COMPARE_DOMAIN(Compare, DomainT) domain_compare;
		typedef icl::discrete_interval_custom_cmp<DomainT, Compare> interval_type;

		static interval_type construct(const domain_type& lo, const domain_type& up)
		{
			return interval_type(lo, up);
		}

		static domain_type lower(const interval_type& inter_val)
		{
			return inter_val.lower();
		};
		static domain_type upper(const interval_type& inter_val)
		{
			return inter_val.upper();
		};
	};

	//==============================================================================
	//=T discrete_interval_custom_cmp -> concept dynamic_interval_traits
	//==============================================================================
	template<class DomainT, ICL_COMPARE Compare>
	struct dynamic_interval_traits<boost::icl::discrete_interval_custom_cmp<DomainT, Compare> >
	{
		typedef dynamic_interval_traits type;
		typedef boost::icl::discrete_interval_custom_cmp<DomainT, Compare> interval_type;
		typedef DomainT domain_type;
		typedef ICL_COMPARE_DOMAIN(Compare, DomainT) domain_compare;

		static interval_type construct(const domain_type& lo, const domain_type& up, interval_bounds bounds)
		{
			return interval_type(lo, up, bounds, static_cast<interval_type*>(0));
		}

		static interval_type construct_bounded(const bounded_value<DomainT>& lo,
											   const bounded_value<DomainT>& up)
		{
			return  interval_type
			(
				lo.value(), up.value(),
				lo.bound().left() | up.bound().right(),
				static_cast<interval_type*>(0)
			);
		}
	};

	//==============================================================================
	//= Type traits
	//==============================================================================
	template <class DomainT, ICL_COMPARE Compare>
	struct interval_bound_type< discrete_interval_custom_cmp<DomainT, Compare> >
	{
		typedef interval_bound_type type;
		BOOST_STATIC_CONSTANT(bound_type, value = interval_bounds::dynamic);
	};

	template <class DomainT, ICL_COMPARE Compare>
	struct is_discrete_interval<discrete_interval_custom_cmp<DomainT, Compare> >
	{
		typedef is_discrete_interval<discrete_interval_custom_cmp<DomainT, Compare> > type;
		BOOST_STATIC_CONSTANT(bool, value = is_discrete<DomainT>::value);
	};

	template <class DomainT, ICL_COMPARE Compare>
	struct type_to_string<icl::discrete_interval_custom_cmp<DomainT, Compare> >
	{
		static std::string apply()
		{
			return "dI<" + type_to_string<DomainT>::apply() + ">";
		}
	};

	template<class DomainT>
	struct value_size<icl::discrete_interval_custom_cmp<DomainT> >
	{
		static std::size_t apply(const icl::discrete_interval_custom_cmp<DomainT>&)
		{
			return 2;
		}
	};
}} // namespace icl boost

template <typename Vector, template <typename> class Compare>
using vector_interval = boost::icl::continuous_interval_custom_cmp<Vector, Compare>;

TEST(interval_set_test_suit, vector_interval_construct)
{
	using namespace geometrix;
	using vector2 = vector_double_2d;
	using point2 = point_double_2d;
	std::vector<point2> points;
	std::vector<vector2> vectors;
	std::vector<double> pangles;

	auto p = point2{ 0., 0. };
	auto v = vector2{ 1.0, 0.0 };
	
	using namespace boost::icl;
	using iset = boost::icl::interval_set<vector2, v_compare, vector_interval<vector2, v_compare>>;
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

	EXPECT_TRUE(boost::icl::contains(sut, interval_t::closed(vector2{ 1,1 }, vector2{ 1,1 })));
	EXPECT_FALSE(boost::icl::contains(sut, interval_t::closed(vector2{ 0,1 }, vector2{ 0,1 })));
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

#include <geometrix/algorithm/intersection/circle_circle_intersection.hpp>
std::pair<point2, point2> calculate_tangent_points(const circle2& c, const point2& p)
{
	auto cp = vector2{ c.get_center() - p };

	auto cOP = circle2{ p + 0.5 * cp, 0.5 * magnitude(cp) };
	auto r = circle_circle_intersection(c, cOP, direct_comparison_policy{});
	return std::make_pair(*r.IntersectionPoint0, *r.IntersectionPoint1);
}

struct velocity_obstacle
{
	velocity_obstacle(const circle2& a, const velocity2& va, const circle2& b, const velocity2& vb)
	{
		auto Rb = a.get_radius() + b.get_radius();
		auto ab = vector2{ b.get_center() - a.get_center() };

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

#include <geometrix/numeric/interval.hpp>

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
