//
//! Copyright © 2026
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <geometrix/utility/assert.hpp>
#include <memory>
#include <type_traits>
#include <utility>

namespace stk
{
    namespace detail
    {
        template<class T>
        using owned_deleter_t = void(*)(T*) noexcept;

        template<class T>
        inline void default_delete_fn(T* p) noexcept
        {
            delete p;
        }

        template<class D, class T>
        inline constexpr bool deleter_convertible_v =
            std::is_convertible_v<D, owned_deleter_t<T>>;
    } //! namespace detail

    //! Move-only owning smart pointer with a hidden function-pointer deleter.
    //!
    //! This is a convenience wrapper around:
    //!
    //!     std::unique_ptr<T, void(*)(T*) noexcept>
    //!
    //! The purpose is to hide the deleter type from template signatures while
    //! still allowing stateless custom deleters.
    //!
    //! Supported deleter forms:
    //! - function pointers
    //! - non-capturing lambdas
    //!
    //! Non-capturing lambdas should generally be written with unary + to force
    //! decay to a function pointer:
    //!
    //!     +[](T* p) noexcept { ... }
    //!
    //! This type is intended for:
    //! - single-object heap ownership
    //! - pointer-like foreign resources with stateless cleanup functions
    //!
    //! This type is not intended for:
    //! - shared ownership
    //! - arrays
    //! - stateful deleters
    template<class T>
    class owned_ptr
    {
    public:
        static_assert(!std::is_array_v<T>,
            "stk::owned_ptr does not support array types");
        static_assert(!std::is_reference_v<T>,
            "stk::owned_ptr cannot own a reference type");
        static_assert(!std::is_function_v<T>,
            "stk::owned_ptr cannot own a function type");
        static_assert(std::is_object_v<T> || std::is_void_v<T>,
            "stk::owned_ptr requires an object type or void");

        using element_type = T;
        using pointer = T*;
        using deleter_type = detail::owned_deleter_t<T>;
        using unique_type = std::unique_ptr<T, deleter_type>;

        //! Construct an empty owned_ptr.
        //!
        //! The stored deleter defaults to delete for T.
        constexpr owned_ptr() noexcept
            : m_ptr(nullptr, &detail::default_delete_fn<T>)
        {}

        //! Construct an empty owned_ptr from nullptr.
        constexpr owned_ptr(std::nullptr_t) noexcept
            : owned_ptr()
        {}

        //! Adopt an existing pointer with an explicit deleter.
        //!
        //! Preconditions:
        //! - d must not be null
        //! - d must correctly destroy or release p
        //! - p must have been obtained in a way compatible with d
        //!
        //! Examples:
        //! - new T(...) paired with delete
        //! - fopen(...) paired with fclose
        constexpr owned_ptr(pointer p, deleter_type d) noexcept
            : m_ptr(p, d)
        {
            GEOMETRIX_ASSERT(d != nullptr);
        }

        owned_ptr(owned_ptr&&) noexcept = default;
        owned_ptr& operator=(owned_ptr&&) noexcept = default;

        owned_ptr(const owned_ptr&) = delete;
        owned_ptr& operator=(const owned_ptr&) = delete;

        //! Reset to empty.
        owned_ptr& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        //! Return the stored raw pointer.
        [[nodiscard]] pointer get() const noexcept
        {
            return m_ptr.get();
        }

        //! Return the stored deleter.
        [[nodiscard]] deleter_type get_deleter() const noexcept
        {
            return m_ptr.get_deleter();
        }

        //! Return true if a non-null pointer is owned.
        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_ptr);
        }

        //! Dereference the owned object.
        //!
        //! Preconditions:
        //! - get() != nullptr
        [[nodiscard]] element_type& operator*() const noexcept
        {
            GEOMETRIX_ASSERT(get() != nullptr);
            return *m_ptr;
        }

        //! Access members of the owned object.
        //!
        //! Preconditions:
        //! - get() != nullptr
        [[nodiscard]] pointer operator->() const noexcept
        {
            GEOMETRIX_ASSERT(get() != nullptr);
            return m_ptr.get();
        }

        //! Release ownership without destroying the resource.
        //!
        //! After this call, the caller becomes responsible for cleanup.
        [[nodiscard]] pointer release() noexcept
        {
            return m_ptr.release();
        }

        //! Replace the owned pointer and keep the existing deleter.
        void reset(pointer p = nullptr) noexcept
        {
            m_ptr.reset(p);
        }

        //! Replace both the owned pointer and the deleter.
        //!
        //! Use this when adopting a new resource with a different destruction
        //! policy.
        void reset(pointer p, deleter_type d) noexcept
        {
            GEOMETRIX_ASSERT(d != nullptr);
            m_ptr = unique_type(p, d);
        }

        //! Swap with another owned_ptr.
        void swap(owned_ptr& other) noexcept
        {
            m_ptr.swap(other.m_ptr);
        }

        //! Return the underlying std::unique_ptr representation.
        //!
        //! This is an escape hatch for APIs that explicitly require it.
        [[nodiscard]] unique_type&& into_unique() && noexcept
        {
            return std::move(m_ptr);
        }

        //! Construct from the underlying std::unique_ptr representation.
        [[nodiscard]] static owned_ptr from_unique(unique_type&& ptr) noexcept
        {
            owned_ptr out;
            out.m_ptr = std::move(ptr);
            return out;
        }

    private:
        unique_type m_ptr;
    };

    template<class T>
    [[nodiscard]] bool operator==(const owned_ptr<T>& p, std::nullptr_t) noexcept
    {
        return !p;
    }

    template<class T>
    [[nodiscard]] bool operator==(std::nullptr_t, const owned_ptr<T>& p) noexcept
    {
        return !p;
    }

    template<class T>
    [[nodiscard]] bool operator!=(const owned_ptr<T>& p, std::nullptr_t) noexcept
    {
        return static_cast<bool>(p);
    }

    template<class T>
    [[nodiscard]] bool operator!=(std::nullptr_t, const owned_ptr<T>& p) noexcept
    {
        return static_cast<bool>(p);
    }

    template<class T, class U>
    [[nodiscard]] bool operator==(const owned_ptr<T>& a, const owned_ptr<U>& b) noexcept
    {
        return a.get() == b.get();
    }

    template<class T, class U>
    [[nodiscard]] bool operator!=(const owned_ptr<T>& a, const owned_ptr<U>& b) noexcept
    {
        return a.get() != b.get();
    }

    template<class T, class U>
    [[nodiscard]] bool operator<(const owned_ptr<T>& a, const owned_ptr<U>& b) noexcept
    {
        return a.get() < b.get();
    }

    template<class T, class U>
    [[nodiscard]] bool operator<=(const owned_ptr<T>& a, const owned_ptr<U>& b) noexcept
    {
        return a.get() <= b.get();
    }

    template<class T, class U>
    [[nodiscard]] bool operator>(const owned_ptr<T>& a, const owned_ptr<U>& b) noexcept
    {
        return a.get() > b.get();
    }

    template<class T, class U>
    [[nodiscard]] bool operator>=(const owned_ptr<T>& a, const owned_ptr<U>& b) noexcept
    {
        return a.get() >= b.get();
    }

    template<class T>
    void swap(owned_ptr<T>& a, owned_ptr<T>& b) noexcept
    {
        a.swap(b);
    }

    //! Adopt an existing pointer using delete as the destruction policy.
    //!
    //! This is appropriate for pointers obtained from:
    //!
    //!     new T(...)
    //!
    //! It is not appropriate for resources that require free, fclose,
    //! CloseHandle, SDL_Destroy..., or similar APIs.
    template<class T>
    [[nodiscard]] owned_ptr<T> adopt_owned(T* p) noexcept
    {
        return owned_ptr<T>(p, &detail::default_delete_fn<T>);
    }

    //! Adopt an existing pointer or resource using a custom stateless deleter.
    //!
    //! The deleter must be convertible to:
    //!
    //!     void(*)(T*) noexcept
    //!
    //! This means:
    //! - function pointers are allowed
    //! - non-capturing lambdas are allowed
    //! - capturing lambdas are not allowed
    //!
    //! Example:
    //!
    //!     auto file = stk::adopt_owned(
    //!         std::fopen("x.txt", "r"),
    //!         +[](FILE* fp) noexcept
    //!         {
    //!             if(fp)
    //!                 std::fclose(fp);
    //!         });
    template<class T, class D>
    [[nodiscard]] owned_ptr<T> adopt_owned(T* p, D&& deleter) noexcept
    {
        static_assert(detail::deleter_convertible_v<D, T>,
            "deleter must be convertible to void(*)(T*) noexcept; "
            "use a non-capturing lambda and prefix it with unary + if needed");

        return owned_ptr<T>(
            p,
            static_cast<detail::owned_deleter_t<T>>(std::forward<D>(deleter)));
    }

    //! Allocate with new T(...) and use delete as the destruction policy.
    template<class T, class... Args>
    [[nodiscard]] owned_ptr<T> make_owned(Args&&... args)
    {
        static_assert(!std::is_void_v<T>,
            "make_owned<void>(...) is ill-formed because void cannot be constructed");

        return owned_ptr<T>(
            new T(std::forward<Args>(args)...),
            &detail::default_delete_fn<T>);
    }

    //! Allocate with new T(...) and use a custom stateless deleter.
    //!
    //! This is mainly useful when allocation still uses new T(...), but cleanup
    //! needs a different stateless destruction function.
    //!
    //! For foreign handles and API-owned resources, prefer adopt_owned(...).
    template<class T, class D, class... Args>
    [[nodiscard]] owned_ptr<T> make_owned_with_deleter(D&& deleter, Args&&... args)
    {
        static_assert(!std::is_void_v<T>,
            "make_owned_with_deleter<void>(...) is ill-formed because void cannot be constructed");
        static_assert(detail::deleter_convertible_v<D, T>,
            "deleter must be convertible to void(*)(T*) noexcept; "
            "use a non-capturing lambda and prefix it with unary + if needed");

        return owned_ptr<T>(
            new T(std::forward<Args>(args)...),
            static_cast<detail::owned_deleter_t<T>>(std::forward<D>(deleter)));
    }
} //! namespace stk
