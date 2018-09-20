//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_CONTAINER_LOCKED_LIST_HPP
#define STK_CONTAINER_LOCKED_LIST_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <stk/utility/boost_unique_ptr.hpp>
#include <boost/optional.hpp>
#include <mutex>
#include <memory>

namespace stk {

    template <typename T, typename Mutex = std::mutex>
    class locked_list
    {
        using mutex_type = Mutex;
        struct node
        {
            mutable mutex_type m;
            std::shared_ptr<T> data;
            std::unique_ptr<node> next;

            node() = default;

            template <typename... U>
            node(U&&... args)
                : data(std::make_shared<T>(std::forward<U>(args)...))
            {}
        };

        node head;

    public:

        locked_list() = default;
        ~locked_list()
        {
            remove_if([](const node&) { return true; });
        }

        locked_list(locked_list const& other) = delete;
        locked_list& operator =(locked_list const& other) = delete;

        //! O(1)
        void push_front(T const& value)
        {
            auto new_node = boost::make_unique<node>(value);
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            new_node->next = std::move(head.next);
            head.next = std::move(new_node);
        }

        void push_front(T&& value)
        {
            auto new_node = boost::make_unique<node>(std::forward<T>(value));
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            new_node->next = std::move(head.next);
            head.next = std::move(new_node);
        }
       
        template <typename... Args>
        void emplace_front(Args&&... args)
        {
            auto new_node = boost::make_unique<node>(std::forward<Args>(args)...);
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            new_node->next = std::move(head.next);
            head.next = std::move(new_node);
        }

        //! O(n)
        void push_back(T const& value)
        {
            auto new_node = boost::make_unique<node>(value);
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            node* next = nullptr;
            auto current = &head;
            while (next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
                lk.unlock();
                current = next;
                lk = std::move(next_lk);
            }

			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &current->m);
			GEOMETRIX_ASSERT(current->next == nullptr);
            current->next = std::move(new_node);
        }

        void push_back(T&& value)
        {
            auto new_node = boost::make_unique<node>(std::forward<T>(value));
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            node* next = nullptr;
            auto current = &head;
            while (next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
                lk.unlock();
                current = next;
                lk = std::move(next_lk);
            }

			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &current->m);
			GEOMETRIX_ASSERT(current->next == nullptr);
            current->next = std::move(new_node);
        }

        template <typename... Args>
        void emplace_back(Args&&... args)
        {
            auto new_node = boost::make_unique<node>(std::forward<Args>(args)...);
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            node* next = nullptr;
            auto current = &head;
            while (next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
                lk.unlock();
                current = next;
                lk = std::move(next_lk);
            }

			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &current->m);
			GEOMETRIX_ASSERT(current->next == nullptr);
            current->next = std::move(new_node);
        }

		//! O(n)
		template <typename Predicate>
		bool add_back(T const& value, Predicate p)
		{
			std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
			auto current = &head;
			while (auto next = current->next.get())
			{
				std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
				lk.unlock();

				if (p(*next->data))
				{
					return false;
				}

				current = next;
				lk = std::move(next_lk);
			}

			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &current->m);
			GEOMETRIX_ASSERT(current->next == nullptr);
			current->next = boost::make_unique<node>(value);
			return true;//added to back.
		}
        
        //! O(n)
        template <typename Predicate>
        bool update_or_add_back(T const& value, Predicate p)
        {
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            auto current = &head;
            while (auto next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
                lk.unlock();
                
                if (p(*next->data))
                {
                    *next->data = value;
                    return true;
                }
                
                current = next;
                lk = std::move(next_lk);
            }

			GEOMETRIX_ASSERT(lk.owns_lock()); 
			GEOMETRIX_ASSERT(lk.mutex() == &current->m);
			GEOMETRIX_ASSERT(current->next == nullptr);
            current->next = boost::make_unique<node>(value);
            return false;//added to back.
        }

        template <typename Fn>
        void for_each(Fn f) const
        {
            auto current = &head;
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            while (node* const next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
                lk.unlock();
                f(*next->data);
                current = next;
                lk = std::move(next_lk);
            }
        }

        template <typename Predicate>
        std::shared_ptr<T> find_first_if(Predicate p) const
        {
            auto current = &head;
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            while (auto next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
                lk.unlock();
                if (p(*next->data))
                {
                    return next->data;
                }
                current = next;
                lk = std::move(next_lk);
            }

            return std::shared_ptr<T>();
        }

        template <typename Predicate>
        boost::optional<T> find(Predicate p) const
        {
            auto current = &head;
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            while (auto next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
				GEOMETRIX_ASSERT(lk.owns_lock());
                lk.unlock();
                if (p(*next->data))
                {
                    return *next->data;
                }
                current = next;
                lk = std::move(next_lk);
            }

            return boost::optional<T>();
        }

        template <typename Predicate>
        void remove_if(Predicate p)
        {
            auto current = &head;
            std::unique_lock<mutex_type> lk{ head.m };
			GEOMETRIX_ASSERT(lk.owns_lock());
			GEOMETRIX_ASSERT(lk.mutex() == &head.m);
            while (auto next = current->next.get())
            {
                std::unique_lock<mutex_type> next_lk{ next->m };
                if (p(*next->data))
                {
                    std::unique_ptr<node> old_next(std::move(current->next));
                    current->next = std::move(next->next);
					GEOMETRIX_ASSERT(next_lk.owns_lock());
                    next_lk.unlock();
                } 
                else
                {
					GEOMETRIX_ASSERT(lk.owns_lock());
                    lk.unlock();
                    current = next;
                    lk = std::move(next_lk);
                }
            }
        }
    };
}//! namespace stk;

#endif//! STK_CONTAINER_LOCKED_LIST_HPP
