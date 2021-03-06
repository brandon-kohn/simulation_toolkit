/*
MIT License

Copyright (c) 2017 Andrey Kubarkov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <type_traits>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <boost/config.hpp>

namespace stk
{
    /**
     * @brief The fixed_function<R(ARGS...), STORAGE_SIZE> class implements
     * functional object.
     * This function is analog of 'std::function' with limited capabilities:
     *  - It supports only move semantics.
     *  - The size of functional objects is limited to storage size.
     * Due to limitations above it is much faster on creation and copying than
     * std::function.
     */
    template <typename SIGNATURE, size_t STORAGE_SIZE = 128>
    class fixed_function;

    template <typename R, typename... ARGS, size_t STORAGE_SIZE>
    class fixed_function<R(ARGS...), STORAGE_SIZE>
    {
        typedef R (*func_ptr_type)(ARGS...);

    public:

        static const std::size_t storage_size = STORAGE_SIZE;

        fixed_function()
            : m_function_ptr(nullptr)
            , m_method_ptr(nullptr)
            , m_alloc_ptr(nullptr)
        {}

        /**
         * @brief fixed_function Constructor from functional object.
         * @param object Functor object will be stored in the internal storage
         * using move constructor. Unmovable objects are prohibited explicitly.
         */
        template <typename FUNC>
        fixed_function(FUNC&& object)
            : fixed_function()
        {
            typedef typename std::remove_reference<FUNC>::type unref_type;

            static_assert(sizeof(unref_type) < STORAGE_SIZE, "functional object doesn't fit into internal storage");
            static_assert(std::is_move_constructible<unref_type>::value, "Should be of movable type");

            m_method_ptr = [](void* object_ptr, func_ptr_type, ARGS... args) -> R
            {
                return static_cast<unref_type*>(object_ptr)->operator()(args...);
            };

            m_alloc_ptr = [](void* storage_ptr, void* object_ptr)
            {
                if(object_ptr)
                {
                    unref_type* x_object = static_cast<unref_type*>(object_ptr);
                    new(storage_ptr) unref_type(std::move(*x_object));
                }
                else
                {
                    static_cast<unref_type*>(storage_ptr)->~unref_type();
                }
            };

            m_alloc_ptr(&m_storage, &object);
        }

        /**
         * @brief fixed_function Constructor from free function or static member.
         */
        template <typename RET, typename... PARAMS>
        fixed_function(RET (*func_ptr)(PARAMS...))
            : fixed_function()
        {
            m_function_ptr = func_ptr;
            m_method_ptr = [](void*, func_ptr_type f_ptr, ARGS... args) -> R
            {
                return static_cast<RET (*)(PARAMS...)>(f_ptr)(args...);
            };
        }

        fixed_function(fixed_function&& o)
            : fixed_function()
        {
            moveFromOther(o);
        }

        fixed_function& operator=(fixed_function&& o)
        {
            moveFromOther(o);
            return *this;
        }

        ~fixed_function()
        {
            if(m_alloc_ptr)
                m_alloc_ptr(&m_storage, nullptr);
        }

        /**
         * @brief operator () Execute stored functional object.
         * @throws std::runtime_error if no functional object is stored.
         */
        BOOST_FORCEINLINE R operator()(ARGS... args) const
        {
            if(!m_method_ptr) 
                throw std::runtime_error("call of empty functor");
            return m_method_ptr((void*)&m_storage, m_function_ptr, args...);
        }

        bool empty() const 
        {
            return !m_method_ptr;
        }

    private:

        fixed_function& operator=(const fixed_function&) = delete;
        fixed_function(const fixed_function&) = delete;

        union
        {
            typename std::aligned_storage<STORAGE_SIZE, sizeof(size_t)>::type m_storage;
            func_ptr_type m_function_ptr;
        };

        typedef R (*method_type)(void* object_ptr, func_ptr_type free_func_ptr, ARGS... args);
        method_type m_method_ptr;

        typedef void (*alloc_type)(void* storage_ptr, void* object_ptr);
        alloc_type m_alloc_ptr;

        void moveFromOther(fixed_function& o)
        {
            if(this == &o) 
                return;

            if(m_alloc_ptr)
            {
                m_alloc_ptr(&m_storage, nullptr);
                m_alloc_ptr = nullptr;
            }
            else
            {
                m_function_ptr = nullptr;
            }

            m_method_ptr = o.m_method_ptr;
            o.m_method_ptr = nullptr;

            if(o.m_alloc_ptr)
            {
                m_alloc_ptr = o.m_alloc_ptr;
                m_alloc_ptr(&m_storage, &o.m_storage);
            }
            else
            {
                m_function_ptr = o.m_function_ptr;
            }
        }
    };
}//! namespace stk;
