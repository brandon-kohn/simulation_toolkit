#pragma once

#include <geometrix/primitive/point.hpp>
#include <geometrix/tensor/vector.hpp>
#include <geometrix/primitive/segment.hpp>
#include <geometrix/primitive/polygon.hpp>
#include <geometrix/primitive/polyline.hpp>
#include <geometrix/primitive/axis_aligned_bounding_box.hpp>
#include <boost/serialization/vector.hpp>//! Should cover polygon/polyline.
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_free.hpp>

#define STK_BOOST_CLASS_VERSION_TPL(T, N)                              \
namespace boost::serialization {                                       \
template<typename ... Ps>                                              \
struct version< T<Ps...> >                                             \
{                                                                      \
    typedef mpl::int_<N> type;                                         \
    typedef mpl::integral_c_tag tag;                                   \
    BOOST_STATIC_CONSTANT(int, value = version::type::value);          \
    static_assert( N < 256, "version is 8 bits." );                    \
};                                                                     \
}                                                                      \
///

STK_BOOST_CLASS_VERSION_TPL( geometrix::point, 1 );
STK_BOOST_CLASS_VERSION_TPL( geometrix::vector, 1 );
STK_BOOST_CLASS_VERSION_TPL( geometrix::segment, 1 );
STK_BOOST_CLASS_VERSION_TPL( geometrix::polygon, 1 );
STK_BOOST_CLASS_VERSION_TPL( geometrix::polyline, 1 );
STK_BOOST_CLASS_VERSION_TPL( geometrix::axis_aligned_bounding_box, 1 );

namespace boost::serialization {

	namespace detail {
		template <std::size_t i, std::size_t N, typename Fn, typename ... Args>
		inline void for_dimension(Fn&& fn)
		{
			fn( i );
			if constexpr( i+1 < N ) 
			{
				for_dimension<i + 1, N>( std::forward<Fn>( fn ) );
			}
		}
	}

	template<class Archive, typename CoordinateType, std::size_t Dim>
	inline void serialize(Archive& ar, geometrix::point<CoordinateType, Dim>& p, const unsigned int version)
	{
		::boost::serialization::detail::for_dimension<0, Dim>(
			[&]( std::size_t i )
			{
				ar & p[i];
			} );
	}
	
	template<class Archive, typename CoordinateType, std::size_t Dim>
	inline void serialize(Archive& ar, geometrix::vector<CoordinateType, Dim>& v, const unsigned int version)
	{
		::boost::serialization::detail::for_dimension<0, Dim>(
			[&]( std::size_t i )
			{
				ar & v[i];
			} );
	}
	
	template<class Archive, typename Point>
	inline void save(Archive & ar, const geometrix::segment<Point>& s, unsigned int version)
	{
		ar & s.get_start();
		ar & s.get_end();
	}

	template<class Archive, typename Point>
	inline void load(Archive & ar, geometrix::segment<Point>& s, unsigned int version)
	{
		Point p0, p1;
		ar &  p0;
		ar &  p1;
		s.set_start( p0 );
		s.set_end( p1 );
	}

	template<class Archive, typename Point>
	inline void serialize(Archive & ar, geometrix::segment<Point>& s, const unsigned int version)
	{
		split_free( ar, s, version );
	}
	
	template<class Archive, typename Point>
	inline void serialize(Archive & ar, geometrix::polyline<Point>& pline, const unsigned int version)
	{
		ar & (std::vector<Point>&)pline;
	}

	template<class Archive, typename Point>
	inline void serialize(Archive & ar, geometrix::polygon<Point>& pgon, const unsigned int version)
	{
		ar & (std::vector<Point>&)pgon;
	}
	
	template<class Archive, typename Point>
	inline void save(Archive & ar, const geometrix::axis_aligned_bounding_box<Point>& b, unsigned int version)
	{
		ar & b.get_lower_bound();
		ar & b.get_upper_bound();
	}

	template<class Archive, typename Point>
	inline void load(Archive & ar, geometrix::axis_aligned_bounding_box<Point>& b, unsigned int version)
	{
		Point p0, p1;
		ar &  p0;
		ar &  p1;
		b = geometrix::axis_aligned_bounding_box<Point>{ p0, p1 };
	}

	template<class Archive, typename Point>
	inline void serialize(Archive & ar, geometrix::axis_aligned_bounding_box<Point>& b, const unsigned int version)
	{
		split_free( ar, b, version );
	}
	
}//! namespace boost::serialization;