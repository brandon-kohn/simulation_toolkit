#ifndef STK_THREAD_CLH_LOCK_HPP
#define STK_THREAD_CLH_LOCK_HPP

#include <stk/thread/thread_specific.hpp>
#include <stk/thread/spin_lock_wait_strategies.hpp>
#include <stk/thread/cache_line_padding.hpp>
#include <atomic>

namespace stk { namespace thread {

    template <typename WaitPolicy = null_wait_policy>
    class thread_specific_clh_lock
    { 
        struct node
        {
            node() = default;
            alignas(STK_CACHE_LINE_SIZE) std::atomic<bool> locked = false;
        };

        using unode_ptr = std::unique_ptr<node>;
        alignas(STK_CACHE_LINE_SIZE) std::atomic<node*> m_tail;
        thread_specific<node*> m_pred;
        thread_specific<unode_ptr> m_node;

   public:

        thread_specific_clh_lock() 
           : m_tail{ new node }
           , m_node([](){ return new node{};})
           , m_pred([](){ return nullptr; })
        {}

        ~thread_specific_clh_lock()
        {
            auto ptr = m_tail.exchange(nullptr);//! dont' delete under contention.
            delete ptr;
        }
        
        void lock()
        {
            WaitPolicy pol{};
            node* qnode = m_node->get();
            qnode->locked = true;
            node* pred = m_tail.exchange(qnode);
            m_pred = pred;
            while(pred->locked.load(std::memory_order_relaxed))
                pol(); 
        }

        void unlock()
        {
            auto& qnode = *m_node;
            qnode->locked.store(false, std::memory_order_relaxed);
            qnode.reset(*m_pred);
        }

   };
}}//! namespace stk::thread;

#endif//! STK_THREAD_CLH_LOCK_HPP

