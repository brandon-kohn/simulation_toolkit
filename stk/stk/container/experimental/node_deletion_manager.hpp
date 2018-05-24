//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/thread/concurrentqueue.h>
#include <memory>

namespace stk { 

    template<typename Node, typename Alloc=std::allocator<Node>>
    class node_deletion_manager
    {
    public:

		using mutex_type = std::mutex;
        using allocator_type = Alloc;
        using node_type = Node;
        using node_ptr = Node*;

        node_deletion_manager(allocator_type al = allocator_type())
            : m_nodeAllocator(al)
        {}

        ~node_deletion_manager()
        {
            //! There should not be any iterators checked out at this point.
            //GEOMETRIX_ASSERT(m_refCounter.load(std::memory_order_relaxed) == 0);
            //for (auto pNode : m_nodes)
            //    destroy_node(pNode);
			quiesce();
        }

        template <typename... Args>
        node_ptr create_node(Args&&... v)
        {
            auto pNode = m_nodeAllocator.allocate(1);
            new(pNode) node_type(std::forward<Args>(v)...);
            return pNode;
        }

        void register_node_to_delete(node_ptr pNode)
        {
            //auto lk = std::unique_lock<mutex_type>{ m_mtx };
            m_nodes.enqueue(pNode);
        }

        void add_checkout()
        {
            m_refCounter.fetch_add(1, std::memory_order_relaxed);
        }

        void remove_checkout()
        {
            GEOMETRIX_ASSERT(m_refCounter > 0);
            m_refCounter.fetch_sub(1, std::memory_order_relaxed);
        }

        void destroy_node(node_ptr pNode)
        {
            pNode->~Node();
            m_nodeAllocator.deallocate(pNode, 1);
        }

        void quiesce()
        {
            GEOMETRIX_ASSERT(m_refCounter == 0);
            std::vector<node_ptr> toDelete;
            //{
            //    auto lk = std::unique_lock<mutex_type>{ m_mtx };
            //    toDelete.swap(m_nodes);
            //}

			m_nodes.try_dequeue_bulk(std::back_inserter(toDelete), toDelete.max_size());
            for(auto& p : toDelete)
                destroy_node(p);
        }

    private:

		allocator_type                         m_nodeAllocator;
        std::atomic<std::size_t>               m_refCounter{ 0 };
        //mutex_type                             m_mtx;
        moodycamel::ConcurrentQueue<node_ptr>  m_nodes;

    };
}//! namespace stk;

