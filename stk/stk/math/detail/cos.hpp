/* origin: FreeBSD /usr/src/lib/msun/src/s_cos.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/* cos(x)
 * Return cosine function of x.
 *
 * kernel function:
 *      __sin           ... sine function on [-pi/4,pi/4]
 *      __cos           ... cosine function on [-pi/4,pi/4]
 *      __rem_pio2      ... argument reduction routine
 *
 * Method.
 *      Let S,C and T denote the sin, cos and tan respectively on
 *      [-PI/4, +PI/4]. Reduce the argument x to y1+y2 = x-k*pi/2
 *      in [-pi/4 , +pi/4], and let n = k mod 4.
 *      We have
 *
 *          n        sin(x)      cos(x)        tan(x)
 *     ----------------------------------------------------------
 *          0          S           C             T
 *          1          C          -S            -1/T
 *          2         -S          -C             T
 *          3         -C           S            -1/T
 *     ----------------------------------------------------------
 *
 * Special cases:
 *      Let trig be any of sin, cos, or tan.
 *      trig(+-INF)  is NaN, with signals;
 *      trig(NaN)    is that NaN;
 *
 * Accuracy:
 *      TRIG(x) returns trig(x) nearly rounded
 */
#pragma once

#include <stk/math/detail/common.hpp>

namespace stk::math::detail {
    inline double cos(double x)
    {
        double y[2];
        std::uint32_t ix;
        unsigned n;

        STK_GET_HIGH_WORD(ix, x);
        ix &= 0x7fffffff;

        /* |x| ~< pi/4 */
        if (ix <= 0x3fe921fb) {
            if (ix < 0x3e46a09e) {  /* |x| < 2**-27 * sqrt(2) */
                /* raise inexact if x!=0 */
                STK_FORCE_EVAL(x + 0x1p120f);
                return 1.0;
            }
            return stk::math::detail::__cos(x, 0);
        }

        /* cos(Inf or NaN) is NaN */
        if (ix >= 0x7ff00000)
            return x-x;

        /* argument reduction */
        n = stk::math::detail::__rem_pio2(x, y);
        switch (n&3) {
        case 0: return  stk::math::detail::__cos(y[0], y[1]);
        case 1: return -stk::math::detail::__sin(y[0], y[1], 1);
        case 2: return -stk::math::detail::__cos(y[0], y[1]);
        default:
            return  stk::math::detail::__sin(y[0], y[1], 1);
        }
    }
}//! namespace stk::math::detail;

