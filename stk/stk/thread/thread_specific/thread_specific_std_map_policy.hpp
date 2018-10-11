//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <map>

namespace stk { namespace thread {

    template <typename Value>
    struct thread_specific_std_map_policy
    {
        template <typename Key>
        struct map_type_generator
        {
            using type = std::map<Key, Value>;
        };

        template <typename Map>
        static void initialize(Map& m)
        {}

        template <typename Map, typename Key>
        static Value* find(Map& m, const Key& k)
        {
            auto it = m.find(k);
            return it != m.end() ? &it->second : nullptr;
        }

        template <typename Map, typename Key>
        static Value* insert(Map& m, const Key& k, Value&& v)
        {
            return &m.insert(std::make_pair(k, std::forward<Value>(v))).first->second;
        }

        template <typename Map, typename Key>
        static void erase(Map& m, const Key& k)
        {
            m.erase(k);
        }

        template <typename Map>
        static bool empty(Map& m)
        {
            return m.empty();
        }

        template <typename Map, typename Visitor>
        static void for_each(Map& m, Visitor&& visitor)
        {
            for (auto& i : m)
                visitor(i.first, i.second);
        }
    };

}}//! namespace stk::thread;

