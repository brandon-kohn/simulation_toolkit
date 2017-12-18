
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef STK_MAKE_READY_FUTURE_HPP
#define STK_MAKE_READY_FUTURE_HPP

#include <boost/fiber/future/promise.hpp>

namespace boost { namespace fibers {

template <typename T>
inline future<typename std::decay<T>::type> make_ready_future(T&& value)
{
	using R = typename std::decay<T>::type;
    boost::fibers::promise<R> pi;
    auto f = pi.get_future();
    pi.set_value(std::forward<T>(value));
    return std::move(f);
}

}}//! namespace boost::fibers;

#endif //! STK_MAKE_READY_FUTURE_HPP
