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
#include <boost/detail/workaround.hpp>
#include <functional>
#include <type_traits>

namespace stk{
	
	namespace detail {
		template <typename Fn>
		struct fn_traits;
		template <typename R, typename C, typename Arg>
		struct fn_traits<R(C::*)(Arg) const>
		{
			using type = Arg;
			using result_type = R;
		};
		template <typename R, typename C, typename Arg>
		struct fn_traits<R(C::*)(Arg)>
		{
			using type = Arg;
			using result_type = R;
		};

		template <typename Lambda>
		struct lambda_traits
		{
		private:
			using fn_type = decltype(&Lambda::operator());
			using arg1_type = typename fn_traits<fn_type>::type;
			static_assert(std::is_pointer<arg1_type>::value, "type_switch cases must operate on a pointer type.");
		public:

			using type = typename std::remove_pointer<arg1_type>::type;
			using result_type = typename fn_traits<fn_type>::result_type;
		};
	}//! namespace detail;


    template <typename T, typename Fn>
    struct type_switch_case
    {
		using result_type = typename detail::lambda_traits<Fn>::result_type;

        type_switch_case(Fn&& op)
            : op(std::forward<Fn>(op))
        {}

        template <typename U>
        bool matches(U* u) const
        {
			return dynamic_cast<T*>(u) != nullptr;
        }

        template <typename U>
        result_type invoke(U* v)
        {
			//GEOMETRIX_ASSERT(dynamic_cast<T*>(v) == v);
			auto x = (T*)v;// boost::polymorphic_downcast<T*>(v);
            return op(x);
        }

        Fn op; 
    };

    template <typename T, typename Fn>
    inline type_switch_case<T, Fn> make_case(Fn&& fn)
    {
        return type_switch_case<T, Fn>{std::forward<Fn>(fn)};
    }

    template <typename Fn>
    inline type_switch_case<typename detail::lambda_traits<Fn>::type, Fn> type_case(Fn&& fn)
    {
        return type_switch_case<typename detail::lambda_traits<Fn>::type, Fn>{std::forward<Fn>(fn)};
    }
    
	template <typename T, typename Fn>
	struct type_switch_default 
    {
		using result_type = typename detail::lambda_traits<Fn>::result_type;

        type_switch_default(Fn&& op)
            : op(std::forward<Fn>(op))
        {}

        template <typename U>
        bool matches(U*) const
        {
			return true;
        }

        template <typename U>
        result_type invoke(U* v)
        {
			//GEOMETRIX_ASSERT(dynamic_cast<T*>(v) == v);
			auto x = (T*)v;// boost::polymorphic_downcast<T*>(v);
            return op(x);
        }

        Fn op; 
    };
    
	template <typename Fn>
    inline type_switch_default<typename detail::lambda_traits<Fn>::type, Fn> type_default(Fn&& fn)
    {
        return type_switch_default<typename detail::lambda_traits<Fn>::type, Fn>{std::forward<Fn>(fn)};
    }

    template <typename... Types>
    class type_switch : detail::type_switch_base<type_switch<Types...>, sizeof...(Types)>
    {
		static_assert(sizeof...(Types) > 0, "Cannot create an empty type_switch.");
        using base_type = detail::type_switch_base<type_switch<Types...>, sizeof...(Types)>;
		using first_type = typename std::decay<decltype(std::get<0>(std::declval<std::tuple<Types...>>()))>::type;

    public:
	
		using result_type = typename first_type::result_type;

        type_switch(Types&&... t)
            : m_state(std::forward<Types>(t)...)
        {
#if BOOST_WORKAROUND(BOOST_MSVC, <= 1800)
		    clear_cache();//! Initialize the static in case of threaded access (which doesn't work on vc120.)
#endif
        }

        template <typename T>
        result_type operator()(T* x)
        {
            return base_type::eval(x, m_state);
        }

		//! Not thread-safe. Can be used to clear memory used by the underlying static type cache.
		void clear_cache()
		{
			base_type::clear_jump_targets();
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

