//
//! Copyright � 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#define DIMENSION BOOST_PP_ITERATION()

namespace stk { namespace detail {

	template <typename Derived>
	struct type_switch_base<Derived, DIMENSION>
	{
		template <typename T, typename States>
		void eval(T* x, States& state)
		{
			static stk::concurrent_numeric_unordered_map<std::intptr_t, std::uint64_t> jump_targets;
			auto key = (std::intptr_t)&typeid(*x);// vtbl(x);
			const void* tptr;
			auto case_n = jump_targets.insert(key, std::uint64_t{}).first;
			switch (case_n) 
			{
				default:
					#define BOOST_PP_LOCAL_MACRO(n)                               \
						if (tptr = std::get<n>(state).matches(x))                 \
						{                                                         \
						    if(case_n == 0)                                       \
						       jump_targets.assign(key, BOOST_PP_ADD(n,1));       \
						    case BOOST_PP_ADD(n,1): std::get<n>(state).invoke(x); \
						} else                                                    \
					/***/
					#define BOOST_PP_LOCAL_LIMITS (0, DIMENSION-1)
					#include BOOST_PP_LOCAL_ITERATE()
				case BOOST_PP_ADD(DIMENSION,2): break;
			};
		}
		
	};

}}//! namespace stk::detail;

#undef DIMENSION
