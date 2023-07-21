#pragma once

#include <sstream>
/*
#include <gtest/gtest.h>

namespace stk::testing {
struct CustomMessageStream
{
    CustomMessageStream() 
		: mColor(::testing::GTestColor::kYellow)
	{}

	CustomMessageStream(::testing::GTestColor messageColor)
		: mColor(messageColor)
	{}

    CustomMessageStream(const std::string& s)
		: mColor(::testing::GTestColor::kYellow)
	{
		mStream << s;
	}

    CustomMessageStream(const std::string& s, ::testing::GTestColor messageColor)
		: mColor(messageColor)
	{
		mStream << s;
	}

    ~CustomMessageStream()
    {
		if (mStream.str().empty() || (*mStream.str().rbegin()) != '\n')
			mStream << '\n';

        ::testing::internal::ColoredPrintf(::testing::GTestColor::kGreen, "[   NOTE   ] "); 
        ::testing::internal::ColoredPrintf(mColor, mStream.str().c_str());
    }
    
    template <typename T>
    std::ostream& operator <<(const T& value) 
    { 
        mStream << value;
        return mStream; 
    }

    std::string str() const { return mStream.str(); }
private:
    ::testing::GTestColor        mColor;
    std::stringstream mStream;
};

}//! namespace stk::testing;
*/
//#define GTEST_MESSAGE(...) stk::testing::CustomMessageStream(__VA_ARGS__)
#define GTEST_MESSAGE( ... ) ( std::cout << std::stringstream( __VA_ARGS__ ).str() )
