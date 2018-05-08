#pragma once
#include <chrono>

template <typename Duration>
inline void synthetic_work(Duration const& work)
{
    if (work == std::chrono::nanoseconds(0))
        return;

    auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {
        if ((std::chrono::high_resolution_clock::now() - start) >= work)
            return;
    }
}

