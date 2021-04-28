//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/new_allocator.hpp>

namespace stk {

    template <typename Key, std::size_t N, typename Compare = std::less<Key>, typename Allocator = boost::container::new_allocator<Key>>
    using small_flat_set = boost::container::flat_set<Key, Compare, boost::container::small_vector<Key, N, Allocator>>;
    
    template <typename Key, std::size_t N, typename Compare = std::less<Key>, typename Allocator = boost::container::new_allocator<Key>>
    using small_flat_multiset = boost::container::flat_multiset<Key, Compare, boost::container::small_vector<Key, N, Allocator>>;

}//! namespace stk;

