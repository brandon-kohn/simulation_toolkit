//
//! Copyright © 2023
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <new>
#include <cstddef>
#include <type_traits>



namespace stk {
#include <memory>
#include <new>
#include <cstddef>
#include <type_traits>

	template <typename T, std::size_t Alignment = std::hardware_destructive_interference_size>
	struct aligned_allocator
	{
		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using void_pointer = void*;
		using const_void_pointer = const void*;
		using size_type = std::size_t;
		using difference_type = std::ptrdiff_t;

		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;

		template <class U>
		struct rebind
		{
			using other = aligned_allocator<U, Alignment>;
		};

		aligned_allocator() noexcept = default;

		template <class U>
		aligned_allocator( const aligned_allocator<U, Alignment>& ) noexcept
		{}

		T* allocate( std::size_t n )
		{
			if( n > std::size_t( -1 ) / sizeof( T ) )
				throw std::bad_alloc();

			return static_cast<T*>( ::operator new[]( n * sizeof( T ), std::align_val_t( Alignment ) ) );
		}

		void deallocate( T* ptr, std::size_t ) noexcept
		{
			::operator delete[]( ptr, std::align_val_t( Alignment ) );
		}

		bool operator==( const aligned_allocator& ) const noexcept { return true; }
		bool operator!=( const aligned_allocator& ) const noexcept { return false; }
	};
} //! namespace stk;

