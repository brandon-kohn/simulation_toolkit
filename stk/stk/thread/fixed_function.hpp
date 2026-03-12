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

#include <boost/config.hpp>

#include <cstddef>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace stk
{
    //! The fixed_function<R(ARGS...), STORAGE_SIZE> class implements
    //! a move-only function wrapper with fixed internal storage.
    //!
    //! Compared to std::function:
    //! - only move semantics are supported
    //! - callable size is limited to STORAGE_SIZE
    //! - no dynamic allocation is performed
    template <typename SIGNATURE, std::size_t STORAGE_SIZE = 128>
    class fixed_function;

    template <typename R, typename... ARGS, std::size_t STORAGE_SIZE>
    class fixed_function<R(ARGS...), STORAGE_SIZE>
    {
        using func_ptr_type = R (*)(ARGS...);
        using storage_type = std::aligned_storage_t<STORAGE_SIZE, alignof(std::max_align_t)>;
        using method_type = R (*)(void* object_ptr, func_ptr_type free_func_ptr, ARGS&&... args);
        using alloc_type = void (*)(void* storage_ptr, void* object_ptr);

    public:
        static constexpr std::size_t storage_size = STORAGE_SIZE;

        fixed_function() noexcept
            : m_function_ptr(nullptr)
            , m_method_ptr(nullptr)
            , m_alloc_ptr(nullptr)
        {}

        //! Construct from a functor/lambda stored in internal storage.
        template <
            typename FUNC,
            typename unref_type = std::remove_reference_t<FUNC>,
            typename = std::enable_if_t<!std::is_same_v<unref_type, fixed_function>>
        >
        fixed_function(FUNC&& object)
            : fixed_function()
        {
            static_assert(sizeof(unref_type) <= STORAGE_SIZE,
                "functional object doesn't fit into internal storage");
            static_assert(alignof(unref_type) <= alignof(storage_type),
                "functional object requires stricter alignment than internal storage provides");
            static_assert(std::is_move_constructible_v<unref_type>,
                "functional object must be move constructible");

            m_method_ptr = [](void* object_ptr, func_ptr_type, ARGS&&... args) -> R
            {
                return (*static_cast<unref_type*>(object_ptr))(std::forward<ARGS>(args)...);
            };

            m_alloc_ptr = [](void* storage_ptr, void* object_ptr)
            {
                if(object_ptr)
                {
                    auto* x_object = static_cast<unref_type*>(object_ptr);
                    new(storage_ptr) unref_type(std::move(*x_object));
                }
                else
                {
                    static_cast<unref_type*>(storage_ptr)->~unref_type();
                }
            };

            new(&m_storage) unref_type(std::forward<FUNC>(object));
        }

        //! Construct from a free function or static member function.
        template <typename RET, typename... PARAMS>
        fixed_function(RET (*func_ptr)(PARAMS...)) noexcept
            : fixed_function()
        {
            m_function_ptr = reinterpret_cast<func_ptr_type>(func_ptr);

            m_method_ptr = [](void*, func_ptr_type f_ptr, ARGS&&... args) -> R
            {
                auto typed_ptr = reinterpret_cast<RET (*)(PARAMS...)>(f_ptr);
                return typed_ptr(std::forward<ARGS>(args)...);
            };
        }

        fixed_function(fixed_function&& other) noexcept
            : fixed_function()
        {
            moveFromOther(other);
        }

        fixed_function& operator=(fixed_function&& other) noexcept
        {
            if(this != &other)
            {
                reset();
                moveFromOther(other);
            }
            return *this;
        }

        fixed_function(const fixed_function&) = delete;
        fixed_function& operator=(const fixed_function&) = delete;

        ~fixed_function()
        {
            reset();
        }

        //! Execute the stored callable.
        //! Throws std::runtime_error if no callable is stored.
        BOOST_FORCEINLINE R operator()(ARGS... args) const
        {
            if(!m_method_ptr)
                throw std::runtime_error("call of empty functor");

            return m_method_ptr(const_cast<void*>(static_cast<const void*>(&m_storage)),
                                m_function_ptr,
                                std::forward<ARGS>(args)...);
        }

        BOOST_FORCEINLINE bool empty() const noexcept
        {
            return m_method_ptr == nullptr;
        }

        BOOST_FORCEINLINE explicit operator bool() const noexcept
        {
            return !empty();
        }

        void reset() noexcept
        {
            if(m_alloc_ptr)
            {
                m_alloc_ptr(&m_storage, nullptr);
            }

            m_function_ptr = nullptr;
            m_method_ptr = nullptr;
            m_alloc_ptr = nullptr;
        }

    private:
        union
        {
            storage_type m_storage;
            func_ptr_type m_function_ptr;
        };

        method_type m_method_ptr;
        alloc_type m_alloc_ptr;

        void moveFromOther(fixed_function& other) noexcept
        {
            m_method_ptr = other.m_method_ptr;
            m_alloc_ptr = other.m_alloc_ptr;

            if(other.m_alloc_ptr)
            {
                m_alloc_ptr(&m_storage, &other.m_storage);
            }
            else
            {
                m_function_ptr = other.m_function_ptr;
            }

            other.m_method_ptr = nullptr;
            other.m_alloc_ptr = nullptr;
            other.m_function_ptr = nullptr;
        }
    };
} //! namespace stk