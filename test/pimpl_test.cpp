#include "pimpl_test.hpp"
#include <stk/utility/pimpl.hpp>
#include <geometrix/utility/assert.hpp>

struct A::AImpl
{
	AImpl(int x)
		: x{ x }
	{}

	AImpl(const AImpl& o) 
		: x(o.x*10)
	{}

	AImpl(AImpl&& o) 
		: x(o.x*100)
	{
		//! So far, this should never be called.
		GEOMETRIX_ASSERT(false);
	}

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

A::A(A&& o)
	: m_impl(std::move(o.m_impl))
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
		: x(o.x*10)
	{}

	AImpl(AImpl&& o) 
		: x(o.x*100)
	{
		//! So far, this should never be called.
		GEOMETRIX_ASSERT(false);
	}

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

struct A_no_copy::AImpl
{
	AImpl(int x)
		: x{ x }
	{}

	AImpl(const AImpl& o) 
		: x(o.x*10)
	{
		GEOMETRIX_ASSERT(false);
	}

	AImpl(AImpl&& o) 
		: x(o.x*100)
	{
		//! So far, this should never be called.
		GEOMETRIX_ASSERT(false);
	}

	int x = 0;
};

A_no_copy::A_no_copy(A_no_copy&& o)
	: m_impl(std::move(o.m_impl))
{

}

A_no_copy::A_no_copy(int x)
	: m_impl(stk::make_pimpl<AImpl>(x))
{

}

A_no_copy& A_no_copy::operator=(A_no_copy&& o)
{
	m_impl = std::move(o.m_impl);
	return *this;
}

int A_no_copy::get_x() const
{
	return m_impl->x;
}

