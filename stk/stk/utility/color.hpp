//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/config.hpp>
#include <geometrix/tensor/vector.hpp>
#include <geometrix/primitive/point.hpp>
#include <geometrix/utility/member_function_fusion_adaptor/adapt_member_function.hpp>
#include <geometrix/tensor/fusion_vector_access_policy.hpp>
#include <geometrix/space/neutral_reference_frame.hpp>
#include <cstdint>
#include <array>

namespace stk {

    //! Type to represent a 24-bit color with an alpha channel in rgba format.
    union color_rgba
    {
        BOOST_CONSTEXPR color_rgba() 
            : bits{}
        {}

        BOOST_CONSTEXPR color_rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
            : red{r}
            , green{g}
            , blue{b}
            , alpha{a}
        {}

        template <typename ColorExpr>
        BOOST_CONSTEXPR color_rgba(ColorExpr const& p)
            : red{geometrix::get<0>(p)}
            , green{geometrix::get<1>(p)}
            , blue{geometrix::get<2>(p)}
            , alpha{geometrix::get<3>(p)}
        {}

        BOOST_CONSTEXPR bool operator ==(const color_rgba& rhs) const BOOST_NOEXCEPT
        {
            return bits == rhs.bits;
        }

        BOOST_CONSTEXPR bool operator !=(const color_rgba& rhs) const BOOST_NOEXCEPT
        {
            return bits != rhs.bits;
        }

        BOOST_CONSTEXPR bool operator <(const color_rgba& rhs) const BOOST_NOEXCEPT
        {
            return bits < rhs.bits;
        }

        BOOST_CONSTEXPR std::uint8_t operator[](std::size_t index) const BOOST_NOEXCEPT
        {
            return this->array[index];
        }

        std::uint8_t& operator[](std::size_t index) BOOST_NOEXCEPT
        {
            return this->array[index];
        }

        std::array<std::uint8_t, 4> array;
        struct
        {
            std::uint8_t red;
            std::uint8_t green;
            std::uint8_t blue;
            std::uint8_t alpha;
        };

    private:

        std::uint32_t bits;
    };

    static_assert(sizeof(color_rgba)==4, "color_rgba should be packed in 4 bytes.");

    template <typename ...Args>
    inline BOOST_CONSTEXPR color_rgba make_color(Args... a)
    {
        return color_rgba(a...);
    }

    using color_vector = geometrix::vector<std::int32_t,4>;
    
}//namespace stk;

GEOMETRIX_INDEX_OPERATOR_FUSION_SEQUENCE(stk::color_rgba, std::uint8_t, 4);

namespace geometrix {
    template <>
    struct construction_policy<stk::color_rgba>
    {    
        template <typename... Args>
        static stk::color_rgba construct(Args... a)
        {
            return stk::color_rgba{a...};
        }
    };
}//namespace geometrix;

GEOMETRIX_DEFINE_POINT_TRAITS(stk::color_rgba, (std::uint8_t), 4, std::uint8_t, double, neutral_reference_frame<4>, fusion_vector_access_policy<stk::color_rgba>);
