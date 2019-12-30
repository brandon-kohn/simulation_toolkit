#pragma once
#include <chrono>

namespace stk {

    template <typename Duration>
    inline void synthetic_work(Duration const& duration)
    {
        auto start = std::chrono::high_resolution_clock::now();
        while ((std::chrono::high_resolution_clock::now() - start) < duration);
    }

}//! namespace stk;
 
