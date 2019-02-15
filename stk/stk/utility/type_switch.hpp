//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/utility/detail/type_switch_generator.hpp>
#include <boost/polymorphic_cast.hpp>
#include <functional>
#include <type_traits>

namespace stk{

    template <typename T, typename Fn>
    struct type_switch_case
    {
        type_switch_case(Fn&& op)
            : op(std::forward<Fn>(op))
        {}

        template <typename U>
        T* matches(U* u)
        {
			return dynamic_cast<T*>(u);
        }

        template <typename U>
        void invoke(U* v)
        {
			//GEOMETRIX_ASSERT(dynamic_cast<T*>(v) == v);
			auto x = (T*)v;// boost::polymorphic_downcast<T*>(v);
            op(x);
        }

        Fn op; 
    };

    template <typename T, typename Fn>
    inline type_switch_case<T, Fn> make_case(Fn&& fn)
    {
        return type_switch_case<T, Fn>{std::forward<Fn>(fn)};
    }


    template <typename... Types>
    class type_switch : detail::type_switch_base<type_switch<Types...>, sizeof...(Types)>
    {
    public:

        template <typename... Cases>
        type_switch(Cases&&... t)
            : m_state(std::forward<Cases>(t)...)
        {}

        template <typename T>
        void operator()(T* x)
        {
            return eval(x, m_state);
        }
    private:

        template <std::size_t I, typename... Args>
        decltype(std::get<I>(std::declval<std::tuple<Types...>>())) get_case()
        {
            return std::get<I>(m_state);
        }

        std::tuple<Types...> m_state;

    };

    template <typename... Us>
    inline type_switch<Us...> make_switch(Us&&... us)
    {
        return type_switch<Us...>(std::forward<Us>(us)...);
    }

}//! namespace stk;

