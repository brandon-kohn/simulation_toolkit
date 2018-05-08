//
//! Copyright © 2015
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
/////////////////////////////////////////////////////////////////////////////
#pragma once

#include <boost/serialization/version.hpp>
#include <boost/preprocessor/enum_params.hpp>

// specify the current version number for the class
// version numbers limited to 8 bits !!!
#define STK_CLASS_VERSION_TMPL(T, NParms, N)                           \
namespace boost {                                                      \
namespace serialization {                                              \
template<BOOST_PP_ENUM_PARAMS(NParms, typename A)>                     \
struct version< T<BOOST_PP_ENUM_PARAMS(NParms, A)> >                   \
{                                                                      \
    typedef mpl::int_<N> type;                                         \
    typedef mpl::integral_c_tag tag;                                   \
    BOOST_STATIC_CONSTANT(int, value = version::type::value);          \
    BOOST_MPL_ASSERT((                                                 \
        boost::mpl::less<                                              \
            boost::mpl::int_<N>,                                       \
            boost::mpl::int_<256>                                      \
        >                                                              \
    ));                                                                \
};                                                                     \
}}                                                                     \
/***/

#define STK_CV_TYPE_ELEM(z, n, seq)                                    \
    BOOST_PP_COMMA_IF(n) BOOST_PP_SEQ_ELEM(n,seq) BOOST_PP_CAT(A,n)    \
/***/

// specify the current version number for the class
// version numbers limited to 8 bits !!!
#define STK_CLASS_VERSION_CUSTOM_TYPE_TMPL(T, Seq, N)                  \
namespace boost {                                                      \
namespace serialization {                                              \
template                                                               \
<                                                                      \
    BOOST_PP_REPEAT(BOOST_PP_SEQ_SIZE(Seq),STK_CV_TYPE_ELEM,Seq)       \
>                                                                      \
struct version<T<BOOST_PP_ENUM_PARAMS(BOOST_PP_SEQ_SIZE(Seq), A)> >    \
{                                                                      \
    typedef mpl::int_<N> type;                                         \
    typedef mpl::integral_c_tag tag;                                   \
    BOOST_STATIC_CONSTANT(int, value = version::type::value);          \
    BOOST_MPL_ASSERT((                                                 \
        boost::mpl::less<                                              \
            boost::mpl::int_<N>,                                       \
            boost::mpl::int_<256>                                      \
        >                                                              \
    ));                                                                \
};                                                                     \
}}                                                                     \
/***/

// specify the current version number for the class
// version numbers limited to 8 bits !!!
#define STK_CLASS_VERSION_TMPL_NESTED(T, NParms, Nested, N)            \
    namespace boost {                                                  \
    namespace serialization {                                          \
    template<BOOST_PP_ENUM_PARAMS(NParms, typename A)>                 \
struct version< typename T<BOOST_PP_ENUM_PARAMS(NParms, A)>::Nested >  \
{                                                                      \
    typedef mpl::int_<N> type;                                         \
    typedef mpl::integral_c_tag tag;                                   \
    BOOST_STATIC_CONSTANT(int, value = version::type::value);          \
    BOOST_MPL_ASSERT((                                                 \
        boost::mpl::less<                                              \
            boost::mpl::int_<N>,                                       \
            boost::mpl::int_<256>                                      \
        >                                                              \
    ));                                                                \
};                                                                     \
}}                                                                     \
/***/

// specify the current version number for the class
// version numbers limited to 8 bits !!!
#define STK_CLASS_VERSION_TMPL_NESTEDT(T,Parms,Nested,NParms,N)        \
    namespace boost {                                                  \
    namespace serialization {                                          \
    template                                                           \
    <                                                                  \
        BOOST_PP_ENUM_PARAMS(Parms, typename A)                        \
      , BOOST_PP_ENUM_PARAMS(NParms, typename B)                       \
    >                                                                  \
struct version                                                         \
<                                                                      \
    typename T<BOOST_PP_ENUM_PARAMS(Parms, A)>                         \
    ::template Nested<BOOST_PP_ENUM_PARAMS(NParms, B)>                 \
>                                                                      \
{                                                                      \
    typedef mpl::int_<N> type;                                         \
    typedef mpl::integral_c_tag tag;                                   \
    BOOST_STATIC_CONSTANT(int, value = version::type::value);          \
    BOOST_MPL_ASSERT((                                                 \
        boost::mpl::less<                                              \
            boost::mpl::int_<N>,                                       \
            boost::mpl::int_<256>                                      \
        >                                                              \
    ));                                                                \
};                                                                     \
}}                                                                     \
/***/

