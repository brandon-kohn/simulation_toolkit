#ifndef STK_CONTAINTER_LOCK_FREE_HASH_SET_HPP
#define STK_CONTAINTER_LOCK_FREE_HASH_SET_HPP

#include <stk/container/atomic_stampable_ptr.hpp>
#pragma once

#define STK_REVERSE_BYTE(x) ((std::uint64_t)((((((uint32_t)(x)) * 0x0802LU & 0x22110LU) | (((uint32_t)(x)) * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16) & 0xff))
# define STK_REVERSE64(x) ((STK_REVERSE_BYTE((((std::uint64_t)(x))) & 0xff) << 56) |\
                     (STK_REVERSE_BYTE((((std::uint64_t)(x)) >> 8) & 0xff) << 48)  |\
                     (STK_REVERSE_BYTE((((std::uint64_t)(x)) >> 16) & 0xff) << 40) |\
                     (STK_REVERSE_BYTE((((std::uint64_t)(x)) >> 24) & 0xff) << 32) |\
                     (STK_REVERSE_BYTE((((std::uint64_t)(x)) >> 32) & 0xff) << 24) |\
                     (STK_REVERSE_BYTE((((std::uint64_t)(x)) >> 40) & 0xff) << 16) |\
                     (STK_REVERSE_BYTE((((std::uint64_t)(x)) >> 48) & 0xff) << 8)  |\
                     (STK_REVERSE_BYTE((((std::uint64_t)(x)) >> 56) & 0xff) << 0))  \
/***/

namespace stk {

    namespace detail
    {
        template <typename T>
        struct node
        {
            std::uint64_t key; 
            T value;
            atomic_stampable_ptr<node> next;
        };
        
        template <typename T>
        class bucket_list
        {
            static const std::int64_t hi_mask = 0x0080000;
            static const std::int64_t mask = 0x00FFFFFF;

        public:

        };
    }//! namespace detail;

template <typename T>
class lock_free_hash_set
{

public:

private:

};

}//!namespace stk;

#endif STK_CONTAINTER_LOCK_FREE_HASH_SET_HPP

