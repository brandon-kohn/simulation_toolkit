#pragma once

#include <stk/utility/pimpl.hpp>

class A
{
public:

	A() = default;
	A(int x);

	int get_x() const;
	
private:

	struct AImpl;
	stk::pimpl<AImpl> m_impl;
};

class A_no_copy_no_move
{
public:

	A_no_copy_no_move() = default;
	A_no_copy_no_move(int x);
	A_no_copy_no_move(const A_no_copy_no_move&) = delete;
	A_no_copy_no_move(A_no_copy_no_move&&) = delete;
	A_no_copy_no_move& operator=(const A_no_copy_no_move&) = delete;
	A_no_copy_no_move& operator=(A_no_copy_no_move&&) = delete;

	int get_x() const;
	
private:

	struct AImpl;
	stk::pimpl<AImpl> m_impl;
};
