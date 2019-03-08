#pragma once

#include <stk/utility/pimpl.hpp>

class A
{
public:

	A(int x);
#ifdef BOOST_NO_CXX11_DEFAULTED_MOVES
	A(A&& o);
	A& operator =(A&& o);
#else
	A(A&& o) = default;
	A& operator =(A&& o) = default;
#endif

	A() = default;
	A(const A& o) = default;
	A& operator =(const A& o) = default;
	~A() = default;

	int get_x() const;

	void swap(A& o) 
	{
		m_impl.swap(o.m_impl);
	}

	bool is_valid() const
	{
		return m_impl;
	}
	
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

	bool is_valid() const
	{
		return m_impl;
	}
	
private:

	struct AImpl;
	stk::pimpl<AImpl> m_impl;
};

class A_no_copy
{
public:

	A_no_copy() = default;
	A_no_copy(int x);
	A_no_copy(const A_no_copy&) = delete;
	A_no_copy(A_no_copy&&);
	A_no_copy& operator=(const A_no_copy&) = delete;
	A_no_copy& operator=(A_no_copy&&);

	int get_x() const;

	bool is_valid() const
	{
		return m_impl;
	}
	
private:

	struct AImpl;
	stk::pimpl<AImpl> m_impl;
};

class ADyn
{
public:

	ADyn(bool& deleted)
		: deleted(deleted)
	{}

	virtual ~ADyn()
	{
		deleted = true;
	}

private:

	bool& deleted;
};

struct B 
{
	B(bool& deleted);
	struct BImpl;
	stk::pimpl<BImpl> m_impl;

};