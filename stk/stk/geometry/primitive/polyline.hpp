//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_POLYLINE_HPP
#define STK_POLYLINE_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include "point.hpp"
#include <geometrix/primitive/polyline.hpp>

using polyline2 = geometrix::polyline<point2>;
using polyline3 = geometrix::polyline<point3>;

#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/transformed.hpp>

template <typename Result, typename Polyline, typename Vector>
inline Result translate_polyline(const Polyline& pline, const Vector& translation)
{
    using namespace geometrix;
    using namespace boost::adaptors;
    BOOST_CONCEPT_ASSERT((PointSequenceConcept<Polyline>));
    using access = geometrix::point_sequence_traits<Polyline>;
    Result poly;

    auto translate = [translation](const typename access::point_type& p) { return geometrix::construct<point2>(p + translation); };
    boost::copy(pline | transformed(translate), std::back_inserter(poly));

    return poly;
}

#endif//STK_POLYLINE_HPP
