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

#if defined(_MSC_VER)
    #pragma once
#endif

#include "point.hpp"

//! Polygon/Polyline type
#include <geometrix/primitive/polygon.hpp>

using polygon2 = geometrix::polygon<point2>;
using polygon3 = geometrix::polygon<point3>;

#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/value_type.hpp>

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
        boost::copy(polygon | transformed(to_point2), std::back_inserter( poly ));
    else
        boost::copy(polygon | reversed | transformed(to_point2), std::back_inserter( poly ));
    
    return clean_polygon(poly, make_tolerance_policy());
}

template <typename PolygonRange>
inline std::vector<polygon2> make_polygons( const PolygonRange& range, geometrix::polygon_winding winding )
{
    using namespace boost::adaptors;
    std::vector<polygon2> polygons;
    
    using polygon_t = typename boost::range_value<PolygonRange>::type;
    
    auto to_polygon2 = [winding](const polygon_t& poly) { return make_polygon(poly, winding); };
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

    auto translate = [translation]( const typename access::point_type& p ){ return construct<point_t>( p + translation ); };
    boost::copy(polygon | transformed(translate), std::back_inserter( poly ));

    return poly;
}

template <typename Result, typename PolygonRange, typename Vector>
inline std::vector<Result> translate_polygons( const PolygonRange& range, const Vector& translation )
{
    std::vector<Result> polygons;
    for( const auto& p : range )
        polygons.emplace_back(translate_polygon<Result>(p, translation));

    return polygons;
}

#endif//STK_POLYGON_HPP
