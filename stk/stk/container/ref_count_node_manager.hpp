//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//! Based on:
//! Multiple improvements were included from studying folly's ConcurrentSkipList:
/*
* Copyright 2017 Facebook, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
// @author: Xin Liu <xliux@fb.com>
//
#pragma once

#include <memory>

namespace stk { namespace detail {

    template<typename Node, typename Alloc=std::allocator<Node>>
    class ref_count_node_manager
    {
    public:

        using allocator_type = Alloc;
        using node_type = Node;
        using node_ptr = Node*;

        ref_count_node_manager(allocator_type al = allocator_type())
            : m_nodeAllocator(al)
        {}

        ~ref_count_node_manager()
        {
            //! There should not be any iterators checked out at this point.
            GEOMETRIX_ASSERT(m_refCounter.load(std::memory_order_relaxed) == 0);
            if (m_nodes)
                for (auto pNode : *m_nodes)
                    destroy_node(pNode);
        }

        template <typename Data>
        node_ptr create_node(Data&& v)
        {
            auto pNode = m_nodeAllocator.allocate(1);
            new(pNode) node_type(std::forward<Data>(v));
            return pNode;
        }

        void register_node_to_delete(node_ptr pNode)
        {
            auto lk = std::unique_lock<std::mutex>{ m_mtx };
            if (m_nodes)
                m_nodes->push_back(pNode);
            else
                m_nodes = boost::make_unique<std::vector<node_ptr>>(1, pNode);

            m_hasNodes.store(true, std::memory_order_relaxed);
        }

        void add_checkout()
        {
            m_refCounter.fetch_add(1, std::memory_order_relaxed);
        }

        void remove_checkout()
        {
            STK_MEMBER_SCOPE_EXIT
            (
                GEOMETRIX_ASSERT(m_refCounter > 0);
                m_refCounter.fetch_sub(1, std::memory_order_relaxed);
            );

            if (m_hasNodes.load(std::memory_order_relaxed) && m_refCounter.load(std::memory_order_relaxed) < 1)
            {
                std::unique_ptr<std::vector<node_ptr>> toDelete;
                {
                    auto lk = std::unique_lock<std::mutex>{ m_mtx };
                    if (m_nodes.get() && m_refCounter.load(std::memory_order_relaxed) < 1)
                    {
                        toDelete.swap(m_nodes);
                        m_hasNodes.store(false, std::memory_order_relaxed);
                    }
                }
                for (auto pNode : *toDelete)
                    destroy_node(pNode);
            }
        }

        void destroy_node(node_ptr pNode)
        {
            pNode->~node();
            m_nodeAllocator.deallocate(pNode, 1);
        }

    private:

        allocator_type                         m_nodeAllocator;
        std::atomic<std::size_t>               m_refCounter{ 0 };
        std::unique_ptr<std::vector<node_ptr>> m_nodes;
        std::atomic<bool>                      m_hasNodes{ false };
        std::mutex                             m_mtx;

    };
}}//! namespace stk::detail;

