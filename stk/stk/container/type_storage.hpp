#pragma once

#include <geometrix/utility/assert.hpp>
#include <boost/config.hpp>
#include <boost/type_traits/intrinsics.hpp>
#include <type_traits>

namespace stk {

#ifdef BOOST_ALIGNMENT_OF
    template <typename T, std::size_t Align = BOOST_ALIGNMENT_OF(T)>
    struct type_storage
    {
        using value_type = T;
        using storage_type = typename std::aligned_storage<sizeof(T), Align>::type;
#else
    template <typename T>
    struct type_storage
    {
        using value_type = T;
        using storage_type = typename std::aligned_storage<sizeof(T)>::type;
#endif

        type_storage() = default;
        type_storage(const type_storage& o) 
        {
            if(o.m_initialized)
                initialize(*o);
        }

        type_storage(type_storage&& o) 
        {
            if(o.m_initialized)
                initialize(std::move(*o));
        }

        type_storage& operator =(type_storage&& o) 
        {
            if(m_initialized)
                if(o.m_initialized)
                    (**this) = std::move(*o);
                else
                    destroy();
            else
                if(o.m_initialized)
                    initialize(std::move(*o));
                   
            return *this;
        }

        type_storage& operator =(type_storage const& o) 
        {
            if(m_initialized)
                if(o.m_initialized)
                    (**this) = *o;
                else
                    destroy();
            else
                if(o.m_initialized)
                    initialize(*o);
                   
            return *this;
        }

        type_storage(T value)
        {
            initialize(std::move(value));
        }

        ~type_storage()
        {
            if(m_initialized)
                destroy();
        }

        template <typename ... Args>
        T& initialize(Args&&... a)
        {
            GEOMETRIX_ASSERT(!m_initialized);
            new(&m_data) T(std::forward<Args>(a)...);
            m_initialized = true;
            return reinterpret_cast<T&>(m_data);
        }

        type_storage& operator =(const T& a)
        {
			if (m_initialized)
				(**this) = a;
			else
				initialize(a);
            return *this;
        }

        type_storage& operator =(T&& a)
        {
			if (m_initialized)
				(**this) = std::forward<T>(a);
			else
				initialize(std::forward<T>(a));
            return *this;
        }

        void destroy()
        {
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

    private:

        bool         m_initialized{false};
        storage_type m_data;

    };

}//! namespace stk;

