//
//! Copyright © 2019
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

namespace stk {

    template <typename Pool>
    struct thread_pool_executor 
    {
        thread_pool_executor(Pool& p)
            : m_pool(p)
        {}

        template <typename Container, typename Fn>
        void for_each(Container&& c, Fn&& f)
        {
            m_pool.parallel_for(std::forward<Container>(c), std::forward<Fn>(f));
        }

        template <typename Fn>
        void execute(Fn&& f)
        {
            m_pool.send(std::forward<Fn>(f));
        }

    private:

        Pool& m_pool;
    };

}//! namespace stk;
