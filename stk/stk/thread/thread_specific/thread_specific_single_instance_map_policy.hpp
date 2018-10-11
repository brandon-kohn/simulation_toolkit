//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/container/type_storage.hpp>

namespace stk { namespace thread {
	
    template <typename Value>
    struct thread_specific_single_instance_map_policy
    {
        template <typename Key>
        struct map_type_generator
        {
			using type = std::pair<Key, type_storage<Value>>;
        };

        template <typename Map>
        static void initialize(Map& m)
        {}

        template <typename Map, typename Key>
        static Value* find(Map& m, const Key& k)
        {
            auto& v = m.second;
            if(v.is_initialized())
                return &*v;
            else
                return nullptr;
        }

        template <typename Map, typename Key>
        static Value* insert(Map& m, const Key& k, Value&& v)
        {
            GEOMETRIX_ASSERT(m.first == k || (m.first == nullptr && !m.second.is_initialized()));
			m.first = k;
			m.second = std::forward<Value>(v);
			return &(*m.second);
        }

        template <typename Map, typename Key>
        static void erase(Map& m, const Key& k)
        {
            if(m.second.is_initialized())
            {
				GEOMETRIX_ASSERT(m.first == k);
                m.second.destroy();
            }
        }

        template <typename Map>
        static bool empty(Map& m)
        {
			return !m.second.is_initialized();
        }

        template <typename Map, typename Visitor>
        static void for_each(Map& m, Visitor&& visitor)
        {
            if(!empty(m))
                visitor(m.first, *m.second);
        }
    };

}}//! namespace stk::thread;

