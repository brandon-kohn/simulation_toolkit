/*
                color Rendering of Spectra

                       by John Walker
                  http://www.fourmilab.ch/

         Last updated: March 9, 2003
         Heavily modified by Brandon Kohn March 1, 2012.

           This program is in the public domain.

    For complete information about the techniques employed in
    this program, see the World-Wide Web document:

             http://www.fourmilab.ch/documents/specrend/

    The xyz_to_rgb() function, which was wrong in the original
    version of this program, was corrected by:

        Andrew J. S. Hamilton 21 May 1999
        Andrew.Hamilton@Colorado.EDU
        http://casa.colorado.edu/~ajsh/

    who also added the gamma correction facilities and
    modified constrain_rgb() to work by desaturating the
    colour by adding white.

    A program which uses these functions to plot CIE
    "tongue" diagrams called "ppmcie" is included in
    the Netpbm graphics toolkit:
        http://netpbm.sourceforge.net/
    (The program was called cietoppm in earlier
    versions of Netpbm.)

*/
#pragma once

#include <stk/utility/color.hpp>
#include <cstdio>
#include <cmath>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace stk {

    /* A colour system is defined by the CIE x and y coordinates of
       its three primary illuminants and the x and y coordinates of
       the white point. */
    struct color_system
    {
        color_system( const std::string& name
            , double xRed, double yRed
            , double xGreen, double yGreen
            , double xBlue, double yBlue
            , double xWhite, double yWhite
            , double gamma )
            : xRed(xRed), yRed(yRed)
            , xGreen(xGreen), yGreen(yGreen)
            , xBlue(xBlue), yBlue(yBlue)
            , xWhite(xWhite), yWhite(yWhite)
            , gamma(gamma)
        {}

        char *name;                     /* color system name */
        double xRed, yRed,              /* Red x, y */
               xGreen, yGreen,          /* Green x, y */
               xBlue, yBlue,            /* Blue x, y */
               xWhite, yWhite,          /* White point x, y */
               gamma;                   /* Gamma correction for system */
    };

    /* White point chromaticities. */

    #define CS_ILLUMINANT_C     0.3101, 0.3162          /* For NTSC television */
    #define CS_ILLUMINANT_D65   0.3127, 0.3291          /* For EBU and SMPTE */
    #define CS_ILLUMINANT_E     0.33333333, 0.33333333  /* CIE equal-energy illuminant */

    /*  Gamma of nonlinear correction.

        See Charles Poynton's ColorFAQ Item 45 and GammaFAQ Item 6 at:

           http://www.poynton.com/ColorFAQ.html
           http://www.poynton.com/GammaFAQ.html

    */

    #define CS_GAMMA_REC709 0       /* Rec. 709 */

    #define CS_DEFINE_COLOUR_SYSTEM(name, friendlyName, xRed, yRed, xGreen, yGreen, xBlue, yBlue, xWhite, yWhite, Gamma ) \
        struct name##system : color_system                                                                             \
        {                                                                                                              \
            name##system()                                                                                             \
                : color_system( friendlyName, xRed, yRed, xGreen, yGreen, xBlue, yBlue, xWhite, yWhite, Gamma )        \
            {}                                                                                                         \
        };                                                                                                             \
    /***/

    //!                   Name                          xRed    yRed    xGreen  yGreen  xBlue  yBlue    White point             Gamma
    CS_DEFINE_COLOUR_SYSTEM( NTSC,   "NTSC",            0.67,   0.33,   0.21,   0.71,   0.14,   0.08,   0.3101, 0.3162,         CS_GAMMA_REC709 )
    CS_DEFINE_COLOUR_SYSTEM( EBU,    "EBU (PAL/SECAM)", 0.64,   0.33,   0.29,   0.60,   0.15,   0.06,   0.3127, 0.3291,         CS_GAMMA_REC709 )
    CS_DEFINE_COLOUR_SYSTEM( SMPTE,  "SMPTE",           0.630,  0.340,  0.310,  0.595,  0.155,  0.070,  0.3127, 0.3291,         CS_GAMMA_REC709 )
    CS_DEFINE_COLOUR_SYSTEM( HDTV,   "HDTV",            0.670,  0.330,  0.210,  0.710,  0.150,  0.060,  0.3127, 0.3291,         CS_GAMMA_REC709 )
    CS_DEFINE_COLOUR_SYSTEM( CIE,    "CIE",             0.7355, 0.2645, 0.2658, 0.7243, 0.1669, 0.0085, 0.33333333, 0.33333333, CS_GAMMA_REC709 )
    CS_DEFINE_COLOUR_SYSTEM( Rec709, "CIE REC 709",     0.64,   0.33,   0.30,   0.60,   0.15,   0.06,   0.3127, 0.3291,         CS_GAMMA_REC709 )

    namespace detail{
        /*                          UPVP_TO_XY
            Given 1976 coordinates u', v', determine 1931 chromaticities x, y
        */
        inline void upvp_to_xy(double up, double vp, double *xc, double *yc)
        {
            *xc = (9 * up) / ((6 * up) - (16 * vp) + 12);
            *yc = (4 * vp) / ((6 * up) - (16 * vp) + 12);
        }

        /*                          XY_TO_UPVP
            Given 1931 chromaticities x, y, determine 1976 coordinates u', v'
        */
        inline void xy_to_upvp(double xc, double yc, double *up, double *vp)
        {
            *up = (4 * xc) / ((-2 * xc) + (12 * yc) + 3);
            *vp = (9 * yc) / ((-2 * xc) + (12 * yc) + 3);
        }

        /*                             XYZ_TO_RGB

            Given an additive tricolour system CS, defined by the CIE x
            and y chromaticities of its three primaries (z is derived
            trivially as 1-(x+y)), and a desired chromaticity (XC, YC,
            ZC) in CIE space, determine the contribution of each
            primary in a linear combination which sums to the desired
            chromaticity.  If the  requested chromaticity falls outside
            the Maxwell  triangle (colour gamut) formed by the three
            primaries, one of the r, g, or b weights will be negative.

            Caller can use constrain_rgb() to desaturate an
            outside-gamut colour to the closest representation within
            the available gamut and/or norm_rgb to normalise the RGB
            components so the largest nonzero component has value 1.

        */
        inline void xyz_to_rgb(struct color_system *cs,
                        double xc, double yc, double zc,
                        double *r, double *g, double *b)
        {
            double xr, yr, zr, xg, yg, zg, xb, yb, zb;
            double xw, yw, zw;
            double rx, ry, rz, gx, gy, gz, bx, by, bz;
            double rw, gw, bw;

            xr = cs->xRed;    yr = cs->yRed;    zr = 1 - (xr + yr);
            xg = cs->xGreen;  yg = cs->yGreen;  zg = 1 - (xg + yg);
            xb = cs->xBlue;   yb = cs->yBlue;   zb = 1 - (xb + yb);

            xw = cs->xWhite;  yw = cs->yWhite;  zw = 1 - (xw + yw);

            /* xyz -> rgb matrix, before scaling to white. */

            rx = (yg * zb) - (yb * zg);  ry = (xb * zg) - (xg * zb);  rz = (xg * yb) - (xb * yg);
            gx = (yb * zr) - (yr * zb);  gy = (xr * zb) - (xb * zr);  gz = (xb * yr) - (xr * yb);
            bx = (yr * zg) - (yg * zr);  by = (xg * zr) - (xr * zg);  bz = (xr * yg) - (xg * yr);

            /* White scaling factors.
               Dividing by yw scales the white luminance to unity, as conventional. */

            rw = ((rx * xw) + (ry * yw) + (rz * zw)) / yw;
            gw = ((gx * xw) + (gy * yw) + (gz * zw)) / yw;
            bw = ((bx * xw) + (by * yw) + (bz * zw)) / yw;

            /* xyz -> rgb matrix, correctly scaled to white. */

            rx = rx / rw;  ry = ry / rw;  rz = rz / rw;
            gx = gx / gw;  gy = gy / gw;  gz = gz / gw;
            bx = bx / bw;  by = by / bw;  bz = bz / bw;

            /* rgb of the desired point */

            *r = (rx * xc) + (ry * yc) + (rz * zc);
            *g = (gx * xc) + (gy * yc) + (gz * zc);
            *b = (bx * xc) + (by * yc) + (bz * zc);
        }

        /*                            INSIDE_GAMUT

             Test whether a requested colour is within the gamut
             achievable with the primaries of the current colour
             system.  This amounts simply to testing whether all the
             primary weights are non-negative. */

        inline int inside_gamut(double r, double g, double b)
        {
            return (r >= 0) && (g >= 0) && (b >= 0);
        }

        /*                          CONSTRAIN_RGB

            If the requested RGB shade contains a negative weight for
            one of the primaries, it lies outside the colour gamut
            accessible from the given triple of primaries.  Desaturate
            it by adding white, equal quantities of R, G, and B, enough
            to make RGB all positive.  The function returns 1 if the
            components were modified, zero otherwise.

        */

        inline int constrain_rgb(double *r, double *g, double *b)
        {
            double w;

            /* Amount of white needed is w = - min(0, *r, *g, *b) */

            w = (0 < *r) ? 0 : *r;
            w = (w < *g) ? w : *g;
            w = (w < *b) ? w : *b;
            w = -w;

            /* Add just enough white to make r, g, b all positive. */

            if (w > 0) {
                *r += w;  *g += w; *b += w;
                return 1;                     /* color modified to fit RGB gamut */
            }

            return 0;                         /* color within RGB gamut */
        }

        /*                          GAMMA_CORRECT_RGB

            Transform linear RGB values to nonlinear RGB values. Rec.
            709 is ITU-R Recommendation BT. 709 (1990) ``Basic
            Parameter Values for the HDTV Standard for the Studio and
            for International Programme Exchange'', formerly CCIR Rec.
            709. For details see

               http://www.poynton.com/ColorFAQ.html
               http://www.poynton.com/GammaFAQ.html
        */

        inline void gamma_correct(const struct color_system *cs, double *c)
        {
            double gamma;

            gamma = cs->gamma;

            if (gamma == CS_GAMMA_REC709) {
            /* Rec. 709 gamma correction. */
            double cc = 0.018;

            if (*c < cc) {
                *c *= ((1.099 * pow(cc, 0.45)) - 0.099) / cc;
            } else {
                *c = (1.099 * pow(*c, 0.45)) - 0.099;
            }
            } else {
            /* Nonlinear colour = (Linear colour)^(1/gamma) */
            *c = pow(*c, 1.0 / gamma);
            }
        }

        inline void gamma_correct_rgb(const struct color_system *cs, double *r, double *g, double *b)
        {
            gamma_correct(cs, r);
            gamma_correct(cs, g);
            gamma_correct(cs, b);
        }

        /*                          NORM_RGB

            Normalise RGB components so the most intense (unless all
            are zero) has a value of 1.

        */
        inline void norm_rgb(double *r, double *g, double *b)
        {
            double greatest = (std::max)(*r, (std::max)(*g, *b));
            if (greatest > 0)
            {
                *r /= greatest;
                *g /= greatest;
                *b /= greatest;
            }
        }

        /*                          SPECTRUM_TO_XYZ

            Calculate the CIE X, Y, and Z coordinates corresponding to
            a light source with spectral distribution given by  the
            function SPEC_INTENS, which is called with a series of
            wavelengths between 380 and 780 nm (the argument is
            expressed in meters), which returns emittance at  that
            wavelength in arbitrary units.  The chromaticity
            coordinates of the spectrum are returned in the x, y, and z
            arguments which respect the identity:

                    x + y + z = 1.
        */

        // inline void spectrum_to_xyz(double (*spec_intens)(double wavelength),
        //                      double *x, double *y, double *z)
        // {
        //     /* CIE colour matching functions xBar, yBar, and zBar for
        //        wavelengths from 380 through 780 nanometers, every 5
        //        nanometers.  For a wavelength lambda in this range:
        //
        //             cie_colour_match[(lambda - 380) / 5][0] = xBar
        //             cie_colour_match[(lambda - 380) / 5][1] = yBar
        //             cie_colour_match[(lambda - 380) / 5][2] = zBar
        //
        //  To save memory, this table can be declared as floats
        //  rather than doubles; (IEEE) float has enough
        //  significant bits to represent the values. It's declared
        //  as a double here to avoid warnings about "conversion
        //  between floating-point types" from certain persnickety
        //  compilers. */
        //
        //     static double cie_colour_match[81][3] = {
        //         {0.0014,0.0000,0.0065}, {0.0022,0.0001,0.0105}, {0.0042,0.0001,0.0201},
        //         {0.0076,0.0002,0.0362}, {0.0143,0.0004,0.0679}, {0.0232,0.0006,0.1102},
        //         {0.0435,0.0012,0.2074}, {0.0776,0.0022,0.3713}, {0.1344,0.0040,0.6456},
        //         {0.2148,0.0073,1.0391}, {0.2839,0.0116,1.3856}, {0.3285,0.0168,1.6230},
        //         {0.3483,0.0230,1.7471}, {0.3481,0.0298,1.7826}, {0.3362,0.0380,1.7721},
        //         {0.3187,0.0480,1.7441}, {0.2908,0.0600,1.6692}, {0.2511,0.0739,1.5281},
        //         {0.1954,0.0910,1.2876}, {0.1421,0.1126,1.0419}, {0.0956,0.1390,0.8130},
        //         {0.0580,0.1693,0.6162}, {0.0320,0.2080,0.4652}, {0.0147,0.2586,0.3533},
        //         {0.0049,0.3230,0.2720}, {0.0024,0.4073,0.2123}, {0.0093,0.5030,0.1582},
        //         {0.0291,0.6082,0.1117}, {0.0633,0.7100,0.0782}, {0.1096,0.7932,0.0573},
        //         {0.1655,0.8620,0.0422}, {0.2257,0.9149,0.0298}, {0.2904,0.9540,0.0203},
        //         {0.3597,0.9803,0.0134}, {0.4334,0.9950,0.0087}, {0.5121,1.0000,0.0057},
        //         {0.5945,0.9950,0.0039}, {0.6784,0.9786,0.0027}, {0.7621,0.9520,0.0021},
        //         {0.8425,0.9154,0.0018}, {0.9163,0.8700,0.0017}, {0.9786,0.8163,0.0014},
        //         {1.0263,0.7570,0.0011}, {1.0567,0.6949,0.0010}, {1.0622,0.6310,0.0008},
        //         {1.0456,0.5668,0.0006}, {1.0026,0.5030,0.0003}, {0.9384,0.4412,0.0002},
        //         {0.8544,0.3810,0.0002}, {0.7514,0.3210,0.0001}, {0.6424,0.2650,0.0000},
        //         {0.5419,0.2170,0.0000}, {0.4479,0.1750,0.0000}, {0.3608,0.1382,0.0000},
        //         {0.2835,0.1070,0.0000}, {0.2187,0.0816,0.0000}, {0.1649,0.0610,0.0000},
        //         {0.1212,0.0446,0.0000}, {0.0874,0.0320,0.0000}, {0.0636,0.0232,0.0000},
        //         {0.0468,0.0170,0.0000}, {0.0329,0.0119,0.0000}, {0.0227,0.0082,0.0000},
        //         {0.0158,0.0057,0.0000}, {0.0114,0.0041,0.0000}, {0.0081,0.0029,0.0000},
        //         {0.0058,0.0021,0.0000}, {0.0041,0.0015,0.0000}, {0.0029,0.0010,0.0000},
        //         {0.0020,0.0007,0.0000}, {0.0014,0.0005,0.0000}, {0.0010,0.0004,0.0000},
        //         {0.0007,0.0002,0.0000}, {0.0005,0.0002,0.0000}, {0.0003,0.0001,0.0000},
        //         {0.0002,0.0001,0.0000}, {0.0002,0.0001,0.0000}, {0.0001,0.0000,0.0000},
        //         {0.0001,0.0000,0.0000}, {0.0001,0.0000,0.0000}, {0.0000,0.0000,0.0000}
        //     };
        //  double lambda, X = 0, Y = 0, Z = 0, XYZ;
        //
        //     for(int i = 0, lambda = 380; lambda < 780.1; ++i, lambda += 5)
        //  {
        //         double Me;
        //         Me = (*spec_intens)(lambda);
        //         X += Me * cie_colour_match[i][0];
        //         Y += Me * cie_colour_match[i][1];
        //         Z += Me * cie_colour_match[i][2];
        //     }
        //
        //  XYZ = (X + Y + Z);
        //     *x = X / XYZ;
        //     *y = Y / XYZ;
        //     *z = Z / XYZ;
        // }

        struct color_table
        {
            static const int num_bins = 440;
            static const int X = 0;
            static const int Y = 1;
            static const int Z = 2;

            //!Return the visible wavelength extremes in nanometers.
            static double min_wavelength() { return 360.0; }
            static double max_wavelength() { return 800.0; }
            static double bin_width() { return (max_wavelength() - min_wavelength()) / num_bins; }

            static double get_value(std::size_t i, std::size_t j)
            {
                //! Actually have data up to 830... but don't need it.
                static double table[471][3] =
                {
                    { 0.0001299, 3.917e-006, 0.0006061 }, { 0.000145847, 4.39e-006, 0.000680879 }, { 0.000163802, 4.93e-006, 0.000765146 },
                    { 0.000184004, 5.53e-006, 0.000860012 }, { 0.00020669, 6.21e-006, 0.000966593 }, { 0.0002321, 6.965e-006, 0.001086 },
                    { 0.000260728, 7.81e-006, 0.00122059 }, { 0.000293075, 8.77e-006, 0.00137273 }, { 0.000329388, 9.84e-006, 0.00154358 },
                    { 0.000369914, 1.1e-005, 0.00173429 }, { 0.0004149, 1.239e-005, 0.001946 }, { 0.000464159, 1.39e-005, 0.00217778 },
                    { 0.000518986, 1.56e-005, 0.00243581 }, { 0.000581854, 1.74e-005, 0.00273195 }, { 0.000655235, 1.96e-005, 0.00307806 },
                    { 0.0007416, 2.202e-005, 0.003486 }, { 0.00084503, 2.48e-005, 0.00397523 }, { 0.000964527, 2.8e-005, 0.00454088 },
                    { 0.00109495, 3.15e-005, 0.00515832 }, { 0.00123115, 3.52e-005, 0.00580291 }, { 0.001368, 3.9e-005, 0.00645 },
                    { 0.00150205, 4.28e-005, 0.00708322 }, { 0.00164233, 4.69e-005, 0.00774549 }, { 0.00180238, 5.16e-005, 0.00850115 },
                    { 0.00199576, 5.72e-005, 0.00941454 }, { 0.002236, 6.4e-005, 0.01055 }, { 0.00253539, 7.23e-005, 0.0119658 },
                    { 0.0028926, 8.22e-005, 0.0136559 }, { 0.00330083, 9.35e-005, 0.0155881 }, { 0.00375324, 0.000106136, 0.0177302 },
                    { 0.004243, 0.00012, 0.02005 }, { 0.00476239, 0.000134984, 0.0225114 }, { 0.00533005, 0.000151492, 0.0252029 },
                    { 0.00597871, 0.000170208, 0.0282797 }, { 0.00674112, 0.000191816, 0.031897 }, { 0.00765, 0.000217, 0.03621 },
                    { 0.00875137, 0.000246907, 0.0414377 }, { 0.0100289, 0.00028124, 0.0475037 }, { 0.0114217, 0.00031852, 0.0541199 },
                    { 0.012869, 0.000357267, 0.060998 }, { 0.01431, 0.000396, 0.06785 }, { 0.0157044, 0.000433715, 0.0744863 },
                    { 0.0171474, 0.000473024, 0.0813616 }, { 0.0187812, 0.000517876, 0.0891536 }, { 0.020748, 0.000572219, 0.0985405 },
                    { 0.02319, 0.00064, 0.1102 }, { 0.0262074, 0.00072456, 0.124613 }, { 0.0297825, 0.0008255, 0.141702 },
                    { 0.0338809, 0.00094116, 0.161303 }, { 0.0384682, 0.00106988, 0.183257 }, { 0.04351, 0.00121, 0.2074 },
                    { 0.0489956, 0.00136209, 0.233692 }, { 0.0550226, 0.00153075, 0.262611 }, { 0.0617188, 0.00172037, 0.294775 },
                    { 0.069212, 0.00193532, 0.330799 }, { 0.07763, 0.00218, 0.3713 }, { 0.0869581, 0.0024548, 0.416209 },
                    { 0.0971767, 0.002764, 0.465464 }, { 0.108406, 0.0031178, 0.519695 }, { 0.120767, 0.0035264, 0.57953 },
                    { 0.13438, 0.004, 0.6456 }, { 0.149358, 0.00454624, 0.718484 }, { 0.165396, 0.00515932, 0.796713 },
                    { 0.181983, 0.00582928, 0.877846 }, { 0.198611, 0.00654616, 0.959439 }, { 0.21477, 0.0073, 1.03905 },
                    { 0.230187, 0.00808651, 1.11537 }, { 0.24488, 0.00890872, 1.1885 }, { 0.258777, 0.00976768, 1.25812 },
                    { 0.271808, 0.0106644, 1.32393 }, { 0.2839, 0.0116, 1.3856 }, { 0.294944, 0.0125732, 1.44264 },
                    { 0.304897, 0.0135827, 1.4948 }, { 0.313787, 0.0146297, 1.54219 }, { 0.321645, 0.0157151, 1.58488 },
                    { 0.3285, 0.01684, 1.62296 }, { 0.334351, 0.0180074, 1.6564 }, { 0.33921, 0.0192145, 1.6853 },
                    { 0.343121, 0.0204539, 1.70987 }, { 0.34613, 0.0217182, 1.73038 }, { 0.34828, 0.023, 1.74706 },
                    { 0.3496, 0.0242946, 1.76004 }, { 0.350147, 0.0256102, 1.76962 }, { 0.350013, 0.0269586, 1.77626 },
                    { 0.349287, 0.0283513, 1.78043 }, { 0.34806, 0.0298, 1.7826 }, { 0.346373, 0.0313108, 1.78297 },
                    { 0.344262, 0.0328837, 1.7817 }, { 0.341809, 0.0345211, 1.7792 }, { 0.339094, 0.0362257, 1.77587 },
                    { 0.3362, 0.038, 1.77211 }, { 0.333198, 0.0398467, 1.76826 }, { 0.330041, 0.041768, 1.76404 },
                    { 0.326636, 0.043766, 1.75894 }, { 0.322887, 0.0458427, 1.75247 }, { 0.3187, 0.048, 1.7441 },
                    { 0.314025, 0.0502437, 1.73356 }, { 0.308884, 0.052573, 1.72086 }, { 0.30329, 0.0549806, 1.70594 },
                    { 0.297258, 0.0574587, 1.68874 }, { 0.2908, 0.06, 1.6692 }, { 0.28397, 0.062602, 1.64753 },
                    { 0.276721, 0.0652775, 1.62341 }, { 0.268918, 0.0680421, 1.59602 }, { 0.260423, 0.0709111, 1.56453 },
                    { 0.2511, 0.0739, 1.5281 }, { 0.240847, 0.077016, 1.48611 }, { 0.229851, 0.0802664, 1.43952 },
                    { 0.218407, 0.0836668, 1.38988 }, { 0.206812, 0.0872328, 1.33874 }, { 0.19536, 0.09098, 1.28764 },
                    { 0.184214, 0.0949176, 1.23742 }, { 0.173327, 0.0990458, 1.18782 }, { 0.162688, 0.103367, 1.13876 },
                    { 0.152283, 0.107885, 1.09015 }, { 0.1421, 0.1126, 1.0419 }, { 0.132179, 0.117532, 0.994198 },
                    { 0.12257, 0.122674, 0.947347 }, { 0.113275, 0.127993, 0.901453 }, { 0.104298, 0.133453, 0.856619 },
                    { 0.09564, 0.13902, 0.81295 }, { 0.0872996, 0.144676, 0.770517 }, { 0.079308, 0.150469, 0.729445 },
                    { 0.0717178, 0.156462, 0.689914 }, { 0.064581, 0.162718, 0.652105 }, { 0.05795, 0.1693, 0.6162 },
                    { 0.0518621, 0.176243, 0.582329 }, { 0.0462815, 0.183558, 0.550416 }, { 0.0411509, 0.191274, 0.520338 },
                    { 0.0364128, 0.199418, 0.491967 }, { 0.03201, 0.20802, 0.46518 }, { 0.0279172, 0.21712, 0.439925 },
                    { 0.0241444, 0.226735, 0.416184 }, { 0.020687, 0.236857, 0.393882 }, { 0.0175404, 0.247481, 0.372946 },
                    { 0.0147, 0.2586, 0.3533 }, { 0.0121618, 0.270185, 0.334858 }, { 0.00991996, 0.282294, 0.317552 },
                    { 0.00796724, 0.29505, 0.301337 }, { 0.00629635, 0.308578, 0.286169 }, { 0.0049, 0.323, 0.272 },
                    { 0.00377717, 0.338402, 0.258817 }, { 0.00294532, 0.354686, 0.246484 }, { 0.00242488, 0.371699, 0.234772 },
                    { 0.00223629, 0.389288, 0.223453 }, { 0.0024, 0.4073, 0.2123 }, { 0.00292552, 0.42563, 0.201169 },
                    { 0.00383656, 0.44431, 0.19012 }, { 0.00517484, 0.463394, 0.179225 }, { 0.00698208, 0.48294, 0.168561 },
                    { 0.0093, 0.503, 0.1582 }, { 0.0121495, 0.523569, 0.148138 }, { 0.0155359, 0.544512, 0.138376 },
                    { 0.0194775, 0.56569, 0.128994 }, { 0.0239928, 0.586965, 0.120075 }, { 0.0291, 0.6082, 0.1117 },
                    { 0.0348149, 0.629346, 0.103905 }, { 0.0411202, 0.650307, 0.0966675 }, { 0.047985, 0.670875, 0.0899827 },
                    { 0.0553786, 0.690842, 0.0838453 }, { 0.06327, 0.71, 0.07825 }, { 0.071635, 0.728185, 0.073209 },
                    { 0.0804622, 0.745464, 0.0686782 }, { 0.08974, 0.761969, 0.0645678 }, { 0.0994565, 0.777837, 0.0607883 },
                    { 0.1096, 0.7932, 0.05725 }, { 0.120167, 0.80811, 0.0539043 }, { 0.131114, 0.822496, 0.0507466 },
                    { 0.142368, 0.836307, 0.0477528 }, { 0.153854, 0.849492, 0.0448986 }, { 0.1655, 0.862, 0.04216 },
                    { 0.177257, 0.873811, 0.0395073 }, { 0.18914, 0.884962, 0.0369356 }, { 0.201169, 0.895494, 0.0344584 },
                    { 0.213366, 0.905443, 0.0320887 }, { 0.22575, 0.91485, 0.02984 }, { 0.238321, 0.923735, 0.0277118 },
                    { 0.251067, 0.932092, 0.0256944 }, { 0.263992, 0.939923, 0.0237872 }, { 0.277102, 0.947225, 0.0219892 },
                    { 0.2904, 0.954, 0.0203 }, { 0.303891, 0.960256, 0.0187181 }, { 0.317573, 0.966007, 0.0172404 },
                    { 0.331438, 0.971261, 0.0158636 }, { 0.345483, 0.976023, 0.0145846 }, { 0.3597, 0.9803, 0.0134 },
                    { 0.374084, 0.984092, 0.0123072 }, { 0.38864, 0.987418, 0.0113019 }, { 0.403378, 0.990313, 0.0103779 },
                    { 0.418312, 0.992812, 0.00952931 }, { 0.43345, 0.99495, 0.00875 }, { 0.448795, 0.996711, 0.0080352 },
                    { 0.464336, 0.998098, 0.0073816 }, { 0.480064, 0.999112, 0.0067854 }, { 0.495971, 0.999748, 0.0062428 },
                    { 0.51205, 1, 0.00575 }, { 0.528296, 0.999857, 0.0053036 }, { 0.544692, 0.999305, 0.0048998 },
                    { 0.561209, 0.998325, 0.0045342 }, { 0.577821, 0.996899, 0.0042024 }, { 0.5945, 0.995, 0.0039 },
                    { 0.611221, 0.992601, 0.0036232 }, { 0.627976, 0.989743, 0.0033706 }, { 0.64476, 0.986444, 0.0031414 },
                    { 0.66157, 0.982724, 0.0029348 }, { 0.6784, 0.9786, 0.00275 }, { 0.695239, 0.974084, 0.0025852 },
                    { 0.712059, 0.969171, 0.0024386 }, { 0.728828, 0.963857, 0.0023094 }, { 0.745519, 0.958135, 0.0021968 },
                    { 0.7621, 0.952, 0.0021 }, { 0.778543, 0.94545, 0.00201773 }, { 0.794826, 0.938499, 0.0019482 },
                    { 0.810926, 0.931163, 0.0018898 }, { 0.826825, 0.923458, 0.00184093 }, { 0.8425, 0.9154, 0.0018 },
                    { 0.857932, 0.907006, 0.00176627 }, { 0.873082, 0.898277, 0.0017378 }, { 0.887894, 0.889205, 0.0017112 },
                    { 0.902318, 0.879782, 0.00168307 }, { 0.9163, 0.87, 0.00165 }, { 0.9298, 0.859861, 0.00161013 },
                    { 0.942798, 0.849392, 0.0015644 }, { 0.955278, 0.838622, 0.0015136 }, { 0.967218, 0.827581, 0.00145853 },
                    { 0.9786, 0.8163, 0.0014 }, { 0.989386, 0.804795, 0.00133667 }, { 0.999549, 0.793082, 0.00127 },
                    { 1.00909, 0.781192, 0.001205 }, { 1.01801, 0.769155, 0.00114667 }, { 1.0263, 0.757, 0.0011 },
                    { 1.03398, 0.744754, 0.0010688 }, { 1.04099, 0.732422, 0.0010494 }, { 1.04719, 0.720004, 0.0010356 },
                    { 1.05247, 0.707496, 0.0010212 }, { 1.0567, 0.6949, 0.001 }, { 1.05979, 0.682219, 0.00096864 },
                    { 1.0618, 0.669472, 0.00092992 }, { 1.06281, 0.656674, 0.00088688 }, { 1.06291, 0.643845, 0.00084256 },
                    { 1.0622, 0.631, 0.0008 }, { 1.06074, 0.618155, 0.00076096 }, { 1.05844, 0.605314, 0.00072368 },
                    { 1.05522, 0.592476, 0.00068592 }, { 1.05098, 0.579638, 0.00064544 }, { 1.0456, 0.5668, 0.0006 },
                    { 1.03904, 0.553961, 0.000547867 }, { 1.03136, 0.541137, 0.0004916 }, { 1.02267, 0.528353, 0.0004354 },
                    { 1.01305, 0.515632, 0.000383467 }, { 1.0026, 0.503, 0.00034 }, { 0.991367, 0.490469, 0.000307253 },
                    { 0.979331, 0.47803, 0.00028316 }, { 0.966492, 0.465678, 0.00026544 }, { 0.952848, 0.453403, 0.000251813 },
                    { 0.9384, 0.4412, 0.00024 }, { 0.923194, 0.42908, 0.000229547 }, { 0.907244, 0.417036, 0.00022064 },
                    { 0.890502, 0.405032, 0.00021196 }, { 0.87292, 0.393032, 0.000202187 }, { 0.85445, 0.381, 0.00019 },
                    { 0.835084, 0.368918, 0.000174213 }, { 0.814946, 0.356827, 0.00015564 }, { 0.794186, 0.344777, 0.00013596 },
                    { 0.772954, 0.332818, 0.000116853 }, { 0.7514, 0.321, 0.0001 }, { 0.729584, 0.309338, 8.61e-005 },
                    { 0.707589, 0.29785, 7.46e-005 }, { 0.685602, 0.286594, 6.5e-005 }, { 0.66381, 0.275624, 5.69e-005 },
                    { 0.6424, 0.265, 5e-005 }, { 0.621515, 0.254763, 4.416e-005 }, { 0.601114, 0.24489, 3.948e-005 },
                    { 0.581105, 0.235334, 3.572e-005 }, { 0.561398, 0.226053, 3.264e-005 }, { 0.5419, 0.217, 3e-005 },
                    { 0.522599, 0.208162, 2.77e-005 }, { 0.503546, 0.199549, 2.556e-005 }, { 0.484744, 0.191155, 2.364e-005 },
                    { 0.466194, 0.182974, 2.18e-005 }, { 0.4479, 0.175, 2e-005 }, { 0.429861, 0.167224, 1.81e-005 },
                    { 0.412098, 0.159646, 1.62e-005 }, { 0.394644, 0.152278, 1.42e-005 }, { 0.377533, 0.145126, 1.21e-005 },
                    { 0.3608, 0.1382, 1e-005 }, { 0.344456, 0.1315, 7.73e-006 }, { 0.328517, 0.125025, 5.4e-006 },
                    { 0.313019, 0.118779, 3.2e-006 }, { 0.298001, 0.112769, 1.33e-006 }, { 0.2835, 0.107, 0 },
                    { 0.269545, 0.101476, 0 }, { 0.256118, 0.0961886, 0 }, { 0.24319, 0.091123, 0 },
                    { 0.230727, 0.0862649, 0 }, { 0.2187, 0.0816, 0 }, { 0.207097, 0.0771206, 0 },
                    { 0.195923, 0.0728255, 0 }, { 0.185171, 0.0687101, 0 }, { 0.174832, 0.0647698, 0 },
                    { 0.1649, 0.061, 0 }, { 0.155367, 0.0573962, 0 }, { 0.14623, 0.053955, 0 },
                    { 0.13749, 0.0506738, 0 }, { 0.129147, 0.0475496, 0 }, { 0.1212, 0.04458, 0 },
                    { 0.11364, 0.0417587, 0 }, { 0.106465, 0.039085, 0 }, { 0.0996904, 0.0365638, 0 },
                    { 0.0933306, 0.0342005, 0 }, { 0.0874, 0.032, 0 }, { 0.081901, 0.0299626, 0 },
                    { 0.0768043, 0.0280766, 0 }, { 0.0720771, 0.0263294, 0 }, { 0.0676866, 0.024708, 0 },
                    { 0.0636, 0.0232, 0 }, { 0.0598069, 0.0218008, 0 }, { 0.0562822, 0.0205011, 0 },
                    { 0.052971, 0.0192811, 0 }, { 0.0498186, 0.0181207, 0 }, { 0.04677, 0.017, 0 },
                    { 0.043784, 0.0159038, 0 }, { 0.0408754, 0.0148372, 0 }, { 0.0380726, 0.0138107, 0 },
                    { 0.0354046, 0.0128348, 0 }, { 0.0329, 0.01192, 0 }, { 0.0305642, 0.0110683, 0 },
                    { 0.0283806, 0.0102734, 0 }, { 0.0263448, 0.00953331, 0 }, { 0.0244527, 0.00884616, 0 },
                    { 0.0227, 0.00821, 0 }, { 0.0210843, 0.00762378, 0 }, { 0.0195999, 0.00708542, 0 },
                    { 0.0182373, 0.00659148, 0 }, { 0.0169872, 0.00613848, 0 }, { 0.01584, 0.005723, 0 },
                    { 0.0147906, 0.00534306, 0 }, { 0.0138313, 0.0049958, 0 }, { 0.0129487, 0.0046764, 0 },
                    { 0.0121292, 0.00438007, 0 }, { 0.0113592, 0.004102, 0 }, { 0.0106293, 0.00383845, 0 },
                    { 0.00993885, 0.0035891, 0 }, { 0.00928842, 0.00335422, 0 }, { 0.00867885, 0.00313409, 0 },
                    { 0.00811092, 0.002929, 0 }, { 0.00758239, 0.00273814, 0 }, { 0.00708875, 0.00255988, 0 },
                    { 0.00662731, 0.00239324, 0 }, { 0.00619541, 0.00223727, 0 }, { 0.00579035, 0.002091, 0 },
                    { 0.00540983, 0.00195359, 0 }, { 0.00505258, 0.00182458, 0 }, { 0.00471751, 0.00170358, 0 },
                    { 0.00440351, 0.00159019, 0 }, { 0.00410946, 0.001484, 0 }, { 0.00383391, 0.0013845, 0 },
                    { 0.00357575, 0.00129127, 0 }, { 0.00333434, 0.00120409, 0 }, { 0.00310908, 0.00112274, 0 },
                    { 0.00289933, 0.001047, 0 }, { 0.00270435, 0.00097659, 0 }, { 0.00252302, 0.000911109, 0 },
                    { 0.00235417, 0.000850133, 0 }, { 0.00219662, 0.000793238, 0 }, { 0.00204919, 0.00074, 0 },
                    { 0.00191096, 0.000690083, 0 }, { 0.00178144, 0.00064331, 0 }, { 0.00166011, 0.000599496, 0 },
                    { 0.00154646, 0.000558455, 0 }, { 0.00143997, 0.00052, 0 }, { 0.00134004, 0.000483914, 0 },
                    { 0.00124628, 0.000450053, 0 }, { 0.00115847, 0.000418345, 0 }, { 0.00107643, 0.000388718, 0 },
                    { 0.000999949, 0.0003611, 0 }, { 0.000928736, 0.000335384, 0 }, { 0.000862433, 0.00031144, 0 },
                    { 0.00080075, 0.000289166, 0 }, { 0.000743396, 0.000268454, 0 }, { 0.000690079, 0.0002492, 0 },
                    { 0.000640516, 0.000231302, 0 }, { 0.000594502, 0.000214686, 0 }, { 0.000551865, 0.000199288, 0 },
                    { 0.000512429, 0.000185048, 0 }, { 0.000476021, 0.0001719, 0 }, { 0.000442454, 0.000159778, 0 },
                    { 0.000411512, 0.000148604, 0 }, { 0.000382981, 0.000138302, 0 }, { 0.000356649, 0.000128793, 0 },
                    { 0.000332301, 0.00012, 0 }, { 0.000309759, 0.00011186, 0 }, { 0.000288887, 0.000104322, 0 },
                    { 0.000269539, 9.73e-005, 0 }, { 0.000251568, 9.08e-005, 0 }, { 0.000234826, 8.48e-005, 0 },
                    { 0.000219171, 7.91e-005, 0 }, { 0.000204526, 7.3858e-005, 0 }, { 0.000190841, 6.8916e-005, 0 },
                    { 0.000178065, 6.43e-005, 0 }, { 0.000166151, 6e-005, 0 }, { 0.000155024, 5.6e-005, 0 },
                    { 0.000144622, 5.22e-005, 0 }, { 0.00013491, 4.87e-005, 0 }, { 0.000125852, 4.54e-005, 0 },
                    { 0.000117413, 4.24e-005, 0 }, { 0.000109552, 3.96e-005, 0 }, { 0.000102225, 3.69e-005, 0 },
                    { 9.54e-005, 3.44e-005, 0 }, { 8.9e-005, 3.21e-005, 0 }, { 8.31e-005, 3e-005, 0 },
                    { 7.75e-005, 2.8e-005, 0 }, { 7.23e-005, 2.61e-005, 0 }, { 6.75e-005, 2.44e-005, 0 },
                    { 6.29e-005, 2.27e-005, 0 }, { 5.87e-005, 2.12e-005, 0 }, { 5.48e-005, 1.98e-005, 0 },
                    { 5.11e-005, 1.85e-005, 0 }, { 4.77e-005, 1.72e-005, 0 }, { 4.45e-005, 1.61e-005, 0 },
                    { 4.15e-005, 1.499e-005, 0 }, { 3.87e-005, 1.4e-005, 0 }, { 3.61e-005, 1.31e-005, 0 },
                    { 3.37e-005, 1.22e-005, 0 }, { 3.15e-005, 1.14e-005, 0 }, { 2.94e-005, 1.06e-005, 0 },
                    { 2.74e-005, 9.89e-006, 0 }, { 2.55e-005, 9.22e-006, 0 }, { 2.38e-005, 8.59e-006, 0 },
                    { 2.22e-005, 8.01e-006, 0 }, { 2.07e-005, 7.47e-006, 0 }, { 1.93e-005, 6.96e-006, 0 },
                    { 1.8e-005, 6.49e-006, 0 }, { 1.67e-005, 6.05e-006, 0 }, { 1.56e-005, 5.64e-006, 0 },
                    { 1.46e-005, 5.26e-006, 0 }, { 1.36e-005, 4.9e-006, 0 }, { 1.27e-005, 4.57e-006, 0 },
                    { 1.18e-005, 4.26e-006, 0 }, { 1.1e-005, 3.97e-006, 0 }, { 1.03e-005, 3.7e-006, 0 },
                    { 9.56e-006, 3.45e-006, 0 }, { 8.91e-006, 3.22e-006, 0 }, { 8.31e-006, 3e-006, 0 },
                    { 7.75e-006, 2.8e-006, 0 }, { 7.22e-006, 2.61e-006, 0 }, { 6.73e-006, 2.43e-006, 0 },
                    { 6.28e-006, 2.27e-006, 0 }, { 5.85e-006, 2.11e-006, 0 }, { 5.46e-006, 1.97e-006, 0 },
                    { 5.09e-006, 1.84e-006, 0 }, { 4.74e-006, 1.71e-006, 0 }, { 4.42e-006, 1.6e-006, 0 },
                    { 4.12e-006, 1.49e-006, 0 }, { 3.84e-006, 1.39e-006, 0 }, { 3.58e-006, 1.29e-006, 0 },
                    { 3.34e-006, 1.21e-006, 0 }, { 3.11e-006, 1.12e-006, 0 }, { 2.9e-006, 1.05e-006, 0 },
                    { 2.71e-006, 9.77e-007, 0 }, { 2.52e-006, 9.11e-007, 0 }, { 2.35e-006, 8.49e-007, 0 },
                    { 2.19e-006, 7.92e-007, 0 }, { 2.04e-006, 7.38e-007, 0 }, { 1.91e-006, 6.88e-007, 0 },
                    { 1.78e-006, 6.42e-007, 0 }, { 1.66e-006, 5.98e-007, 0 }, { 1.54e-006, 5.58e-007, 0 },
                    { 1.44e-006, 5.2e-007, 0 }, { 1.34e-006, 4.85e-007, 0 }, { 1.25e-006, 4.52e-007, 0 }
                };

                return table[i][j];
            }

            //! View table as 3D histogram binned on wavelength.
            static int find_bin( double wavelength )
            {
                if( wavelength < min_wavelength() || wavelength > max_wavelength() )
                    return -1;

                return (int)((wavelength - min_wavelength()) / bin_width());
            }

            template <unsigned int Axis>
            static double get(double spectralIntensity, double wavelength)
            {
                int bin = find_bin(wavelength);
                if( bin < 0 )
                    return 0.;

                double waveLo = min_wavelength() + (double)bin * bin_width();
                double xlo = spectralIntensity * get_value(bin, Axis);
                double xhi = spectralIntensity * get_value(bin+1, Axis);
                return xlo + (xhi-xlo) * ((wavelength-waveLo)/bin_width());
            }

        };

        inline void wavelength_to_xyz(double spec_intens, double lambda, double& x, double& y, double& z)
        {
            /* CIE colour matching functions xBar, yBar, and zBar for
               wavelengths from 380 through 780 nanometers, every 5
               nanometers.  For a wavelength lambda in this range:

                    cie_colour_match[(lambda - 380) / 5][0] = xBar
                    cie_colour_match[(lambda - 380) / 5][1] = yBar
                    cie_colour_match[(lambda - 380) / 5][2] = zBar

            To save memory, this table can be declared as floats
            rather than doubles; (IEEE) float has enough
            significant bits to represent the values. It's declared
            as a double here to avoid warnings about "conversion
            between floating-point types" from certain persnickety
            compilers. */

            x = color_table::get<color_table::X>(spec_intens, lambda);
            y = color_table::get<color_table::Y>(spec_intens, lambda);
            z = color_table::get<color_table::Z>(spec_intens, lambda);
        }

        template <typename SpectralIntensity>
        inline void spectrum_to_xyz(SpectralIntensity spec_intens, double& x, double& y, double& z)
        {
            double XYZ;

            x = 0;
            y = 0;
            z = 0;
            for(double lambda = color_table::min_wavelength(); lambda < color_table::max_wavelength(); lambda += color_table::bin_width())
            {
                double Me = spec_intens(lambda);
                double a,b,c;
                wavelength_to_xyz(Me, lambda, a, b, c);
                x += a;
                y += b;
                z += c;
            }

            XYZ = (x + y + z);
            x /= XYZ;
            y /= XYZ;
            z /= XYZ;
        }

        /*  Built-in test program which displays the x, y, and Z and RGB
            values for black body spectra from 1000 to 10000 degrees kelvin.
            When run, this program should produce the following output:

            Temperature       x      y      z       R     G     B
            -----------    ------ ------ ------   ----- ----- -----
               1000 K      0.6528 0.3444 0.0028   1.000 0.007 0.000 (Approximation)
               1500 K      0.5857 0.3931 0.0212   1.000 0.126 0.000 (Approximation)
               2000 K      0.5267 0.4133 0.0600   1.000 0.234 0.010
               2500 K      0.4770 0.4137 0.1093   1.000 0.349 0.067
               3000 K      0.4369 0.4041 0.1590   1.000 0.454 0.151
               3500 K      0.4053 0.3907 0.2040   1.000 0.549 0.254
               4000 K      0.3805 0.3768 0.2428   1.000 0.635 0.370
               4500 K      0.3608 0.3636 0.2756   1.000 0.710 0.493
               5000 K      0.3451 0.3516 0.3032   1.000 0.778 0.620
               5500 K      0.3325 0.3411 0.3265   1.000 0.837 0.746
               6000 K      0.3221 0.3318 0.3461   1.000 0.890 0.869
               6500 K      0.3135 0.3237 0.3628   1.000 0.937 0.988
               7000 K      0.3064 0.3166 0.3770   0.907 0.888 1.000
               7500 K      0.3004 0.3103 0.3893   0.827 0.839 1.000
               8000 K      0.2952 0.3048 0.4000   0.762 0.800 1.000
               8500 K      0.2908 0.3000 0.4093   0.711 0.766 1.000
               9000 K      0.2869 0.2956 0.4174   0.668 0.738 1.000
               9500 K      0.2836 0.2918 0.4246   0.632 0.714 1.000
              10000 K      0.2807 0.2884 0.4310   0.602 0.693 1.000
        */

        template <typename CSystem>
        struct color_converter
        {
        private:

            //! BB_SPECTRUM
            //!  Calculate, by Planck's radiation law, the emittance of a black body
            //!  of temperature bbTemp at the given wavelength (in metres).  */
            static double bb_spectrum(double wavelength, double temp)
            {
                double wlm = wavelength * 1e-9;   /* Wavelength in meters */

                return (3.74183e-16 * pow(wlm, -5.0)) /
                       (exp(1.4388e-2 / (wlm * temp)) - 1.0);
            }

        public:

            static color_rgba apply_wavelength(double wavelength, double temp)
            {
                using namespace detail;

                double x, y, z, r, g, b;
                static color_system cs = CSystem();
                double spec = bb_spectrum(wavelength, temp);
                wavelength_to_xyz(spec, wavelength, x, y, z);
                xyz_to_rgb(&cs, x, y, z, &r, &g, &b);
                constrain_rgb(&r, &g, &b);
                norm_rgb(&r, &g, &b);
                return color_rgba(255*r,255*g,255*b);
            }

            static color_rgba apply_temp(double temp)
            {
                using namespace detail;
                auto spectralIntensityMap = [temp](double wavelength)
                {
                    double wlm = wavelength * 1e-9;   /* Wavelength in meters */
                    return (3.74183e-16 * pow(wlm, -5.0)) / (exp(1.4388e-2 / (wlm * temp)) - 1.0);
                };

                double x, y, z, r, g, b;
                static color_system cs = CSystem();
                spectrum_to_xyz(spectralIntensityMap, x, y, z);
                xyz_to_rgb(&cs, x, y, z, &r, &g, &b);
                if (constrain_rgb(&r, &g, &b))
                    norm_rgb(&r, &g, &b);
                else
                    norm_rgb(&r, &g, &b);

                return color_rgba(255*r,255*g,255*b);
            }
        };
    }//! namespace detail;

    struct spectral_color
    {
        spectral_color(double l, color_rgba c)
            : data(l,l,c)
        {}

        spectral_color(double l, double u, color_rgba c)
            : data(l,u,c)
        {}

        bool operator()(const spectral_color& rhs) const
        {
            return data < rhs.data;
        }

        const double& upper() const { return data.get<1>(); }
        const double& lower() const { return data.get<0>(); }
        const color_rgba& colour() const { return data.get<2>(); }
        double& upper() { return data.get<1>(); }
        double& lower() { return data.get<0>(); }
        color_rgba& colour() { return data.get<2>(); }

    private:

        boost::tuple<double,double, color_rgba> data;

    };

    template <typename System>
    inline std::vector<spectral_color> generate_colors(double interval = detail::color_table::bin_width(), double startWavelength = detail::color_table::min_wavelength(), double endWavelength = detail::color_table::max_wavelength(), double colourTemp = 5500.)
    {
        std::size_t intervals = (endWavelength - startWavelength) / interval;
        std::vector<spectral_color> c;
        c.reserve(intervals);
        for( std::size_t i = 0; i < intervals; ++i )
        {
            double l = startWavelength + (double)i * interval;
            color_rgba colour = detail::color_converter<System>::apply_wavelength(l, colourTemp);
            if (c.empty() || c.back().colour() != colour)
                c.emplace_back(l, l + interval, colour);
            else
                c.back().upper() = l;
        }

        return c;
    }

    template <typename CSystem = Rec709system>
    struct color_spectrum_mapper
    {
        BOOST_CONSTEXPR color_spectrum_mapper(double xmin, double xmax, double startWavelength = detail::color_table::min_wavelength(), double endWavelength = detail::color_table::max_wavelength(), double colorTemp = 5500.0)
            : m_xmin{xmin}
            , m_xmax{xmax}
            , m_start{startWavelength}
            , m_end{endWavelength}
            , m_temp{colorTemp}
            , m_slope{(endWavelength - startWavelength) / (xmax - xmin)}
        {}

        BOOST_CONSTEXPR color_rgba operator()( double x ) const BOOST_NOEXCEPT
        {
            double l = m_slope * (x-m_xmin) + m_start;
            return detail::color_converter<CSystem>::apply_wavelength(l, m_temp);
        }

    private:

        double m_xmin;
        double m_xmax;
        double m_start;
        double m_end;
        double m_temp;
        double m_slope;

    };

    template <typename CSystem = Rec709system>
    struct color_spectrum_mapper_temp
    {
        BOOST_CONSTEXPR color_spectrum_mapper_temp(double xmin, double xmax, double startTemp = 1000, double endTemp = 10000)
            : m_xmin{xmin}
            , m_xmax{xmax}
            , m_start{startTemp}
            , m_end{endTemp}
            , m_slope{(endTemp - startTemp) / (xmax - xmin)}
        {}

        BOOST_CONSTEXPR color_rgba operator()(double x) const BOOST_NOEXCEPT
        {
            double l = m_slope * (x-m_xmin) + m_start;
            return detail::color_converter<CSystem>::apply_temp(l);
        }

    private:

        double m_xmin;
        double m_xmax;
        double m_start;
        double m_end;
        double m_slope;

    };

}//namespace stk;
