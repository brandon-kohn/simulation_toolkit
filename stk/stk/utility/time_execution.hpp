#pragma once
#include <chrono>

namespace stk {
    
    template <typename Fn, typename ... Ts>
    inline std::chrono::duration<double> time_execution(Fn&& fn, Ts&&... args)
    {
        auto start = std::chrono::high_resolution_clock::now();
        fn(std::forward<Ts>(args)...);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>{ end - start };
    }

}//! namespace stk;
 
