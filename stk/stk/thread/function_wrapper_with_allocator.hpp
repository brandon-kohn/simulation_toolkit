//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <geometrix/utility/assert.hpp>
#include <geometrix/test/test.hpp>
#include <memory>

#define STK_DEBUG_FUNCTION_WRAPPER_DEALLOC 0

namespace stk { namespace thread {

    //! From Anthony William's Concurrency in Action.
    template <typename Alloc>
    class function_wrapper_with_allocator
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
            char cachepad[64];
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

        function_wrapper_with_allocator(const function_wrapper_with_allocator&) = delete;
        function_wrapper_with_allocator& operator=(const function_wrapper_with_allocator&) = delete;

        template <typename F>
        function_wrapper_with_allocator(alloc_t& alloc, F&& f)
            : m_pImpl( make_impl(std::forward<F>(f), alloc) )
        {}

        function_wrapper_with_allocator(function_wrapper_with_allocator&& f)
            : m_pImpl(std::move(f.m_pImpl))
        {
#if GEOMETRIX_TEST_ENABLED(STK_DEBUG_FUNCTION_WRAPPER_DEALLOC)
			m_executed = f.m_executed;
			GEOMETRIX_ASSERT(!m_executed);
			if (m_executed)
				throw std::logic_error("function_wrapper should not already be executed");
			f.m_executed = true;
#endif
		}

        function_wrapper_with_allocator& operator=(function_wrapper_with_allocator&& x)
        {
            m_pImpl = std::move(x.m_pImpl);
#if GEOMETRIX_TEST_ENABLED(STK_DEBUG_FUNCTION_WRAPPER_DEALLOC)
			m_executed = x.m_executed;
			GEOMETRIX_ASSERT(!m_executed);
			if (m_executed)
				throw std::logic_error("function_wrapper should not already be executed");
			x.m_executed = true;
#endif
			return *this;
        }

        void operator()()
        {
            GEOMETRIX_ASSERT(m_pImpl);
            m_pImpl->call();
#if GEOMETRIX_TEST_ENABLED(STK_DEBUG_FUNCTION_WRAPPER_DEALLOC)
			GEOMETRIX_ASSERT(!m_executed);
			m_executed = true;
#endif
		}

		~function_wrapper_with_allocator()
		{
#if GEOMETRIX_TEST_ENABLED(STK_DEBUG_FUNCTION_WRAPPER_DEALLOC)
			GEOMETRIX_ASSERT(!m_pImpl || m_executed);
			if (m_pImpl && !m_executed)
				throw std::logic_error("function_wrapper not executed");
#endif		 
		}

        bool empty() const
        {
            return !m_pImpl;
        }

    private:

        std::unique_ptr<impl_base, deleter> m_pImpl;
#if GEOMETRIX_TEST_ENABLED(STK_DEBUG_FUNCTION_WRAPPER_DEALLOC)
		bool m_executed{ false };
#endif
    };

}}//! namespace stk::thread;

