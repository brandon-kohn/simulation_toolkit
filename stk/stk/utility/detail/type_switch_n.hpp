//
//! Copyright © 2019
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
		static stk::concurrent_numeric_unordered_map<std::intptr_t, std::uint64_t>& jump_targets()
		{
			static stk::concurrent_numeric_unordered_map<std::intptr_t, std::uint64_t> instance;
			return instance;
		}

		//! May be called from a quiescent state to clear the cached jump targets.
		void clear_jump_targets()
		{
			jump_targets().clear();
			jump_targets().quiesce();
		}

		template <typename T, typename States>
		void eval(T* x, States& state)
		{
			auto key = (std::intptr_t)&typeid(*x);// vtbl(x);
			auto it = jump_targets().find(key);
			auto case_n = it ? *it : std::uint64_t{};
			switch (case_n) 
			{
				default:
					#define BOOST_PP_LOCAL_MACRO(n)                               \
						if (std::get<n>(state).matches(x))                        \
						{                                                         \
						    if(case_n == 0)                                       \
						       jump_targets().assign(key, BOOST_PP_ADD(n,1));     \
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
