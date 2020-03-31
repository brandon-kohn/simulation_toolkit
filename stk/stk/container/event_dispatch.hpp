//
//! Copyright © 2020
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CONTAINER_EVENT_DISPATCH_HPP
#define STK_CONTAINER_EVENT_DISPATCH_HPP
#pragma once

#include <stk/thread/fixed_function.hpp>
#include <boost/container/flat_map.hpp>
#include <cstdint>
#include <functional>
#include <vector>

namespace stk {

    struct use_std_function{};
    struct use_fixed_function{};

    namespace detail{

        template <typename T, typename EnableIF=void>
        struct function_chooser{};

        template <>
        struct function_chooser<use_std_function>
        {
            template <typename Signature>
            struct apply
            {
                using type = std::function<Signature>;
            };
        };
        
        template <>
        struct function_chooser<use_fixed_function>
        {
            template <typename Signature>
            struct apply
            {
                using type = fixed_function<Signature>;
            };
        };

    }//! namespace detail;

    template <typename Signature, typename Key = std::uintptr_t, typename FunctionStorage = use_fixed_function>
    class event_dispatch
    {
    public:

        using function_storage = typename detail::function_chooser<FunctionStorage>::template apply<Signature>::type;

        template <typename ObserverKey, typename Fn>
        void add_listener(ObserverKey key, Fn&& fn)
        {
            static_assert(sizeof(ObserverKey) == sizeof(Key), "The ObserverKey must be c-style cast convertible to the Key type (i.e. ptr to uintptr_t.)");
            m_listeners.emplace(std::make_pair((Key)(key), std::forward<Fn>(fn)));
        }
        
        template <typename Key>
        void remove_listener(const Key& key)
        {
            m_listeners.erase(key);
        }
        
        template <typename ... Args>
        void operator()(Args&&...a) const
        {
            for(auto& item : m_listeners)
                item.second(std::forward<Args>(a)...);
        }

    private:

        boost::container::flat_map<Key, function_storage> m_listeners;

    };

}//! namespace stk;

#endif//! STK_CONTAINER_EVENT_DISPATCH_HPP

