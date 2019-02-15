//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cmath>
#include <type_traits>
#include <stk/container/concurrent_numeric_unordered_map.hpp>
#include <boost/polymorphic_cast.hpp>
#include <functional>

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
            auto x = boost::polymorphic_cast<T*>(v);
            op(x);
        }

        Fn op; 
    };

    template <typename T, typename Fn>
    inline type_switch_case<T, Fn> make_case(Fn&& fn)
    {
        return type_switch_case<T, Fn>{std::forward<Fn>(fn)};
    }

    template <typename U>
    inline std::intptr_t vtbl(const U* x)
    {
        static_assert(std::is_polymorphic<U>::value, "U must be a polymorphic type.");
        return *reinterpret_cast<const std::intptr_t*>(x);
    };
 
	struct type_switch_info
	{
		type_switch_info(std::uint64_t d)
			: data(d)
		{}

		type_switch_info(std::int32_t o, std::uint32_t c)
			: offset(o)
			, case_n(c)
		{}

		union
		{
			std::int32_t offset;
			std::uint32_t case_n;
			std::uint64_t data;
		};
	};

	template <typename Derived, std::size_t N>
	struct type_switch_base;

	template <typename Derived>
	struct type_switch_base<Derived, 1>
	{
		template <typename T, typename States>
		void eval(T* x, States& state)
		{
			static stk::concurrent_numeric_unordered_map<std::intptr_t, std::uint64_t> jump_targets;
			auto key = vtbl(x);
			const void* tptr;
			type_switch_info info(jump_targets.insert(key, std::uint64_t{}).first);
			switch (info.case_n) 
			{
				default:
					#define BOOST_PP_LOCAL_MACRO(n)                                                                                                                                                                                                 \
						if (tptr = std::get<n>(state).matches(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), BOOST_PP_ADD(n,1)).data); case BOOST_PP_ADD(n,1): std::get<n>(state).invoke(x); } else \
					/***/
					#define BOOST_PP_LOCAL_LIMITS (0, 1)
					#include BOOST_PP_LOCAL_ITERATE()
				case 2: break;
			};
		}
	};

    template <typename... Types>
    class type_switch : type_switch_base<type_switch<Types...>, sizeof...(Types)>
    {
    public:

        template <typename... Cases>
        type_switch(Cases&&... t)
            : m_state(std::forward<Cases>(t)...)
        {}

        template <typename T>
        void operator()(T* x)
        {
            using raw_type = std::decay<T>::type;
            struct type_switch_info
            {
                type_switch_info(std::uint64_t d)
                    : data(d)
                {}
                
                type_switch_info(std::int32_t o, std::uint32_t c)
                    : offset(o)
                    , case_n(c)
                {}

                union
                {
                    std::int32_t offset;
                    std::uint32_t case_n;
                    std::uint64_t data;
                };
            };
            static stk::concurrent_numeric_unordered_map<std::intptr_t, std::uint64_t> jump_targets;
            auto key = vtbl(x);
            const void* tptr;
            type_switch_info info(jump_targets.insert(key, std::uint64_t{}).first);
            switch (info.case_n) 
            {
                default:
					#define BOOST_PP_LOCAL_MACRO(n)                                                                                                                                                                                                 \
						if (tptr = std::get<n>(m_state).matches(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), BOOST_PP_ADD(n,1)).data); case BOOST_PP_ADD(n,1): std::get<n>(m_state).invoke(x); } else \
					/***/
					#define BOOST_PP_LOCAL_LIMITS (0, STK_MAX_TYPE_SWITCH_N)
					#include BOOST_PP_LOCAL_ITERATE()
						case BOOST_PP_ADD(STK_MAX_TYPE_SWITCH_N,1): break;
                    //if (tptr = std::get<0>(m_state).matches(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), 1).data); case 1: std::get<0>(m_state).invoke(x); } else
                    //if (tptr = std::get<1>(m_state).matches(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), 2).data); case 2: std::get<1>(m_state).invoke(x); } else
                    //if (tptr = std::get<2>(m_state).matches(x)) { if(info.case_n == 0) jump_targets.assign(key, type_switch_info(std::intptr_t(tptr) - std::intptr_t(x), 3).data); case 3: std::get<2>(m_state).invoke(x); } else
                    //    case BOOST_PP_ADD(STK_MAX_TYPE_SWITCH_N, 1): break;//! no true predicates.
            };
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

