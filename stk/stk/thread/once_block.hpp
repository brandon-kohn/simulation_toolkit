#ifndef STK_THREAD_ONCEBLOCK_HPP
#define STK_THREAD_ONCEBLOCK_HPP
#pragma once

#include <atomic>
#include <boost/preprocessor/cat.hpp>

//! \def STK_ONCE_BLOCK()
//! \brief This macro can be used to define the entry to a block which will only be run once even in the context of multiple threads.
#define STK_ONCE_BLOCK()                                                          \
    static std::atomic_bool BOOST_PP_CAT(stk_once_block_sentinel,__LINE__)(false);\
    bool BOOST_PP_CAT(stk_once_block_expected,__LINE__) = false;                  \
    if(BOOST_PP_CAT(stk_once_block_sentinel,__LINE__).compare_exchange_strong(    \
                                   BOOST_PP_CAT(stk_once_block_expected,__LINE__) \
                                 , true, std::memory_order_seq_cst))              \
/***/

#endif // STK_THREAD_ONCEBLOCK_HPP
