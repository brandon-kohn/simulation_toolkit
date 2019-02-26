#include "pimpl_test.hpp"
#include <stk/utility/pimpl.ipp>

struct A::AImpl
{
	AImpl(int x)
		: x{ x }
	{}

	AImpl(const AImpl& o) 
		: x(o.x+1)
	{}

	AImpl(AImpl&& o) 
		: x(o.x+1)
	{}

	int x = 0;
};

A::A(int x)
	: m_impl(stk::make_pimpl<AImpl>(x))
{

}

A::A(const A& o) 
	: m_impl(o.m_impl)
{

}

A& A::operator=(A o)
{
	o.swap(*this);
	return *this;
}

int A::get_x() const
{
	return m_impl->x;
}

struct A_no_copy_no_move::AImpl
{
	AImpl(int x)
		: x{ x }
	{}

	AImpl(const AImpl& o) 
		: x(o.x+1)
	{}

	AImpl(AImpl&& o) 
		: x(o.x+1)
	{}

	int x = 0;
};

A_no_copy_no_move::A_no_copy_no_move(int x)
	: m_impl(stk::make_pimpl<AImpl>(x))
{

}

int A_no_copy_no_move::get_x() const
{
	return m_impl->x;
}

struct B::BImpl : ADyn
{
	BImpl(bool& deleted)
		: ADyn(deleted)
	{}
};
B::B(bool& deleted)
	: m_impl(stk::make_pimpl<BImpl>(deleted))
{

}
