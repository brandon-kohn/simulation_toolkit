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
            , m_nodes(nullptr)
        {}

        ~ref_count_node_manager()
        {
            //! There should not be any iterators checked out at this point.
            GEOMETRIX_ASSERT(m_refCounter.load(std::memory_order_relaxed) == 0);
            auto pNodes = m_nodes.load(std::memory_order_relaxed);
            if (pNodes)
                for (auto pNode : *pNodes)
                    destroy_node(pNode);
            delete pNodes;
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
            //! Add a checkout so that there is no chance swapping will happen while this call is in effect.
            m_refCounter.fetch_add(1, std::memory_order_acquire);
            STK_MEMBER_SCOPE_EXIT
            (
                GEOMETRIX_ASSERT(m_refCounter > 0);
                m_refCounter.fetch_sub(1, std::memory_order_relaxed);
            );

            auto lk = std::unique_lock<std::mutex>{ m_mtx };
            auto pNodes = m_nodes.load(std::memory_order_relaxed);
            if (pNodes)
            {
                pNodes->push_back(pNode);
            }
            else
            {
                m_nodes.store( new std::vector<node_ptr>(1, pNode) );
            }
        }

        void add_checkout()
        {
            m_refCounter.fetch_add(1, std::memory_order_relaxed);
        }

        void remove_checkout()
        {
            GEOMETRIX_ASSERT(m_refCounter > 0);
            STK_MEMBER_SCOPE_EXIT( m_refCounter.fetch_sub(1, std::memory_order_relaxed); );

            //auto lk = std::unique_lock<std::mutex>{ m_mtx };
            if (BOOST_UNLIKELY(m_refCounter.load(std::memory_order_relaxed) == 1))
            {
                std::unique_ptr<std::vector<node_ptr>> toDelete;
                auto pNodes = m_nodes.exchange(nullptr, std::memory_order_acquire);
                if(pNodes)
                {
                    toDelete.reset(pNodes);
                    if (m_refCounter.load(std::memory_order_relaxed) == 1)
                    {
                        for (auto pNode : *toDelete)
                            destroy_node(pNode);
                    }
                    else
                    {
                        for (auto pNode : *toDelete)
                            register_node_to_delete(pNode);
                    }
                }
            }
        }

        void destroy_node(node_ptr pNode)
        {
            pNode->~Node();
            m_nodeAllocator.deallocate(pNode, 1);
        }

    private:

        allocator_type                         m_nodeAllocator;
        std::atomic<std::size_t>               m_refCounter{ 0 };
        std::atomic<std::vector<node_ptr>*>    m_nodes;
        std::mutex                             m_mtx;

    };
}}//! namespace stk::detail;

