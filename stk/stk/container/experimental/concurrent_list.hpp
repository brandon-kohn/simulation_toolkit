//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/utility/boost_unique_ptr.hpp>
#include <stk/container/atomic_markable_ptr.hpp>
#include <memory>

namespace stk {

    template <typename T>
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

        static node* tail()
        {
            return nullptr;
        }

        using ptr_mark_pair = std::tuple<node*, bool>;

		using value_type = T;
        using reference = typename std::add_reference<value_type>::type;
        using const_reference = typename std::add_const<reference>::type;
        using node_ptr = node*;

        concurrent_list()
            : m_head(new node())
        {
            m_head.get_ptr()->next.set_ptr(tail());
        }

        ~concurrent_list()
        {
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
							break;
					}
				} 
				else
				{
					return false;
				}
			}

			if (!left->next.compare_exchange_strong(right, marked, rightNext, false)) 
			{
				//! someone else marked.. help remove.
				/*right =*/ search(left, [right](node_ptr pOther)
				{
					return right == pOther;
				});
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
							break;
					}
				} 
				else
				{
					return false;
				}
			}

			if (!left->next.compare_exchange_strong(right, marked, rightNext, false)) 
			{
				//! someone else marked.. help remove.
				/*right =*/ search(left, std::forward<Predicate>(pred));
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
            std::unique_ptr<node> pNew{ new node(std::forward<Args>(args)...) };
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
            node* left = nullptr;
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
                        std::tie(pExpected, mExpected) = tNext;
                        std::tie(pDesired, mDesired) = pExpected->next.get();
                        if(!std::get<0>(t)->next.compare_exchange_strong(pExpected, mExpected, pDesired, mDesired))
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

        node* get_front() const
        {
            while (true) {
                ptr_mark_pair t = m_head.get_ptr()->next.get();
                if (std::get<0>(t) == tail())
                    return nullptr;
                ptr_mark_pair tNext = std::get<0>(t)->next.get();

                //! Find front node.
                while(true)
                {
                    if (!std::get<1>(tNext))
                    {
                        return std::get<0>(t);
                    }
                    else
                    {
                        //! snip it.
                        node* pExpected, *pDesired;
                        bool mExpected, mDesired;
                        std::tie(pExpected, mExpected) = tNext;
                        std::tie(pDesired, mDesired) = pExpected->next.get();
                        if (!std::get<0>(t)->next.compare_exchange_strong(pExpected, mExpected, pDesired, mDesired))
                            continue;
                        tNext = ptr_mark_pair(pDesired, mDesired);
                    }

                    t = tNext;
                    if (std::get<0>(t) == tail())
                        break;
                    tNext = std::get<0>(t)->next.get();
                } 

                return nullptr;
            }
        }

        template <typename... Args>
        node_ptr push_front_impl(Args&&... args)
        {
            std::unique_ptr<node> pNew{ new node(std::forward<Args>(args)...) };
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
        mutable atomic_markable_ptr<node> m_head;
        std::atomic<std::size_t> m_size;

    };
}//! namespace stk;
