//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/checked_delete.hpp>
#include <memory>
#include <type_traits>

namespace stk{

    namespace detail{
		template <typename T>
		inline void deleter(T* x)
		{
			boost::checked_delete(x);
		}
    }//! namespace detail;

	template <typename T>
	class pimpl
	{
		using deleter_type = void (*)(T*);
		static void dummy_deleter(T* x)
		{
			delete x;
		}

	public:

		using pointer = T*;
		using const_pointer = typename std::add_const<pointer>::type;
		using reference = typename std::add_lvalue_reference<T>::type;
		using const_reference = typename std::add_const<reference>::type;

		pimpl()
			: m_pimpl(nullptr, &dummy_deleter)
		{}

		template <typename D>
		pimpl(pointer p, D d)
			: m_pimpl(p, d)
		{}

		pimpl(const pimpl& o)
			: m_pimpl(o.clone(), o.m_pimpl.get_deleter())
		{}

		pimpl(pimpl&& o)
			: m_pimpl(std::move(o.m_pimpl))
		{}

		~pimpl() = default;
		
		pimpl& operator=(pimpl o)
		{
			o.swap(*this);
			return *this;
		}

		const_pointer operator->() const
		{
			return m_pimpl.get();
		}

		pointer operator->()
		{
			return m_pimpl.get();
		}

		reference operator*()
		{
			return *m_pimpl;
		}

		const_reference operator*() const
		{
			return *m_pimpl;
		}

		void swap(pimpl& o)
		{
			m_pimpl.swap(o.m_pimpl);
		}

		operator bool() const
		{
			return m_pimpl.get() != nullptr;
		}

	private:

		pointer clone() const
		{
			return m_pimpl ? new T(*m_pimpl) : nullptr;
		}

		std::unique_ptr<T, deleter_type> m_pimpl;

	};

    template <typename T, typename... Args>
    inline pimpl<T> make_pimpl(Args&&... a)
    {
        return pimpl<T>(new T(std::forward<Args>(a)...), &detail::deleter<T>); 
    }

}//! namespace stk;

