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
