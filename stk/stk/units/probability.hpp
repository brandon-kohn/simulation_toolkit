//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_PROBABILITY_HPP
#define STK_PROBABILITY_HPP
#pragma once

#include <boost/units/quantity.hpp>
#include <boost/units/pow.hpp>
#include <boost/units/systems/si.hpp>
#include <boost/units/systems/si/prefixes.hpp>
#include <boost/units/make_scaled_unit.hpp>

namespace probability_system {
    using namespace boost::units;

    struct probability_base_dimension : base_dimension<probability_base_dimension, 1> {};
    using probability_dimension = probability_base_dimension::dimension_type;

    struct probability_base_unit : base_unit<probability_base_unit, probability_dimension, 1>
    {};
    
    using system = make_system<probability_base_unit>::type;
    using probability_unit = unit<probability_dimension, system>;
    
    BOOST_UNITS_STATIC_CONSTANT(proportion, probability_unit);
    BOOST_UNITS_STATIC_CONSTANT(proportions, probability_unit);
    BOOST_UNITS_STATIC_CONSTANT(fraction, probability_unit);
    BOOST_UNITS_STATIC_CONSTANT(fractions, probability_unit);
    BOOST_UNITS_STATIC_CONSTANT(parts_per_unit, probability_unit);
    
    //! Percent
    using percent_base_unit = scaled_base_unit<probability_base_unit, scale<10, static_rational<-2>>>;
    using percent_system = make_system<percent_base_unit>::type;
    using percent_unit = unit<probability_dimension, percent_system>;
    
    BOOST_UNITS_STATIC_CONSTANT(percent, percent_unit); 
}//! namespace percent_units;

namespace boost {
    namespace units {
        template<> struct base_unit_info<probability_system::probability_base_unit>
        {
            static std::string name() { return "probability"; }
            static std::string symbol() { return "P"; }
        };

        template<> struct base_unit_info<probability_system::percent_base_unit>
        {
            static std::string name() { return "percent"; }
            static std::string symbol() { return "%"; }
        };

        template<typename System1, typename X, typename System2, typename Y>
        inline quantity<probability_system::probability_unit, decltype(std::declval<X>() + std::declval<Y>())>
            operator+(const quantity<unit<probability_system::probability_dimension, System1>, X>& lhs,
                const quantity<unit<probability_system::probability_dimension, System2>, Y>& rhs)
        {
            using type = quantity<probability_system::probability_unit, decltype(std::declval<X>() + std::declval<Y>())>;

            return type::from_value(lhs.value() + rhs.value());
        }

        template<typename System1, typename X, typename System2, typename Y>
        inline quantity<probability_system::probability_unit, decltype(std::declval<X>() - std::declval<Y>())>
            operator-(const quantity<unit<probability_system::probability_dimension, System1>, X>& lhs,
                const quantity<unit<probability_system::probability_dimension, System2>, Y>& rhs)
        {
            using type = quantity<probability_system::probability_unit, decltype(std::declval<X>() - std::declval<Y>())>;

            return type::from_value(lhs.value() - rhs.value());
        }

        template<typename System1, typename X, typename System2, typename Y>
        inline quantity<probability_system::probability_unit, decltype(std::declval<X>() * std::declval<Y>())>
            operator*(const quantity<unit<probability_system::probability_dimension, System1>, X>& lhs,
                const quantity<unit<probability_system::probability_dimension, System2>, Y>& rhs)
        {
            using type = quantity<probability_system::probability_unit, decltype(std::declval<X>() * std::declval<Y>())>;

            return type::from_value(lhs.value() * rhs.value());
        }

        template<typename System1, typename X, typename System2, typename Y>
        inline quantity<probability_system::probability_unit, decltype(std::declval<X>() / std::declval<Y>())>
            operator/(const quantity<unit<probability_system::probability_dimension, System1>, X>& lhs,
                const quantity<unit<probability_system::probability_dimension, System2>, Y>& rhs)
        {
            using type = quantity<probability_system::probability_unit, decltype(std::declval<X>() / std::declval<Y>())>;

            return type::from_value(lhs.value() / rhs.value());
        }
    }
}

namespace units {
    using namespace boost::units;
    using namespace probability_system;

    using probability = quantity<probability_unit, double>;
    using percentage = quantity<percent_unit, double>;
}//! namespace units;

#endif//STK_PROBABILITY_HPP
