//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_GEOMETRY_APPLY_UNIT_HPP
#define STK_GEOMETRY_APPLY_UNIT_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <stk/units/boost_units.hpp>

#include <geometrix/space/dimension.hpp>
#include <geometrix/primitive/point_sequence_traits.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace stk {
    
namespace detail {

    template <typename Point, typename Unit>
    inline Point apply_unit(std::initializer_list<double> p, Unit unit, geometrix::dimension<2>)
    {
        return Point{ units::apply((&*p.begin())[0], unit), units::apply((&*p.begin())[1], unit) };
    }

    template <typename Point, typename Unit>
    inline Point apply_unit(std::initializer_list<double> p, Unit unit, geometrix::dimension<3>)
    {
        return Point{ units::apply((&*p.begin())[0], unit), units::apply((&*p.begin())[1], unit), units::apply((&*p.begin())[2], unit) };
    }

    template <typename PointSequence, typename Unit>
    inline PointSequence apply_unit(std::initializer_list<std::initializer_list<double>> ilist, Unit unit, geometrix::dimension<2>)
    {
        using boost::adaptors::transformed;

        using point_t = typename geometrix::point_sequence_traits<PointSequence>::point_type;

        PointSequence test;
        boost::copy(ilist | transformed([&unit](std::initializer_list<double> p) -> point_t { return{ units::apply((&*p.begin())[0], unit), units::apply((&*p.begin())[1], unit) }; }), std::back_inserter(test));

        return test;
    }

    template <typename PointSequence, typename Unit>
    inline PointSequence apply_unit(std::initializer_list<std::initializer_list<double>> ilist, Unit unit, geometrix::dimension<3>)
    {
        using boost::adaptors::transformed;

        using point_t = typename geometrix::point_sequence_traits<PointSequence>::point_type;

        PointSequence test;
        boost::copy(ilist | transformed([&unit](std::initializer_list<double> p) -> point_t { return{ units::apply((&*p.begin())[0], unit), units::apply((&*p.begin())[1], unit), units::apply((&*p.begin())[2], unit) }; }), std::back_inserter(test));

        return test;
    }
}//! namespace detail;

template <typename Point, typename Unit>
inline Point apply_unit(std::initializer_list<double> ilist, Unit unit)
{
    return ::stk::detail::apply_unit<Point>(ilist, unit, typename geometrix::dimension_of<Point>::type());
}

template <typename PointSequence, typename Unit>
inline PointSequence apply_unit(std::initializer_list<std::initializer_list<double>> ilist, Unit unit)
{
    using point_t = typename geometrix::point_sequence_traits<PointSequence>::point_type;
    return ::stk::detail::apply_unit<PointSequence>(ilist, unit, typename geometrix::dimension_of<point_t>::type());
}

}//! namespace stk;

#endif//STK_GEOMETRY_APPLY_UNIT_HPP
