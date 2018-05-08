#ifndef STK_CONTAINER_MULTI_INDEX_CONTAINER_DETAIL_HAS_TAG_HPP
#define STK_CONTAINER_MULTI_INDEX_CONTAINER_DETAIL_HAS_TAG_HPP
#pragma once

#include <boost/mpl/contains.hpp>

namespace stk { namespace detail {
        template<typename Tag>
        struct has_tag 
        {
            template<typename Index>
            struct apply
                : boost::mpl::contains<BOOST_DEDUCED_TYPENAME Index::tag_list,Tag>
            {}; 
        };
    }

}}//! namespace stk::detail;

#endif // STK_CONTAINER_MULTI_INDEX_CONTAINER_DETAIL_HAS_TAG_HPP
