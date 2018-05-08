#ifndef STK_CONTAINER_MULTI_INDEX_CONTAINER_DETAIL_INDIRECT_COMPARE_HPP
#define STK_CONTAINER_MULTI_INDEX_CONTAINER_DETAIL_INDIRECT_COMPARE_HPP
#pragma once

namespace stk { namespace detail {
        template <typename Container, typename Compare>
        struct indirect_compare_adaptor
        {
            indirect_compare_adaptor(Container& c, const Compare& cmp)
                : c(c)
                , cmp(cmp)
            {}

            bool operator()( std::size_t lhs, std::size_t rhs ) const
            {
                return cmp(c[lhs], c[rhs]);
            }

            Container& c;
            Compare cmp;
        };

        template <typename Container, typename Value, typename Compare>
        struct indirect_compare_value_adaptor 
        {
            indirect_compare_value_adaptor(Container& c, const Value& v, const Compare& cmp)
                : c(c)
                , v(v)
                , cmp(cmp)
            {}

            bool operator()( std::size_t lhs, std::size_t rhs ) const
            {
                if( lhs == (std::numeric_limits<std::size_t>::max)())
                    return cmp(v, c[rhs]);
                else if( rhs == (std::numeric_limits<std::size_t>::max)())
                    return cmp(c[lhs], v);
                else
                    return cmp(c[lhs], c[rhs]);
            }

            Container&   c;
            Compare      cmp;
            Value const& v;
        };
}}//! namespace stk::detail;

#endif // STK_CONTAINER_MULTI_INDEX_CONTAINER_DETAIL_INDIRECT_COMPARE_HPP

