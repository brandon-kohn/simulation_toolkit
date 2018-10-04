#pragma once


#ifndef STK_NO_CXX11_THREAD_LOCAL
#include <stack>
#else
#include <boost/thread/thread.hpp>
#endif

namespace stk { namespace this_thread {

#ifndef STK_NO_CXX11_THREAD_LOCAL
    template <typename Fn>
    inline void on_thread_exit(Fn&& fn)
    {
        thread_local struct raii_tl
        {
            std::stack<std::function<void()>> callbacks;

            ~raii_tl()
            {
                for (auto& callback : callbacks)
                    callback();
            }
        } instance;

        instance.callbacks.emplace(std::forward<Fn>(fn));
    }
#else
    template <typename Fn>
    inline void on_thread_exit(Fn&& fn)
    {
        boost::this_thread::at_thread_exit(std::forward<Fn>(fn));
    }

#endif

}}//! namespace stk::this_thread;
