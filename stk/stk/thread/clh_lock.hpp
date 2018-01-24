#ifndef STK_THREAD_CLH_LOCK_HPP
#define STK_THREAD_CLH_LOCK_HPP

#include <stk/thread/thread_specific.hpp>
#include <atomic>

namespace stk { namespace thread {

   class clh_lock
   { 

       struct node
       {
           node() = default;
           bool locked = false;
       };
       std::atomic<node*> m_tail;
       thread_specific<node*> m_pred;
       thread_specific<node*> m_node;

   public:

       clh_lock() 
           : m_tail{ new node }
           , m_node(  
         

   };
}}//! namespace stk::thread;

#endif//! STK_THREAD_CLH_LOCK_HPP

