
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef STK_MAKE_READY_FUTURE_HPP
#define STK_MAKE_READY_FUTURE_HPP
/*
#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

#include <boost/config.hpp>

#include <boost/fiber/detail/convert.hpp>
#include <boost/fiber/detail/disable_overload.hpp>
#include <boost/fiber/exceptions.hpp>
#include <boost/fiber/future/detail/task_base.hpp>
#include <boost/fiber/future/detail/task_object.hpp>
#include <boost/fiber/future/future.hpp>

namespace boost {
namespace fibers {

template< typename R>
class ready_task 
{
private:
    typedef typename detail::task_base<R>::ptr_t   ptr_t;
    
    template< typename R>
    struct ready_task_impl : public detail::task_base<R>
    {
        ready_task_impl(R&& value)
        {
            set_value(std::forward<R>(value));
        }
        
        void run() override{};
        ptr_t reset() override {};
    };

    bool            obtained_{ false };
    ptr_t           task_{};

public:
    constexpr ready_task() noexcept = default;

    template<typename Value>
    explicit ready_task(Value&& v) 
    : task_(new ready_task_impl<Value>(std::forward<Value>(v)))
    {
        
    }

    ~ready_task() 
    {
        if (task_) 
        {
            task_->owner_destroyed();
        }
    }

    ready_task(ready_task const&) = delete;
    ready_task & operator=(ready_task const&) = delete;

    ready_task(ready_task && other) noexcept 
    : obtained_{other.obtained_}
    , task_{std::move(other.task_)}
    {
        other.obtained_ = false;
    }

    ready_task & operator=(ready_task && other) noexcept 
    {
        if (this == &other)
			return * this;
        ready_task tmp{ std::move( other) };
        swap( tmp);
        return * this;
    }

    void swap(ready_task & other) noexcept 
    {
        std::swap(obtained_, other.obtained_);
        task_.swap(other.task_);
    }

    bool valid() const noexcept 
    {
        return nullptr != task_.get();
    }

    future< R > get_future() 
    {
        if (obtained_)
            throw future_already_retrieved{};
        obtained_ = true;
        return future<R>{boost::static_pointer_cast<detail::shared_state<R>>(task_)};
    }

    BOOST_FORCEINLINE void operator()(){}

    void reset() 
    {
        ready_task tmp;
        tmp.task_ = task_;
        task_ = tmp.task_->reset();
        obtained_ = false;
    }
};

template< typename Signature >
inline void swap( ready_task< Signature > & l, ready_task< Signature > & r) noexcept 
{
    l.swap( r);
}

template <typename T>
inline future<T> make_ready_future(T&& value)
{
    ready_task<T> tsk(std::forward<T>(value));
    return tsk.get_future();
}

}}//! namespace boost::fibers;
*/
#endif //! STK_MAKE_READY_FUTURE_HPP
