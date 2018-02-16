/*  Multi-producer/multi-consumer bounded queue.
 *  Copyright (c) 2010-2011, Dmitry Vyukov. All rights reserved.
 *  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright notice, this list of
 *        conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright notice, this list
 *        of conditions and the following disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *  THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 *  DMITRY VYUKOV OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  The views and conclusions contained in the software and documentation are those of the authors and should not be interpreted
 *  as representing official policies, either expressed or implied, of Dmitry Vyukov.
 */ 
#pragma once

#include <geometrix/utility/assert.hpp>
#include <atomic>

namespace stk { namespace thread {

    template<typename T>
    class vyukov_mpmc_bounded_queue
    {
    public:
        vyukov_mpmc_bounded_queue(size_t buffer_size)
            : buffer_(new cell_t [buffer_size])
            , buffer_mask_(buffer_size - 1)
        {
			//! buffer_size needs to be a power of 2.
            GEOMETRIX_ASSERT((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0));
            for (size_t i = 0; i != buffer_size; i += 1)
                buffer_[i].sequence_.store(i, std::memory_order_relaxed);
            enqueue_pos_.store(0, std::memory_order_relaxed);
            dequeue_pos_.store(0, std::memory_order_relaxed);
        }

        ~vyukov_mpmc_bounded_queue()
        {
            delete [] buffer_;
        }

        template <typename U>
        bool try_push(U const& data)
        {
            cell_t* cell;
            size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
            for (;;)
            {
                cell = &buffer_[pos & buffer_mask_];
                size_t seq = cell->sequence_.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)pos;
                if (dif == 0)
                {
                    if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0)
                    return false;
                else
                    pos = enqueue_pos_.load(std::memory_order_relaxed);
            }

            cell->data_ = data;
            cell->sequence_.store(pos + 1, std::memory_order_release);

            return true;
        }

        bool try_push(T&& data)
        {
            cell_t* cell;
            size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
            for (;;)
            {
                cell = &buffer_[pos & buffer_mask_];
                size_t seq = cell->sequence_.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)pos;
                if (dif == 0)
                {
                    if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0)
                    return false;
                else
                    pos = enqueue_pos_.load(std::memory_order_relaxed);
            }

            cell->data_ = std::move(data);
            cell->sequence_.store(pos + 1, std::memory_order_release);

            return true;
        }

        bool try_pop(T& data)
        {
            cell_t* cell;
            size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
            for (;;)
            {
                cell = &buffer_[pos & buffer_mask_];
                size_t seq = cell->sequence_.load(std::memory_order_acquire);
                intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
                if (dif == 0)
                {
                    if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0)
                    return false;
                else
                    pos = dequeue_pos_.load(std::memory_order_relaxed);
            }

            data = std::move(cell->data_);
            cell->sequence_.store(pos + buffer_mask_ + 1, std::memory_order_release);

            return true;
        }

    private:
        
        struct cell_t
        {
            std::atomic<size_t>     sequence_;
            T                       data_;
        };

        static size_t const         cacheline_size = 64;
        typedef char                cacheline_pad_t [cacheline_size];

        cacheline_pad_t             pad0_;
        cell_t* const               buffer_;
        size_t const                buffer_mask_;
        cacheline_pad_t             pad1_;
        std::atomic<size_t>         enqueue_pos_;
        cacheline_pad_t             pad2_;
        std::atomic<size_t>         dequeue_pos_;
        cacheline_pad_t             pad3_;

        vyukov_mpmc_bounded_queue(vyukov_mpmc_bounded_queue const&) = delete;
        void operator = (vyukov_mpmc_bounded_queue const&) = delete;
    };

    struct vyukov_mpmc_queue_traits
	{
		template <typename T, typename Alloc = std::allocator<T>, typename Mutex = std::mutex>
		using queue_type = vyukov_mpmc_bounded_queue<T>;
		using queue_info = void*;

		template <typename T>
		static queue_info get_queue_info(queue_type<T>&q)
		{
			return nullptr;
		}

		template <typename T, typename Value>
		static bool try_push(queue_type<T>& q, queue_info, Value&& value)
		{
			return q.try_push(std::forward<Value>(value));
		}

		template <typename T>
		static bool try_pop(queue_type<T>& q, queue_info, T& value)
		{
			return q.try_pop(value);
		}

		template <typename T>
		static bool try_steal(queue_type<T>& q, queue_info, T& value)
		{
			return q.try_pop(value);
		}

		template <typename T, typename Value>
		static bool try_push(queue_type<T>& q, Value&& value)
		{
			return q.try_push(std::forward<Value>(value));
		}

		template <typename T>
		static bool try_pop(queue_type<T>& q, T& value)
		{
			return q.try_pop(value);
		}

		template <typename T>
		static bool try_steal(queue_type<T>& q, T& value)
		{
			return q.try_pop(value);
		}
	};
}}//! namespace stk::thread;
