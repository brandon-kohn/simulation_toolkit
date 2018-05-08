//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
#ifndef STK_THREAD_BARRIER_HPP
#define STK_THREAD_BARRIER_HPP

#include <cstddef>
#include <condition_variable>
#include <mutex>
#include <boost/config.hpp>

#include <geometrix/utility/assert.hpp>

namespace stk { namespace thread {

	template <typename Mutex = std::mutex>
    class barrier
    {
    public:

		using mutex_type = Mutex;
    
        explicit barrier(std::size_t initial) 
        : initial_{ initial }
        , current_{ initial_ } 
        {
			if (BOOST_UNLIKELY(0 == initial)) 
			{
				throw std::invalid_argument{ "zero initial barrier count" };
			}
        }

        barrier( barrier const&) = delete;
        barrier & operator=( barrier const&) = delete;

        bool wait() 
        {
            std::unique_lock< mutex_type > lk{ mtx_ };
            const std::size_t cycle = cycle_;
            if (0 == --current_)
            {
                ++cycle_;
                current_ = initial_;
                lk.unlock(); // no pessimization
                cond_.notify_all();
                return true;
            }
            else
            {
                cond_.wait(lk, [this, cycle]{ return cycle != cycle_; });
            }
           
            return false;
        }
        
    private:
    
        std::size_t					initial_;
        std::size_t					current_;
        std::size_t					cycle_{ 0 };
        mutex_type					mtx_;
        std::condition_variable_any cond_;

    };

}}//! namespace stk::thread;

#endif // STK_THREAD_BARRIER_HPP
