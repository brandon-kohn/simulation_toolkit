#pragma once

#include <geometrix/utility/assert.hpp>
#include <boost/config.hpp>
#include <boost/type_traits/intrinsics.hpp>
#include <type_traits>

namespace stk {

#ifdef BOOST_ALIGNMENT_OF
    template <typename T, std::size_t Align = BOOST_ALIGNMENT_OF(T)>
    struct type_storage_pod
    {
        using value_type = T;
        using storage_type = typename std::aligned_storage<sizeof(T), Align>::type;
#else
    template <typename T>
    struct type_storage_pod
    {
        using value_type = T;
        using storage_type = typename std::aligned_storage<sizeof(T)>::type;
#endif

        type_storage_pod() = default;

        template <typename ... Args>
        T& initialize(Args&&... a)
        {
            GEOMETRIX_ASSERT(!m_initialized);
            new(&m_data) T(std::forward<Args>(a)...);
            m_initialized = true;
            return reinterpret_cast<T&>(m_data);
        }
        
        void destroy()
        {
            GEOMETRIX_ASSERT(m_initialized);
            reinterpret_cast<T&>(m_data).~T();
            m_initialized = false;
        }

        operator T&()
        {
            return reinterpret_cast<T&>(m_data);
        }

        operator T const&() const
        {
            return reinterpret_cast<T const&>(m_data);
        }

        T& operator *()
        {
            return reinterpret_cast<T&>(m_data);
        }

        T const& operator *() const
        {
            return reinterpret_cast<T const&>(m_data);
        }

        T* operator ->()
        {
            return reinterpret_cast<T*>(&m_data);
        }

        T const* operator ->() const
        {
            return reinterpret_cast<T const*>(&m_data);
        }

        bool is_initialized() const { return m_initialized; }

        template <typename U>
        friend type_storage_pod<U> make_empty_type_storage_pod()
        {
            auto result = type_storage_pod<T>{};
            result.m_initialized = false;
            return result;
        }

        template <typename U, typename... Args>
        friend type_storage_pod<U> make_type_storage_pod(Args&&... a)
        {
            auto result = type_storage_pod<T>{};
            result.initialize(std::forward<Args>(a)...);
            return result;
        }

    private:

        bool         m_initialized;
        storage_type m_data;

        static_assert(std::is_pod<type_storage_pod<T>>::value, "This must be a POD.");

    };

}//! namespace stk;

