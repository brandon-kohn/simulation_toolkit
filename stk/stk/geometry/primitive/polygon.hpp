//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_POLYGON_HPP
#define STK_POLYGON_HPP
#pragma once

#include "point.hpp"

//! Polygon/Polyline type
#include <geometrix/primitive/polygon.hpp>

using polygon2 = geometrix::polygon<point2>;
using polygon3 = geometrix::polygon<point3>;

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

template <typename Polygon>
inline polygon2 make_polygon( const Polygon& polygon, geometrix::polygon_winding winding )
{
    using namespace geometrix;
    using namespace boost::adaptors;
    BOOST_CONCEPT_ASSERT( (PointSequenceConcept<Polygon>) );
    using access = geometrix::point_sequence_traits<Polygon>;
    polygon2 poly;
    auto to_point2 = [](const typename access::point_type& p) { return point2{ p }; };
    auto signedArea = get_signed_area( polygon );
    if( (signedArea.value() >= 0. && winding == polygon_winding::counterclockwise) || (signedArea.value() <= 0. && winding == polygon_winding::clockwise) )
        boost::copy(polygon | tranformed(to_point2), std::back_inserter( poly ));
    else
        boost::copy(polygon | reversed | tranformed(to_point2), std::back_inserter( poly ));
    
    return clean_polygon(poly, make_tolerance_policy());
}

template <typename PolygonRange>
inline std::vector<polygon2> make_polygons( const PolygonRange& range, geometrix::polygon_winding winding )
{
    using namespace boost::adaptors;
    std::vector<polygon2> polygons;
    
    using polygon_t = boost::value_type<PolygonRange>::type;
    
    auto to_polygon2 = [&winding](const polygon_t& poly) { return make_polygon(poly, winding); };
    boost::copy(range | transformed(to_polygon2), std::back_inserter(polygons));
    
    return polygons;
}

//! Given another format polygon, make a polygon2.
template <typename Result, typename Polygon, typename Vector>
inline Result translate_polygon( const Polygon& polygon, const Vector& translation )
{
    using namespace geometrix;
    using namespace boost::adaptors;
    BOOST_CONCEPT_ASSERT( (PointSequenceConcept<Polygon>) );
    using access = point_sequence_traits<Polygon>;
    using point_t = typename point_sequence_traits<Result>::point_type;
    Result poly;

    auto translate = [&translation]( const typename access::point_type& p ){ return construct<point_t>( p + translation ); };
    boost::copy(polygon | tranformed(translate), std::back_inserter( poly ));

    return poly;
}

template <typename Result, typename Polyline, typename Vector>
inline Result translate_polyline(const Polyline& polyline, const Vector& translation)
{
    using namespace geometrix;
    using namespace boost::adaptors;
    BOOST_CONCEPT_ASSERT((PointSequenceConcept<Polyline>));
    using access = geometrix::point_sequence_traits<Polyline>;
    Result poly;

    auto translate = [&translation](const typename access::point_type& p) { return geometrix::construct<point2>(p + translation); };
    boost::copy(polyline | transformed(translate), std::back_inserter(poly));

    return poly;
}

template <typename Result, typename PolygonRange, typename Vector>
inline std::vector<Result> translate_polygons( const PolygonRange& range, const Vector& translation )
{
    using namespace boost::adaptors;
    using polygon_t = boost::value_type<PolygonRange>::type;
    
    std::vector<Result> polygons;
    auto translate = [&translation](const polygon_t& p) { return translate_polygon(p, translation); }
    boost::copy(polygons | transformed(translate), std::back_inserter(polygons));

    return polygons;
}

inline rectangle2 generateRectangle(const segment2& seg, units::length offset)
{
    using namespace geometrix;
    vector2 parallel = offset * normalize(seg.get_end() - seg.get_start());
    vector2 left_perp = left_normal(parallel);
    vector2 right_perp = right_normal(parallel);
    return std::array<point2, 4>{{
            point2{ seg.get_start() + (right_perp - parallel) }
          , point2{ seg.get_end() + (right_perp + parallel) }
          , point2{ seg.get_end() + (left_perp + parallel) }
          , point2{ seg.get_start() + (left_perp - parallel) }
        }};
}

inline polygon2 generateSquare(const point2& center, const units::length& width)
{
    auto halfwidth = 0.5 * width;
    auto xmax = get<0>(center) + halfwidth;
    auto ymax = get<1>(center) + halfwidth;
    auto xmin = get<0>(center) - halfwidth;
    auto ymin = get<1>(center) - halfwidth;

    return{ { xmin, ymin },{ xmax, ymin },{ xmax, ymax },{ xmin, ymax } };
}

//! Stream operators for geometry.
namespace geometrix {

    std::ostream& operator << (std::ostream& os, const polygon2& p);

}//namespace geometrix.

#endif//STK_POLYGON_HPP
