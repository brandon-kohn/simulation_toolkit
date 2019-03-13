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

    struct seq_executor
    {
        template <typename Container, typename Fn>
        void for_each(Container&& c, Fn&& f)
        {
            for (auto& c : c)
                f(c);
        }

        template <typename Fn>
        void execute(Fn&& f)
        {
            f();
        }
    };

}//! namespace stk;
