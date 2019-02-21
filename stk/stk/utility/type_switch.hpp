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
        bool matches(U* u) const
        {
			return dynamic_cast<T*>(u) != nullptr;
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

	namespace detail {
		template <typename Fn>
		struct get_arg;
		template <typename R, typename C, typename Arg>
		struct get_arg<R(C::*)(Arg) const>
		{
			using type = Arg;
		};
		template <typename R, typename C, typename Arg>
		struct get_arg<R(C::*)(Arg)>
		{
			using type = Arg;
		};

		template <typename Lambda>
		struct arg_type
		{
		private:
			using fn_type = decltype(&Lambda::operator());
			using arg1_type = typename get_arg<fn_type>::type;// typename boost::function_traits<fn_type>::arg1_type;
			static_assert(std::is_pointer<arg1_type>::value, "type_switch cases must operate on a pointer type.");
		public:

			using type = typename std::remove_pointer<arg1_type>::type;
		};
	}//! namespace detail;

    template <typename Fn>
    inline type_switch_case<typename detail::arg_type<Fn>::type, Fn> type_case(Fn&& fn)
    {
        return type_switch_case<typename detail::arg_type<Fn>::type, Fn>{std::forward<Fn>(fn)};
    }
    
	template <typename T, typename Fn>
	struct type_switch_default 
    {
        type_switch_default(Fn&& op)
            : op(std::forward<Fn>(op))
        {}

        template <typename U>
        bool matches(U*) const
        {
			return true;
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
    
	template <typename Fn>
    inline type_switch_default<typename detail::arg_type<Fn>::type, Fn> type_default(Fn&& fn)
    {
        return type_switch_default<typename detail::arg_type<Fn>::type, Fn>{std::forward<Fn>(fn)};
    }

    template <typename... Types>
    class type_switch : detail::type_switch_base<type_switch<Types...>, sizeof...(Types)>
    {
        using base_type = detail::type_switch_base<type_switch<Types...>, sizeof...(Types)>;

    public:

        type_switch(Types&&... t)
            : m_state(std::forward<Types>(t)...)
        {}

        template <typename T>
        void operator()(T* x)
        {
            return base_type::eval(x, m_state);
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

