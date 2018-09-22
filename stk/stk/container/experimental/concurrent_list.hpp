//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/utility/boost_unique_ptr.hpp>
#include <stk/container/atomic_markable_ptr.hpp>
#include <stk/container/experimental/qsbr_allocator.hpp>
#include <memory>

namespace stk {

    template <typename T, typename Alloc = stk::qsbr_allocator<T>>
    class concurrent_list
    {
    public:

        struct node
        {
            friend class concurrent_list<T>;

            template <typename... U>
            node(U&&... args)
                : data(std::forward<U>(args)...)
            {}

            ~node() = default;

            T data;

        private:
            atomic_markable_ptr<node> next;
        };

    private:

        static node* tail()
        {
            return nullptr;
        }

        using node_ptr = node*;
        using node_alloc_t = typename std::allocator_traits<Alloc>::template rebind_alloc<node>;
        using alloc_traits = std::allocator_traits<node_alloc_t>;
        template <typename... Args>
        node_ptr create_node(Args&&... a) const
        {
            auto ptr = alloc_traits::allocate(m_alloc, 1);
            alloc_traits::construct(m_alloc, ptr, std::forward<Args>(a)...);
            return ptr;
        }

        void destroy_node(node_ptr p) const
        {
            alloc_traits::destroy(m_alloc, p);//! this ought to be a no-op.. but here if there is something done.
            alloc_traits::deallocate(m_alloc, p, 1);//! this should use safe reclamation.
        }

        using ptr_mark_pair = std::tuple<node*, bool>;

    public:

        using value_type = T;
        using reference = typename std::add_reference<value_type>::type;
        using const_reference = typename std::add_const<reference>::type;

        concurrent_list() = default;
        concurrent_list(const Alloc& alloc)
            : m_alloc(alloc)
            , m_head(create_node())
        {
            m_head.get_ptr()->next.set_ptr(tail());
        }

        ~concurrent_list()
        {
            auto pCurr = m_head.get_ptr();
            while(pCurr)
            {
                auto pNext = get_next(pCurr);
                destroy_node(pCurr);
                pCurr = pNext;
            }
        }

        concurrent_list(concurrent_list const& other) = delete;
        concurrent_list& operator =(concurrent_list const& other) = delete;

        std::size_t size() const
        {
            return m_size.load();
        }

        //! O(1)
        node_ptr push_front(const T& v)
        {
            return push_front_impl(v);
        }

        //! amortized O(1)
        template <typename... Args>
        node_ptr emplace_front(Args&&... v)
        {
            return push_front_impl(std::forward<Args>(v)...);
        }

        void pop_front()
        {
            erase_front();
        }

        value_type front() const
        {
            auto pFront = get_front();
            GEOMETRIX_ASSERT(pFront != nullptr);

            return pFront->data;
        }

        //! amortized O(N)
        node_ptr push_back(const T& v)
        {
            return push_back_impl(v);
        }

        template <typename... Args>
        node_ptr emplace_back(Args&&... v)
        {
            return push_back_impl(std::forward<Args>(v)...);
        }

        void pop_back()
        {
            erase_back();
        }

        value_type back() const
        {
            auto pBack = get_back();
            GEOMETRIX_ASSERT(pBack!= nullptr);
            return pBack->data;
        }

        template <typename Predicate>
        bool find(Predicate&& pred)
        {
            node_ptr right, left;
            right = search(left, std::forward<Predicate>(pred));
            return right != tail() && pred(right);
        }

        bool erase(node_ptr pNode)
        {
            node_ptr right, rightNext, left;
            bool marked;

            while(true)
            {
                right = search(left, [pNode](node_ptr pOther) { return pNode == pOther; });
                if(right)
                {
                    std::tie(rightNext, marked) = right->next.get();
                    if(!marked)
                    {
                        //! Mark for deletion.
                        if (right->next.compare_exchange_strong(rightNext, marked, rightNext, true))
                        {
                            //destroy_node(right);
                            break;
                        }
                    }
                }
                else
                {
                    return false;
                }
            }

            if (left->next.compare_exchange_strong(right, marked, rightNext, false))
            {
                destroy_node(right);
                //! someone else marked.. help remove.
                ///*right =*/ search(left, [right](node_ptr pOther)
                //{
                //    return right == pOther;
                //});
            }

            return true;
        }

        template <typename Predicate>
        bool erase_if(Predicate&& pred)
        {
            node_ptr right, rightNext, left;
            bool marked;

            while(true)
            {
                right = search(left, std::forward<Predicate>(pred));
                if(right)
                {
                    std::tie(rightNext, marked) = right->next.get();
                    if(!marked)
                    {
                        //! Mark for deletion.
                        if (right->next.compare_exchange_strong(rightNext, marked, rightNext, true))
                        {
                            //destroy_node(right);
                            break;
                        }
                    }
                }
                else
                {
                    return false;
                }
            }

            if (left->next.compare_exchange_strong(right, marked, rightNext, false))
            {
                destroy_node(right);
                //! someone else marked.. help remove.
                ///*right =*/ search(left, std::forward<Predicate>(pred));
            }

            return true;
        }

    private:

        node* get_next(node* n) const
        {
            return n->next.get_ptr();
        }

        template <typename... Args>
        node_ptr push_back_impl(Args&&... args)
        {
            auto deleter = [this](node* p) { destroy_node(p); };
            std::unique_ptr<node, decltype(deleter)> pNew{ create_node(std::forward<Args>(args)...), deleter };
            pNew->next.set(tail(), false);
            node* pDesired = pNew.get();
            bool mDesired = false;
            node* pExpected = tail();
            bool mExpected = false;
            ptr_mark_pair right;
            node_ptr left;

            while(true)
            {
                left = get_back();
                if(left->next.compare_exchange_weak(pExpected, mExpected, pDesired, mDesired))
                {
                    pNew.release();
                    m_size.fetch_add(1);
                    return pDesired;
                }
            }
        }

        node* get_back() const
        {
            node* left = m_head.get_ptr();
            ptr_mark_pair leftNext;
            while(true)
            {
                ptr_mark_pair t = m_head.get();
                ptr_mark_pair tNext = std::get<0>(t)->next.get();

                //! Find back node.
                do
                {
                    if(!std::get<1>(tNext))
                    {
                        left = std::get<0>(t);
                        leftNext = tNext;
                    }
                    else
                    {
                        //! snip it.
                        node* pExpected, *pDesired;
                        bool mExpected, mDesired;
                        std::tie(pExpected, mExpected) = t;
                        std::tie(pDesired, mDesired) = pExpected->next.get();
                        if(left->next.compare_exchange_strong(pExpected, mExpected, pDesired, mDesired))
                            destroy_node(std::get<0>(t));
                        else
                            continue;
                        tNext = ptr_mark_pair(pDesired, mDesired);
                    }

                    t = tNext;
                    if(std::get<0>(t) == tail())
                        break;
                    tNext = std::get<0>(t)->next.get();
                }
                while (true);

                //! Check nodes are adjacent.
                if(std::get<0>(leftNext) == std::get<0>(t))
                    return left;
            }
        }

        void erase_back() const
        {
            node* left = m_head.next.get_ptr();
            node* pExpected;
            bool mExpected;
            ptr_mark_pair leftNext;
            while(true)
            {
                ptr_mark_pair t = m_head.get();
                ptr_mark_pair tNext = std::get<0>(t)->next.get();

                //! Check if it's empty.
                if (std::get<0>(tNext) == tail())
                    return;

                //! Find back node.
                do
                {
                    if(!std::get<1>(tNext))
                    {
                        left = std::get<0>(t);
                        leftNext = tNext;
                    }
                    else
                    {
                        //! snip it.
                        node* *pDesired;
                        bool mDesired;
                        std::tie(pExpected, mExpected) = t;
                        std::tie(pDesired, mDesired) = pExpected->next.get();
                        if(left->next.compare_exchange_strong(pExpected, mExpected, pDesired, mDesired))
                            destroy_node(std::get<0>(t));
                        else
                            continue;
                        tNext = ptr_mark_pair(pDesired, mDesired);
                    }

                    t = tNext;
                    if(std::get<0>(t) == tail())
                        break;
                    tNext = std::get<0>(t)->next.get();
                }
                while (true);

                //! Check nodes are adjacent.
                if (std::get<0>(leftNext) == std::get<0>(t) && !std::get<1>(leftNext))
                {
                    //! Mark for deletion.
                    std::tie(pExpected, mExpected) = leftNext;
                    if (left && left->next.compare_exchange_strong(pExpected, mExpected, std::get<0>(leftNext), true))
                    {
                        destroy_node(left);
                        return;
                    }
                }
            }
        }

        node* get_front() const
        {
            while (true)
            {
                auto t = m_head.get_ptr()->next.get_ptr();
                if (t == tail())
                    return nullptr;
                ptr_mark_pair tNext = t->next.get();

                //! Find front node.
                if (!std::get<1>(tNext))
                {
                    return t;
                }
                else
                {
                    //! snip it.
                    node* pExpected = t, *pDesired;
                    bool mExpected = false, mDesired;
                    std::tie(pDesired, mDesired) = pExpected->next.get();
                    if (m_head.get_ptr()->next.compare_exchange_strong(pExpected, mExpected, pDesired, false))
                        destroy_node(t);
                }
            }
        }

        void erase_front() const
        {
            while (true)
            {
                auto t = m_head.get_ptr()->next.get_ptr();
                if (t == tail())
                    return;
                ptr_mark_pair tNext = t->next.get();

                //! Find front node.
                auto pExpected = std::get<0>(tNext);
                auto mExpected = false;
                if (!std::get<1>(tNext))
                {
                    if(t->next.compare_exchange_strong(pExpected, mExpected, pExpected, true))
                    {
                        get_front();//! perform the deletion.
                        return;
                    }
                }
                else
                {
                    //! snip it.
                    node* pExpected = t, *pDesired;
                    bool mExpected = false, mDesired;
                    std::tie(pDesired, mDesired) = pExpected->next.get();
                    if (m_head.get_ptr()->next.compare_exchange_strong(pExpected, mExpected, pDesired, mDesired))
                        destroy_node(t);
                }
            }
        }

        template <typename... Args>
        node_ptr push_front_impl(Args&&... args)
        {
            auto deleter = [this](node* p) { destroy_node(p); };
            std::unique_ptr<node, decltype(deleter)> pNew{ create_node(std::forward<Args>(args)...), deleter };
            node* pExpected;
            bool mExpected;
            node_ptr left;

            while(true)
            {
                pExpected = get_front();
                mExpected = false;
                pNew->next.set(pExpected, false);
                if(m_head.get_ptr()->next.compare_exchange_weak(pExpected, mExpected, pNew.get(), false))
                {
                    m_size.fetch_add(1);
                    return pNew.release();
                }
            }
        }

        template <typename Predicate>
        node_ptr search(node_ptr& left, Predicate&& pred) const
        {
            node_ptr right = nullptr;
            ptr_mark_pair leftNext;
            node* pExpected;
            bool mExpected;
            while(true)
            {
                ptr_mark_pair t = m_head.get();
                ptr_mark_pair tNext = std::get<0>(t)->next.get();

                do
                {
                    if(!std::get<1>(tNext))
                    {
                        left = std::get<0>(t);
                        leftNext = tNext;
                    }

                    t = tNext;
                    if(std::get<0>(t) == tail())
                        break;
                    tNext = std::get<0>(t)->next.get();
                }
                while (std::get<1>(tNext) || !pred(std::get<0>(t)));

                right = std::get<0>(t);

                //! Check nodes are adjacent.
                if(std::get<0>(leftNext) == right)
                {
                    if (right != tail() && std::get<1>(t))
                        continue;
                    else
                        return right;
                }

                //! Remove marked nodes.
                std::tie(pExpected, mExpected) = leftNext;
                if (left->next.compare_exchange_strong(pExpected, mExpected, right, false))
                    continue;
                else
                    return right;
            }
        }

        mutable node_alloc_t m_alloc;
        mutable atomic_markable_ptr<node> m_head;
        std::atomic<std::size_t> m_size;

    };
}//! namespace stk;
