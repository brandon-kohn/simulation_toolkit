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

template <typename T>
class locked_list
{
    struct node
    {
        std::mutex m;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        
        node() 
            : next()
        {}
        
        node(T const& value)
            : data(std::make_shared<T>(value))
        {}
    };
    
    node head;
    
public:

    locked_list() = default;
    ~locked_list()
    {
        remove_if([](const node&){ return true; });
    }
    
    locked_list(locked_list const& other) = delete;
    locked_list& operator =(locked_list const& other) = delete;
    
    void push_front(T const& value)
    {
        auto new_node = boost::make_unique<node>(value);
        auto lk = std::lock_guard<std::mutex>{head.m};
        new_node->next = std::move(head.next);
        head.next = std::move(new_node);
    }
    
    template <typename Fn>
    void for_each(Fn f)
    {
        auto current = &head;
        auto lk = std::unique_lock<std::mutex>{head.m};
        while(node* const next = current->next.get())
        {
            auto next_lk = std::unique_lock<std::mutex>{next->m};
            lk.unlock();
            f(*next->data);
            current = next;
            lk = std::move(next_lk);
        }
    }
    
    template <typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate p)
    {
        auto current = &head;
        auto lk = std::unique_lock<std::mutex>{head.m};
        while(auto next = current->next.get())
        {
            auto next_lk = std::unique_lock<std::mutex>{next->m};
            lk.unlock();
            if(p(*next->data))
            {
                return next->data;
            }
            current = next;
            lk = std::move(next_lk);
        }
        
        template <typename Predicate>
        void remove_if(Predicate p)
        {
            auto current = &head;
            auto lk = std::unique_lock<std::mutex>{head.m};
            while(auto next = current->next.get())
            {
                auto next_lk = std::unique_lock<std::mutex>{next->m};
                if(p(*next->data)
                {
                    auto old_next = std::unique_ptr<node>{ std::move{ current->next} };
                    current->next = std::move(next->next);
                    next_lk.unlock();
                }
                else
                {
                    lk.unlock();
                    current = next;
                    lk = std::move(next_lk);
                }
            }
        }
    }
};

#endif//! STK_CONTAINER_LOCKED_LIST_HPP
