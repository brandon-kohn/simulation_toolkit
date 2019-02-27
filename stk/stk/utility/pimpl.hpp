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
#include <boost/config.hpp>
#include <geometrix/utility/assert.hpp>
#include <memory>
#include <type_traits>

namespace stk{

    namespace detail{
		template <typename T>
		inline void deleter(T* x)
		{
			boost::checked_delete(x);
		}
	
		template <typename T>
		inline void dummy_deleter(T* x)
		{
			GEOMETRIX_ASSERT(x == nullptr);
		}

		template <typename T>
		inline T* copier(T* x)
		{
			static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible.");
			GEOMETRIX_ASSERT(x != nullptr);
			return new T(*x);
		}

		template <typename T>
		inline T* dummy_copy(T* x)
		{
			GEOMETRIX_ASSERT(x == nullptr); 
			return x;
		}
    }//! namespace detail;

	template <typename T>
	class pimpl
	{
	public:

		using pointer = T*;
		using const_pointer = typename std::add_const<pointer>::type;
		using reference = typename std::add_lvalue_reference<T>::type;
		using const_reference = typename std::add_const<reference>::type;

		BOOST_CONSTEXPR pimpl() BOOST_NOEXCEPT
			: m_pimpl(nullptr, &detail::dummy_deleter<T>)
			, m_copier(&detail::dummy_copy)
		{}

		template <typename D, typename C>
		pimpl(pointer p, D d, C c)
			: m_pimpl(p, d)
			, m_copier(c)
		{}

		pimpl(const pimpl& o)
			: m_pimpl(o.clone(), o.m_pimpl.get_deleter())
			, m_copier(o.m_copier)
		{}

		pimpl(pimpl&& o)
			: m_pimpl(std::move(o.m_pimpl))
			, m_copier(o.m_copier)
		{}

		~pimpl() = default;
		
		pimpl& operator=(pimpl&& o)
		{
			m_pimpl = std::move(o.m_pimpl);
			m_copier = std::move(o.m_copier);
			return *this;
		}
		
		pimpl& operator=(const pimpl& o)
		{
			pimpl tmp(o);
			tmp.swap(*this);
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
			using std::swap;
			m_pimpl.swap(o.m_pimpl);
			swap(m_copier, o.m_copier);
		}

		operator bool() const
		{
			return m_pimpl.get() != nullptr;
		}

	private:

		pointer clone() const
		{
			return m_pimpl ? m_copier(m_pimpl.get()) : nullptr;
		}

		using deleter_type = void (*)(T*);
		using copier_type = T* (*)(T*);
		std::unique_ptr<T, deleter_type> m_pimpl;
		copier_type                      m_copier;

	};

    template <typename T, typename... Args>
    inline pimpl<T> make_pimpl(Args&&... a)
	{
		static_assert(sizeof(T) > 0, "T must be a complete type.");
        return pimpl<T>(new T(std::forward<Args>(a)...), &detail::deleter<T>, &detail::copier<T>); 
    }

}//! namespace stk;

