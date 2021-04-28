//
//! Copyright © 2021
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/new_allocator.hpp>

namespace stk {

    template <typename Key, typename Value, std::size_t N, typename Compare = std::less<Key>, typename Allocator = boost::container::new_allocator<std::pair<Key,Value>>>
    using small_flat_map = boost::container::flat_map<Key, Value, Compare, boost::container::small_vector<std::pair<Key, Value>, N, Allocator>>;  
    
    template <typename Key, typename Value, std::size_t N, typename Compare = std::less<Key>, typename Allocator = boost::container::new_allocator<std::pair<Key,Value>>>
    using small_flat_multimap = boost::container::flat_multimap<Key, Value, Compare, boost::container::small_vector<std::pair<Key, Value>, N, Allocator>>;  

}//! namespace stk;

