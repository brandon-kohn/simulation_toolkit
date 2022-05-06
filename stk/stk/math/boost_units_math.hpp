// Boost.Units - A C++ library for zero-overhead dimensional analysis and 
// unit/quantity manipulation and conversion
//
// Copyright (C) 2003-2008 Matthias Christian Schabel
// Copyright (C) 2007-2008 Steven Watanabe
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#pragma once

#include <boost/units/cmath.hpp>
#include <stk/math/math.hpp>

namespace stk {

    /*
    /// For non-dimensionless quantities, integral and rational powers 
    /// and roots can be computed by @c pow<Ex> and @c root<Rt> respectively.
    template<class S, class Y>
    inline BOOST_CONSTEXPR
    boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>
    pow(const boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>& q1,
        const boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>& q2)
    {
        typedef boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S),Y> quantity_type;
        return quantity_type::from_value(stk::pow(q1.value(), q2.value()));
    }
    */

    template<class S, class Y>
    inline BOOST_CONSTEXPR
    boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>
    exp(const boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>& q)
    {
        typedef boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y> quantity_type;
        return quantity_type::from_value(stk::exp(q.value()));
    }

    template<class S, class Y>
    inline BOOST_CONSTEXPR
    boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>
    log(const boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>& q)
    {
        typedef boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y> quantity_type;
        return quantity_type::from_value(stk::log(q.value()));
    }

    template<class S, class Y>
    inline BOOST_CONSTEXPR
    boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>
    log10(const boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y>& q)
    {
        typedef boost::units::quantity<BOOST_UNITS_DIMENSIONLESS_UNIT(S), Y> quantity_type;
        return quantity_type::from_value(stk::log10(q.value()));
    }

    /*
    template<class Unit,class Y>
    inline BOOST_CONSTEXPR
    typename boost::units::root_typeof_helper<
                                boost::units::quantity<Unit,Y>,
                                boost::units::static_rational<2>
                               >::type
    sqrt(const boost::units::quantity<Unit,Y>& q)
    {
        using namespace boost::units;
        typedef typename root_typeof_helper<
                                            boost::units::quantity<Unit,Y>,
                                            static_rational<2>
                                           >::type quantity_type;

        return quantity_type::from_value(stk::sqrt(q.value()));
    }
    */

    /// cos of theta in radians
    template<class Y>
    inline BOOST_CONSTEXPR
    typename boost::units::dimensionless_quantity<boost::units::si::system,Y>::type 
    cos(const boost::units::quantity<boost::units::si::plane_angle,Y>& theta)
    {
        return stk::cos(theta.value());
    }

    /// sin of theta in radians
    template<class Y>
    inline BOOST_CONSTEXPR
    typename boost::units::dimensionless_quantity<boost::units::si::system,Y>::type 
    sin(const boost::units::quantity<boost::units::si::plane_angle,Y>& theta)
    {
        return stk::sin(theta.value());
    }

    /// tan of theta in radians
    template<class Y>
    inline BOOST_CONSTEXPR
    typename boost::units::dimensionless_quantity<boost::units::si::system,Y>::type 
    tan(const boost::units::quantity<boost::units::si::plane_angle,Y>& theta)
    {
        return stk::tan(theta.value());
    }

    /// cos of theta in other angular units 
    template<class System,class Y>
    inline BOOST_CONSTEXPR
    typename boost::units::dimensionless_quantity<System,Y>::type 
    cos(const boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension,System>,Y>& theta)
    {
        return stk::cos(boost::units::quantity<boost::units::si::plane_angle,Y>(theta));
    }

    /// sin of theta in other angular units 
    template<class System,class Y>
    inline BOOST_CONSTEXPR
    typename boost::units::dimensionless_quantity<System,Y>::type 
    sin(const boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension,System>,Y>& theta)
    {
        return stk::sin(boost::units::quantity<boost::units::si::plane_angle,Y>(theta));
    }

    /// tan of theta in other angular units 
    template<class System,class Y>
    inline BOOST_CONSTEXPR
    typename boost::units::dimensionless_quantity<System,Y>::type 
    tan(const boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension,System>,Y>& theta)
    {
        return stk::tan(boost::units::quantity<boost::units::si::plane_angle,Y>(theta));
    }

    /// acos of dimensionless boost::units::quantity returning angle in same system
    template<class Y,class System>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >,Y>
    acos(const boost::units::quantity<boost::units::unit<boost::units::dimensionless_type, boost::units::homogeneous_system<System> >,Y>& val)
    {
        return boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >,Y>(stk::acos(val.value())*boost::units::si::radians);
    }

    /// acos of dimensionless boost::units::quantity returning angle in radians
    template<class Y>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>
    acos(const boost::units::quantity<boost::units::unit<boost::units::dimensionless_type, boost::units::heterogeneous_dimensionless_system>,Y>& val)
    {
        return boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>::from_value(stk::acos(val.value()));
    }

    /// asin of dimensionless boost::units::quantity returning angle in same system
    template<class Y,class System>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >,Y>
    asin(const boost::units::quantity<boost::units::unit<boost::units::dimensionless_type, boost::units::homogeneous_system<System> >,Y>& val)
    {
        return boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >,Y>(stk::asin(val.value())*boost::units::si::radians);
    }

    /// asin of dimensionless boost::units::quantity returning angle in radians
    template<class Y>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>
    asin(const boost::units::quantity<boost::units::unit<boost::units::dimensionless_type, boost::units::heterogeneous_dimensionless_system>,Y>& val)
    {
        return boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>::from_value(stk::asin(val.value()));
    }

    /// atan of dimensionless boost::units::quantity returning angle in same system
    template<class Y,class System>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >,Y>
    atan(const boost::units::quantity<boost::units::unit<boost::units::dimensionless_type, boost::units::homogeneous_system<System> >, Y>& val)
    {
        return boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >,Y>(stk::atan(val.value())*boost::units::si::radians);
    }

    /// atan of dimensionless boost::units::quantity returning angle in radians
    template<class Y>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>
    atan(const boost::units::quantity<boost::units::unit<boost::units::dimensionless_type, boost::units::heterogeneous_dimensionless_system>, Y>& val)
    {
        return boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>::from_value(stk::atan(val.value()));
    }

    /// atan2 of @c value_type returning angle in radians
    template<class Y, class Dimension, class System>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >, Y>
    atan2(const boost::units::quantity<boost::units::unit<Dimension, boost::units::homogeneous_system<System> >, Y>& y,
          const boost::units::quantity<boost::units::unit<Dimension, boost::units::homogeneous_system<System> >, Y>& x)
    {
        return boost::units::quantity<boost::units::unit<boost::units::plane_angle_dimension, boost::units::homogeneous_system<System> >, Y>(stk::atan2(y.value(),x.value())*boost::units::si::radians);
    }

    /// atan2 of @c value_type returning angle in radians
    template<class Y, class Dimension, class System>
    inline BOOST_CONSTEXPR
    boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>
    atan2(const boost::units::quantity<boost::units::unit<Dimension, boost::units::heterogeneous_system<System> >, Y>& y,
          const boost::units::quantity<boost::units::unit<Dimension, boost::units::heterogeneous_system<System> >, Y>& x)
    {
        return boost::units::quantity<boost::units::angle::radian_base_unit::unit_type,Y>::from_value(stk::atan2(y.value(),x.value()));
    }

}//! namespace stk; 

