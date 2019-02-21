#pragma once

#include <boost/range/end.hpp>

namespace stk {
    template <typename Element, typename ContainerTraits, typename Iterator>
    inline bool map_lower_bound_contains(const Element& element, const ContainerTraits& cont, const Iterator& it)
    {
        return (it != boost::end(cont) && !(cont.key_comp()(element, it->first)));
    };

    template <typename Element, typename ContainerTraits, typename Iterator>
    inline bool set_lower_bound_contains(const Element& element, const ContainerTraits& cont, const Iterator& it)
    {
        return (it != boost::end(cont) && !(cont.key_comp()(element, *it)));
    };
}//! namespace stk;

