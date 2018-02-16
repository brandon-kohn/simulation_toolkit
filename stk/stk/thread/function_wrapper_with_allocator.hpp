//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/fixed_function.hpp>
#include <boost/type_traits/is_stateless.hpp>
#include <geometrix/utility/assert.hpp>
#include <geometrix/test/test.hpp>
#include <memory>

namespace stk { namespace thread {

//    template <typename Alloc, typename EnableIf=void>
//	class function_wrapper_with_allocator;

    //! From Anthony William's Concurrency in Action.

	//! For stateful allocators
	/*
    template <typename Alloc>
    class function_wrapper_with_allocator<Alloc, typename std::enable_if<!boost::is_stateless<Alloc>::value>::type>
    {
        using alloc_t = Alloc;
        
        struct impl_base
        {
        public:
            virtual void call() = 0;
            virtual ~impl_base(){}
        };

        template <typename F>
        struct move_impl_type : impl_base
        {
			using f_type = typename std::decay<F>::type;
            move_impl_type(F&& f)
                : m_f(std::move(f))
            {}

            void call()
            {
                m_f();
            }

            f_type m_f;
        };

        struct deleter
        {
			deleter() = default;
            deleter(alloc_t& alloc)
                : m_alloc(&alloc)
            {}

            void operator()(impl_base* p)
            {
				GEOMETRIX_ASSERT(m_alloc);
                p->~impl_base();
                m_alloc->deallocate(reinterpret_cast<char*>(p), 1);
            }

			alloc_t* m_alloc{ nullptr };
        };

        template <typename F>
        std::unique_ptr<impl_base, deleter> make_impl(F&& f, alloc_t& alloc)
        {
            void* ptr = alloc.allocate(sizeof(move_impl_type<F>));
            new (ptr) move_impl_type<F>(std::move(f));
			auto del = deleter{ alloc };
			auto bptr = reinterpret_cast<impl_base*>(ptr);
            return std::unique_ptr<impl_base, deleter>(bptr, del);
        }

    public:

        using result_type = void;

        function_wrapper_with_allocator() = default;

		template <typename F, typename std::enable_if<fixed_function<void()>::storage_size < sizeof(typename std::decay<F>::type), int>::type = 0>
        function_wrapper_with_allocator(alloc_t& alloc, F&& f)
            : m_pImpl( make_impl(std::forward<F>(f), alloc) )
			, m_fixed_function([p = m_pImpl.get()]() 
		      { 
			      GEOMETRIX_ASSERT(p); 
				  p->call();
		      })
        {}

        template <typename F, typename std::enable_if<sizeof(typename std::decay<F>::type) <= fixed_function<void()>::storage_size, int>::type = 0>
        function_wrapper_with_allocator(alloc_t& alloc, F&& f)
            : m_fixed_function(std::forward<F>(f))
        {}

        function_wrapper_with_allocator(function_wrapper_with_allocator&& f)
            : m_pImpl(std::move(f.m_pImpl))
            , m_fixed_function(std::move(f.m_fixed_function))
        {}

        function_wrapper_with_allocator& operator=(function_wrapper_with_allocator&& x)
        {
            m_pImpl = std::move(x.m_pImpl);
            m_fixed_function = std::move(x.m_fixed_function);
			return *this;
        }

        function_wrapper_with_allocator(const function_wrapper_with_allocator&) = delete;
        function_wrapper_with_allocator& operator=(const function_wrapper_with_allocator&) = delete;

        BOOST_FORCEINLINE void operator()()
        {
            m_fixed_function(); 
		}

		~function_wrapper_with_allocator()
		{}

        bool empty() const
        {
            return !m_pImpl && m_fixed_function.empty();
        }

    private:

        std::unique_ptr<impl_base, deleter> m_pImpl;
        fixed_function<void(), 128> m_fixed_function;
    };
*/
	//! Stateless allocators.
	template <typename Alloc>
    class function_wrapper_with_allocator//<Alloc, typename std::enable_if<boost::is_stateless<Alloc>::value>::type>
    {
        using alloc_t = Alloc;
        
        struct impl_base
        {
        public:
            virtual void call() = 0;
            virtual ~impl_base(){}
        };

        template <typename F>
        struct move_impl_type : impl_base
        {
			using f_type = typename std::decay<F>::type;
            move_impl_type(F&& f)
                : m_f(std::move(f))
            {}

            void call()
            {
                m_f();
            }

            f_type m_f;
        };

        struct deleter
        {
			deleter() = default;

            void operator()(impl_base* p)
            {
				GEOMETRIX_ASSERT(m_alloc);
                p->~impl_base();
                alloc_t().deallocate(reinterpret_cast<char*>(p), 1);
            }
        };

        template <typename F>
        std::unique_ptr<impl_base, deleter> make_impl(F&& f)
        {
            void* ptr = alloc_t().allocate(sizeof(move_impl_type<F>));
            new (ptr) move_impl_type<F>(std::move(f));
			auto del = deleter{};
			auto bptr = reinterpret_cast<impl_base*>(ptr);
            return std::unique_ptr<impl_base, deleter>(bptr, del);
        }

    public:

        using result_type = void;

        function_wrapper_with_allocator() = default;

		template <typename F, typename std::enable_if<fixed_function<void()>::storage_size < sizeof(typename std::decay<F>::type), int>::type = 0>
        function_wrapper_with_allocator(F&& f)
            : m_pImpl( make_impl(std::forward<F>(f)) )
			, m_fixed_function([p = m_pImpl.get()]() 
		      { 
			      GEOMETRIX_ASSERT(p); 
				  p->call();
		      })
        {}

        template <typename F, typename std::enable_if<sizeof(typename std::decay<F>::type) <= fixed_function<void()>::storage_size, int>::type = 0>
        function_wrapper_with_allocator(F&& f)
            : m_fixed_function(std::forward<F>(f))
        {}

        function_wrapper_with_allocator(function_wrapper_with_allocator&& f)
            : m_pImpl(std::move(f.m_pImpl))
            , m_fixed_function(std::move(f.m_fixed_function))
        {}

        function_wrapper_with_allocator& operator=(function_wrapper_with_allocator&& x)
        {
            m_pImpl = std::move(x.m_pImpl);
            m_fixed_function = std::move(x.m_fixed_function);
			return *this;
        }

        function_wrapper_with_allocator(const function_wrapper_with_allocator&) = delete;
        function_wrapper_with_allocator& operator=(const function_wrapper_with_allocator&) = delete;

        BOOST_FORCEINLINE void operator()()
        {
            m_fixed_function(); 
		}

		~function_wrapper_with_allocator()
		{}

        bool empty() const
        {
            return !m_pImpl && m_fixed_function.empty();
        }

    private:

        std::unique_ptr<impl_base, deleter> m_pImpl;
        fixed_function<void(), 128> m_fixed_function;
    };


}}//! namespace stk::thread;
