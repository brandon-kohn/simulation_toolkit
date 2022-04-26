//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_BOOST_UNITS_HPP
#define STK_BOOST_UNITS_HPP

#if defined(_MSC_VER)
    #pragma once
#endif

#include <boost/units/quantity.hpp>
#include <boost/units/pow.hpp>
#include <boost/units/systems/si.hpp>
#include <boost/units/systems/si/prefixes.hpp>
#include <boost/units/make_scaled_unit.hpp>
#include <boost/units/io.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <iterator>

namespace stk { 

namespace units {
    using namespace boost::units;

    template <typename T>
    using inverse = power_typeof_helper<T, static_rational<-1>>;

    using dimensionless = quantity<si::dimensionless, double>;
    using time = quantity<si::time, double>;    
    using length = quantity<si::length, double>;    
    using area = quantity<si::area, double>;
    using volume = quantity<si::volume, double>;
    using area_squared = decltype(area()*area());
    using mass = quantity<si::mass, double>;
    using angle = quantity<si::plane_angle, double>;
    using speed = quantity<si::velocity, double>;
    using kinematic_viscosity = quantity<si::kinematic_viscosity, double>;//! m^2/s ... some motion calcs have this term.
    using frequency = quantity<si::frequency, double>;
    using spatial_frequency = quantity<si::wavenumber, double>; 
    using angular_velocity = quantity<si::angular_velocity, double>;
    using acceleration = quantity<si::acceleration, double>;
    using force = quantity<si::force, double>;
    using momentum = quantity<si::momentum, double>;

    using curvature = spatial_frequency;
    using curvature_squared = decltype(curvature() * curvature());
    using curvature_cubed = decltype(curvature() * curvature_squared());
    using boost::units::si::seconds;
    using boost::units::si::meters;
    using boost::units::si::meters_per_second;
    using boost::units::si::meters_per_second_squared;
    using boost::units::si::square_meters;
    using boost::units::si::square_meter;

    template <typename T>
    inline T get_underlying_value(T v)
    {
        return v;
    }

    template <typename Unit, typename T>
    inline T get_underlying_value(boost::units::quantity<Unit, T> v)
    {
        return v.value();
    }
    
    template <typename X, typename Unit>
    inline decltype(std::declval<X>() * std::declval<Unit>()) apply(const X& x, Unit u) { return x * u; }

    template <typename Unit>
    struct unit_applier
    {
        using unit_type = Unit;

        template <typename Sig>
        struct result;

        template <typename This, typename X>
        struct result<This(X)>
        {
            using type = decltype(std::declval<X>() * std::declval<unit_type>());
        };

        template <typename X>
        typename result<unit_applier(X)>::type operator()(const X& x) const
        {
            return x * unit_type();
        }
    };

    template <typename Unit>
    inline unit_applier<Unit> make_unit_applier(Unit) { return unit_applier<Unit>(); }
}//! namespace units;

}//! namespace stk;

#endif//STK_BOOST_UNITS_HPP
