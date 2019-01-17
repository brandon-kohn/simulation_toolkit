/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
ROOT Software Terms and Conditions

The authors hereby grant permission to use, copy, and distribute this
software and its documentation for any purpose, provided that existing
copyright notices are retained in all copies and that this notice is
included verbatim in any distributions.Additionally, the authors grant
permission to modify this software and its documentation for any purpose,
provided that such modifications are not distributed without the explicit
consent of the authors and that existing copyright notices are retained in
all copies.Users of the software are asked to feed back problems, benefits,
and/or suggestions about the software to the ROOT Development Team
(rootdev@root.cern.ch).Support for this software - fixing of bugs,
incorporation of new features - is done on a best effort basis.All bug
fixes and enhancements will be made available under the same terms and
conditions as the original software,


IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON - INFRINGEMENT.THIS SOFTWARE IS
PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE NO
OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.
*/
//! stk interface Based on Root V4 TH1F.
//! For internal use only. Not for distribution.
//! Copyright © 2015
//! Brandon Kohn 
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
/////////////////////////////////////////////////////////////////////////////
#pragma once

#include <geometrix/utility/assert.hpp>
#include <stk/sim/histogram_1d.hpp>
#include <algorithm>

namespace stk {

template <typename T>
inline axis<T>::axis()
: m_numBins(1)
, m_min(0)
, m_max(1)
, m_first(1)
, m_last(1)
{

}

template <typename T>
inline axis<T>::axis(std::size_t nbins, T xmin, T xmax)
	: m_numBins(nbins)
	, m_min(xmin)
	, m_max(xmax)
	, m_first(1)
	, m_last(nbins)
{

}

template <typename T>
inline axis<T>::axis(std::size_t nbins, const T *xbins)
{
    set(nbins, xbins);
}

template <typename T>
inline axis<T>::~axis()
{

}

template <typename T>
template <typename T2>
inline axis<T>::axis( const axis<T2>& other )
: m_numBins( other.m_numBins )
, m_min(boost::numeric_cast<T>(other.m_min))
, m_max(boost::numeric_cast<T>(other.m_max))
, m_first(other.m_first)
, m_last(other.m_last)
{
    for(std::size_t i=0; i < other.m_bins.size(); ++i )
        m_bins[i] = boost::numeric_cast<T>( other.m_bins[i] );
}

template <typename T>
template <typename T2>
inline axis<T>& axis<T>::operator =(const axis<T2>& rhs) 
{
    m_numBins = rhs.m_numBins;
    m_min = boost::numeric_cast<T>(rhs.m_min);
    m_max = boost::numeric_cast<T>(rhs.m_max);
    m_first = rhs.m_first;
    m_last = rhs.m_last;

    for(std::size_t i=0; i < rhs.m_bins.size(); ++i )
        m_bins[i] = boost::numeric_cast<T>( rhs.m_bins[i] );

    return *this;
}

template <typename T>
inline std::size_t axis<T>::find_bin(T x) const
{
    // Find bin number corresponding to abscissa x
	if (x > m_min)
	{
		if (x < m_max)
		{
			if (m_bins.empty())
			{
				//*-* fix bins
				return 1 + boost::numeric_cast<std::size_t>(m_numBins*(x - m_min) / (m_max - m_min));
			}
			else
			{
				//*-* variable bin sizes
				//for (bin =1; x >= m_bins[bin]; bin++);
				return 1 + std::distance(m_bins.begin(), std::lower_bound(m_bins.begin(), m_bins.end(), x));
			}
		}
		else
		{
			return m_numBins + 1;//!overflow
		}
	}
	else
	{
		return 0;//! underflow
	}    
}

template <typename T>
inline T axis<T>::get_bin_center(std::size_t bin) const
{
   // Return center of bin

   T binwidth;
   if (m_bins.empty() || bin<1 || bin > m_numBins)
   {
      binwidth = (m_max - m_min) / T(m_numBins);
      return m_min + (bin-1) * binwidth + 0.5*binwidth;
   }
   else
   {
      binwidth = m_bins[bin] - m_bins[bin-1];
      return m_bins[bin-1] + 0.5*binwidth;
   }
}

template <typename T>
inline T axis<T>::get_bin_center_log(std::size_t bin) const
{
   // Return center of bin in log
   // With a log-equidistant binning for a bin with low and up edges, the mean is : 
   // 0.5*(ln low + ln up) i.e. sqrt(low*up) in logx (e.g. sqrt(10^0*10^2) = 10). 
   //Imagine a bin with low=1 and up=100 : 
   // - the center in bin is (100-1)/2=50.5 
   // - the center in log would be sqrt(1*100)=10 (!=log(50.5)) 
   // NB: if the low edge of the bin is negative, the function returns the bin center
   //     as computed by axis<T>::get_bin_center
   
   T low,up;
   if(m_bins.empty())
   {
      T binwidth = (m_max - m_min) / T(m_numBins);
      low = m_min + (bin-1) * binwidth;
      up  = low+binwidth;
   }
   else
   {
      low = m_bins[bin-1];
      up  = m_bins[bin];
   }
   if (low <=0 ) 
       return get_bin_center(bin);

   return std::sqrt(low*up);
}

template <typename T>
inline T axis<T>::get_bin_low_edge(std::size_t bin) const
{
    // Return low edge of bin

    if(!m_bins.empty())
        return m_bins[bin-1];
    
    T binwidth = (m_max - m_min) / T(m_numBins);
    return m_min + (bin-1) * binwidth;
}

template <typename T>
inline T axis<T>::get_bin_up_edge(std::size_t bin) const
{
   // Return up edge of bin

   T binwidth;
   if (m_bins.empty())
   {
      binwidth = (m_max - m_min) / T(m_numBins);
      return m_min + bin*binwidth;
   }
   else 
   {
      binwidth = m_bins[bin] - m_bins[bin-1];
      return m_bins[bin-1] + binwidth;
   }
}

template <typename T>
inline T axis<T>::get_bin_width(std::size_t bin) const
{
    // Return bin width
    if (m_numBins == 0) 
        return 0;
    if (m_bins.empty())
        return (m_max - m_min) / T(m_numBins);
    
    return m_bins[bin] - m_bins[bin-1];
}

template <typename T>
inline void axis<T>::get_center(T *center) const
{
   // Return an array with the center of all bins

   std::size_t bin;
   for (bin=1; bin<=m_numBins; ++bin)
       *(center + bin-1) = get_bin_center(bin);
}

template <typename T>
inline void axis<T>::get_low_edge(T *edge) const
{
   // Return an array with the low edge of all bins

   std::size_t bin;
   for (bin=1; bin<=m_numBins; ++bin) 
       *(edge + bin-1) = get_bin_low_edge(bin);
}

template <typename T>
inline void axis<T>::set(std::size_t nbins, T xlow, T xup)
{
   // Initialize axis with fix bins
   m_numBins = nbins;
   m_min = xlow;
   m_max = xup;
   m_first = 1;
   m_last = nbins;
}

template <typename T>
inline void axis<T>::set(std::size_t nbins, const T *xbins)
{
    // Initialize axis with variable bins
    m_numBins = nbins;
    m_bins.resize(m_numBins+1);
    std::vector<T>(m_numBins+1,T()).swap(m_bins);
    
    for(std::size_t bin=0; bin<= m_numBins; ++bin)
        m_bins[bin] = xbins[bin];
    
    m_min = m_bins[0];
    m_max = m_bins[m_numBins];    

    m_first = 1;
    m_last = nbins;
}

template <typename T>
inline void axis<T>::set_limits(T xmin, T xmax)
{
    //set the axis limits    
    m_min = xmin;
    m_max = xmax;
}

#if defined(SIMULATION_USE_BOOST_SERIALIZATION)
template <typename T>
template <typename Archive>
inline void axis<T>::save( Archive& ar, boost::uint32_t /*version*/ ) const
{
    boost::uint32_t numBins = boost::numeric_cast<boost::uint32_t>(m_numBins);
    ar & BOOST_SERIALIZATION_NVP(numBins);
    ar & BOOST_SERIALIZATION_NVP(m_min);
    ar & BOOST_SERIALIZATION_NVP(m_max);
    ar & BOOST_SERIALIZATION_NVP(m_bins);
    boost::uint32_t first = boost::numeric_cast<boost::uint32_t>(m_first);
    ar & BOOST_SERIALIZATION_NVP(first);
    boost::uint32_t last = boost::numeric_cast<boost::uint32_t>(m_last);
    ar & BOOST_SERIALIZATION_NVP(last);
}

template <typename T>
template <typename Archive>
inline void axis<T>::load( Archive& ar, boost::uint32_t version )
{
    boost::uint32_t temp;
    ar & BOOST_SERIALIZATION_NVP(temp);
    m_numBins = temp;
    
    ar & BOOST_SERIALIZATION_NVP(m_min);
    ar & BOOST_SERIALIZATION_NVP(m_max);
    ar & BOOST_SERIALIZATION_NVP(m_bins);

    ar & BOOST_SERIALIZATION_NVP(temp);
    m_first = temp;

    ar & BOOST_SERIALIZATION_NVP(temp);
    m_last = temp;    
}
#endif 

template <typename T, typename Statistic, typename AxisType>
inline histogram_1d<T, Statistic, AxisType>::~histogram_1d()
{

}

template <typename T, typename Statistic, typename AxisType>
template <typename T2, typename Statistic2, typename AxisType2>
inline histogram_1d<T, Statistic, AxisType>::histogram_1d( const histogram_1d<T2, Statistic2, AxisType2>& other )
: m_axis(other.m_axis)
, m_counts(boost::numeric_cast<Statistic>(other.m_counts))
, m_totalSumWeights(boost::numeric_cast<Statistic>(other.m_totalSumWeights))
, m_totalSumWeightsSquares(boost::numeric_cast<Statistic>(other.m_totalSumWeightsSquares))
, m_totalSumWeightsSquaresX(boost::numeric_cast<Statistic>(other.m_totalSumWeightsSquaresX))
, m_totalSumWeightsSquaresX2(boost::numeric_cast<Statistic>(other.m_totalSumWeightsSquaresX2))
, m_normFactor(boost::numeric_cast<Statistic>(other.m_normFactor))
, m_bins(other.m_bins.size(), T())
{
    for( std::size_t i = 0; i < m_bins.size(); ++i )
    {
        m_bins[i] = boost::numeric_cast<T>(other.m_bins[i]);
    }
}	

template <typename T, typename Statistic, typename AxisType>
template <typename U, typename Statistic2, typename AxisType2>
inline bool histogram_1d<T, Statistic, AxisType>::operator == (const histogram_1d<U, Statistic2, AxisType2>& rhs) const
{
    return get_xmin() == rhs.get_xmin() && 
        get_xmax() == rhs.get_xmax() &&
        m_bins.size() == rhs.m_bins.size() &&
        std::equal( m_bins.begin(), m_bins.end(), rhs.m_bins.begin() );
}

template <typename T, typename Statistic, typename AxisType>
template <typename T2, typename Statistic2, typename AxisType2>
inline histogram_1d<T, Statistic, AxisType>& histogram_1d<T, Statistic, AxisType>::operator = (const histogram_1d<T2, Statistic2, AxisType2>& rhs)
{
    m_bins.resize(rhs.m_bins.size(), T());
    m_counts = boost::numeric_cast<Statistic>(rhs.get_counts());
    m_normFactor = (boost::numeric_cast<Statistic>(rhs.get_norm_factor()));
    m_totalSumWeights = boost::numeric_cast<Statistic>(rhs.m_totalSumWeights);
    m_totalSumWeightsSquares = boost::numeric_cast<Statistic>(rhs.m_totalSumWeightsSquares);
    m_totalSumWeightsSquaresX = boost::numeric_cast<Statistic>(rhs.m_totalSumWeightsSquaresX);
    m_totalSumWeightsSquaresX2 = boost::numeric_cast<Statistic>(rhs.m_totalSumWeightsSquaresX2);
    m_axis = rhs.m_axis;

    for( std::size_t i = 0; i < rhs.m_bins.size(); ++i )
    {
        m_bins[i] = boost::numeric_cast<T>(rhs.m_bins[i]);
    }

    return *this;
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::get_stats(std::vector<Statistic>& stats) const
{
    // fill the array stats from the contents of this histogram
    // The array stats must be correctly dimensioned in the calling program.
    // stats[0] = sumw
    // stats[1] = sumw2
    // stats[2] = sumwx
    // stats[3] = sumwx2
    //
    // If no axis-subrange is specified (via TAxis::SetRange), the array stats
    // is simply a copy of the statistics quantities computed at filling time.
    // If a sub-range is specified, the function recomputes these quantities
    // from the bin contents in the current axis range.
    //
    //  Note that the mean value/RMS is computed using the bins in the currently
    //  defined range (see TAxis::SetRange). By default the range includes
    //  all bins from 1 to nbins included, excluding underflows and overflows.
    //  To force the underflows and overflows in the computation, one must
    //  call the static function TH1::StatOverflows(kTRUE) before filling
    //  the histogram.

    // Loop on bins (possibly including underflows/overflows)
    std::size_t binx;
    statistic_type w,err;
    axis_type x;
    
    if(m_totalSumWeights == 0 && m_counts > 0 && !m_bins.empty()) 
    {
        std::size_t firstBinX = m_axis.get_first_bin();
        std::size_t lastBinX  = m_axis.get_last_bin();
        // include underflow/overflow if TH1::StatOverflows(kTRUE) in case no range is set on the axis
        //             if (fgStatOverflows && !fXaxis.TestBit(TAxis::kAxisRange)) {
        //                 if (firstBinX == 1) firstBinX = 0;
        //                 if (lastBinX ==  fXaxis.GetNbins() ) lastBinX += 1;
        //             }
        for (binx = firstBinX; binx <= lastBinX; ++binx) 
        {
            x = m_axis.get_bin_center(binx);
            w = std::abs(get_bin_content(binx));
            err = std::abs(get_bin_error(binx));
            stats[0] += w;
            stats[1] += err*err;
            stats[2] += w*x;
            stats[3] += w*x*x;
        }
    } 
    else
    {
        stats[0] = m_totalSumWeights;
        stats[1] = m_totalSumWeightsSquares;
        stats[2] = m_totalSumWeightsSquaresX;
        stats[3] = m_totalSumWeightsSquaresX2;
    }
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::put_stats(const std::vector<Statistic>& stats)
{
    // Replace current statistics with the values in array stats
    m_totalSumWeights = stats[0];
    m_totalSumWeightsSquares = stats[1];
    m_totalSumWeightsSquaresX = stats[2];
    m_totalSumWeightsSquaresX2 = stats[3];
}

#if defined(SIMULATION_USE_BOOST_SERIALIZATION)
template <typename T, typename Statistic, typename AxisType>
template <typename Archive>
inline void histogram_1d<T, Statistic, AxisType>::save( Archive& ar, boost::uint32_t /*version*/ ) const
{
    ar & BOOST_SERIALIZATION_NVP(m_axis);
    ar & BOOST_SERIALIZATION_NVP(m_counts);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeights);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeightsSquares);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeightsSquaresX);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeightsSquaresX2);
    ar & BOOST_SERIALIZATION_NVP(m_normFactor);
    ar & BOOST_SERIALIZATION_NVP(m_bins);
}

template <typename T, typename Statistic, typename AxisType>
template <typename Archive>
inline void histogram_1d<T, Statistic, AxisType>::load( Archive& ar, boost::uint32_t version )
{
    ar & BOOST_SERIALIZATION_NVP(m_axis);

    ar & BOOST_SERIALIZATION_NVP(m_counts);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeights);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeightsSquares);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeightsSquaresX);
    ar & BOOST_SERIALIZATION_NVP(m_totalSumWeightsSquaresX2);
    ar & BOOST_SERIALIZATION_NVP(m_normFactor);
    
    ar & BOOST_SERIALIZATION_NVP(m_bins);
}
#endif

template <typename T, typename Statistic, typename AxisType>
inline histogram_1d<T, Statistic, AxisType>::histogram_1d()
    : m_counts()
    , m_totalSumWeights()
    , m_totalSumWeightsSquares()
    , m_totalSumWeightsSquaresX()
    , m_totalSumWeightsSquaresX2()
	, m_normFactor()
{}

template <typename T, typename Statistic, typename AxisType>
inline histogram_1d<T, Statistic, AxisType>::histogram_1d(std::size_t nbins, AxisType xlow, AxisType xup)    
    : m_axis(nbins ? nbins : 1, xlow, xup)
    , m_counts()
    , m_totalSumWeights()
    , m_totalSumWeightsSquares()
    , m_totalSumWeightsSquaresX()
    , m_totalSumWeightsSquaresX2()
	, m_normFactor()
	, m_bins(nbins ? nbins + 2 : 3, T())
{
    //   -*-*-*-*-*-*-*Normal constructor for fix bin size histograms*-*-*-*-*-*-*
    //                 ==============================================
    //
    //     Creates the main histogram structure:
    //        name   : name of histogram (avoid blanks)
    //        title  : histogram title
    //                 if title is of the form "stringt;stringx;stringy;stringz"
    //                 the histogram title is set to string,
    //                 the x axis title to stringy, the y axis title to stringy, etc.
    //        nbins  : number of bins
    //        xlow   : low edge of first bin
    //        xup    : upper edge of last bin (not included in last bin)
    //
    //      When an histogram is created, it is automatically added to the list
    //      of special objects in the current directory.
    //      To find the pointer to this histogram in the current directory
    //      by its name, do:
    //      CHistogram1DF *h1 = (CHistogram1DF*)gDirectory->FindObject(name);
    //
    //   -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
// 
//     if (nbins == 0)
//         nbins = 1;
//     m_axis.set(nbins,xlow,xup);
//     nbins = m_axis.get_number_bins()+2;
//     m_bins.resize(nbins, T());
}

template <typename T, typename Statistic, typename AxisType>
inline histogram_1d<T, Statistic, AxisType>::histogram_1d(std::size_t nbins,const AxisType *xbins)
    : m_counts()
    , m_totalSumWeights()
    , m_totalSumWeightsSquares()
    , m_totalSumWeightsSquaresX()
    , m_totalSumWeightsSquaresX2()
	, m_normFactor()
	, m_bins(nbins + 2, T())
{
    //   -*-*-*-*-*Normal constructor for variable bin size histograms*-*-*-*-*-*-*
    //             ===================================================
    //
    //  Creates the main histogram structure:
    //     nbins  : number of bins
    //     xbins  : array of low-edges for each bin
    //              This is an array of size nbins+1
    //
    //   -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    if (nbins == 0)
        nbins = 1;
    if (xbins)
        m_axis.set(nbins,xbins);
    else
        m_axis.set(nbins,0,1);

    nbins = m_axis.get_number_bins()+2;
    m_bins.resize(nbins, T());
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::compute_integral() const
{
    //  Compute integral (cumulative sum of bins)
    //  The result stored in m_integral is used by the get_random functions.
    //  This function is automatically called by get_random when the m_integral
    //  array does not exist or when the number of entries in the histogram
    //  has changed since the previous call to get_random.
    //  The resulting integral is normalized to 1
    std::size_t bin, ibin;

    // delete previously computed integral (if any)
    if (!m_integral.empty()) 
        m_integral.clear();

    //   - Allocate space to store the integral and compute integral
    std::size_t nbins = get_number_bins();

    m_integral.resize(nbins+2);
    ibin = 0;
    m_integral[ibin] = 0;
    for(bin=1;bin<=nbins;++bin) 
    {
        ++ibin;
        m_integral[ibin] = m_integral[ibin-1] + get_bin_content(bin);
    }

    //   - Normalize integral to 1
    if (m_integral[nbins] == 0 ) 
    {
        //Error("compute_integral", "integral = zero"); 
        return 0;
    }

    for (bin=1;bin<=nbins;++bin)
        m_integral[bin] /= m_integral[nbins];
    m_integral[nbins+1] = m_counts;
    return m_integral[nbins];
}

template <typename T, typename Statistic, typename AxisType>
inline std::size_t histogram_1d<T, Statistic, AxisType>::fill(AxisType x)
{
    //   -*-*-*-*-*-*-*-*Increment bin with abscissa X by 1*-*-*-*-*-*-*-*-*-*-*
    //                   ==================================
    //
    //    if x is less than the low-edge of the first bin, the Underflow bin is incremented
    //    if x is greater than the upper edge of last bin, the Overflow bin is incremented
    //
    //    If the storage of the sum of squares of weights has been triggered,
    //    via the function sumw2, then the sum of the squares of weights is incremented
    //    by 1 in the bin corresponding to x.
    //
    //   -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    ++m_counts;
    std::size_t bin = m_axis.find_bin(x);
    add_bin_content(bin);
    
    ++m_totalSumWeights;
    ++m_totalSumWeightsSquares;
    m_totalSumWeightsSquaresX  += x;
    m_totalSumWeightsSquaresX2 += x*x;
    return bin;
}

template <typename T, typename Statistic, typename AxisType>
inline std::size_t histogram_1d<T, Statistic, AxisType>::fill(AxisType x, T w)
{
    //   -*-*-*-*-*-*Increment bin with abscissa X with a weight w*-*-*-*-*-*-*-*
    //               =============================================
    //
    //    if x is less than the low-edge of the first bin, the Underflow bin is incremented
    //    if x is greater than the upper edge of last bin, the Overflow bin is incremented
    //
    //    If the storage of the sum of squares of weights has been triggered,
    //    via the function sumw2, then the sum of the squares of weights is incremented
    //    by w^2 in the bin corresponding to x.
    //
    //   -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
    ++m_counts;
    std::size_t bin = m_axis.find_bin(x);
    add_bin_content(bin, w);
    
    statistic_type z = w;
    m_totalSumWeights += z;
    m_totalSumWeightsSquares += z*z;
    m_totalSumWeightsSquaresX  += z*x;
    m_totalSumWeightsSquaresX2 += z*x*x;
    return bin;
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_counts() const
{
    // return the current number of entries
    return m_counts;
}

template <typename T, typename Statistic, typename AxisType>
inline T histogram_1d<T, Statistic, AxisType>::get_random(T r1) const
{
    // return a random number distributed according the histogram bin contents.
    // This function checks if the bins integral exists. If not, the integral
    // is evaluated, normalized to one.
    // The integral is automatically recomputed if the number of entries
    // is not the same then when the integral was computed.
    // NB Only valid for 1-d histograms. Use get_random2 or 3 otherwise.

	statistic_type integral = get_integral();    
    if (integral == 0)
        return 0;
        
    auto it(std::lower_bound(m_integral.begin(), m_integral.end(), r1));
    std::size_t ibin = std::distance(m_integral.begin(), it);
    if (it == m_integral.end() || *it != r1)
        --ibin;
    axis_type x = get_bin_low_edge(ibin+1);
    if (r1 > m_integral[ibin]) 
        x += get_bin_width(ibin+1)*(r1-m_integral[ibin])/(m_integral[ibin+1] - m_integral[ibin]);
    return boost::numeric_cast<T>(x);
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::cdf(Statistic x) const
{
	auto xmin = get_xmin();
	auto xmax = get_xmax();
	if (x <= xmin) {
		return 0;
	}
	if (x >= xmax) {
		return 1;
	}

	auto bin = find_bin(x);
	auto upper = get_bin_up_edge(bin);
	auto lower = get_bin_low_edge(bin);
	get_integral();
	auto rlower = bin ? m_integral[bin - 1] : 0;
	auto rupper = bin < m_integral.size() ? m_integral[bin] : 1;
	return ((upper * rlower - lower * rupper) + x *(rupper - rlower)) / get_bin_width(bin);
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::quantile(Statistic proportion) const
{
	get_integral();
	auto it = std::lower_bound(m_integral.begin(), m_integral.end(), proportion);
	auto bin = std::distance(m_integral.begin(), it);
	
	auto rlower = bin ? m_integral[bin - 1] : 0;
	auto rupper = bin < m_integral.size() ? m_integral[bin] : 1;
	auto xlower = get_bin_low_edge(bin);
	auto xupper = get_bin_up_edge(bin);
	return ((rupper*xlower - rlower*xupper) + proportion*get_bin_width(bin)) / (rupper - rlower);
}

template <typename T, typename Statistic, typename AxisType>
inline T histogram_1d<T, Statistic, AxisType>::interpolate(AxisType x)
{
    // Given a point x, approximates the value via linear interpolation
    // based on the two nearest bin centers
    // Andy Mastbaum 10/21/08

    std::size_t xbin = find_bin(x);
    axis_type x0,x1;
    value_type y0,y1;

    if(x<=get_bin_center(1)) 
    {
        return get_bin_content(1);
    }
    else if(x>=get_bin_center(get_number_bins())) 
    {
        return get_bin_content(get_number_bins());
    }
    else 
    {
        if(x<=get_bin_center(xbin)) 
        {
            y0 = get_bin_content(xbin-1);
            x0 = get_bin_center(xbin-1);
            y1 = get_bin_content(xbin);
            x1 = get_bin_center(xbin);
        }
        else 
        {
            y0 = get_bin_content(xbin);
            x0 = get_bin_center(xbin);
            y1 = get_bin_content(xbin+1);
            x1 = get_bin_center(xbin+1);
        }
        return boost::numeric_cast<T>(y0 + (x-x0)*((y1-y0)/(x1-x0)));
    }
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::scale(Statistic c1, bool width)
{
    //   -*-*-*Multiply this histogram by a constant c1*-*-*-*-*-*-*-*-*
    //         ========================================
    //
    //   this = c1*this
    //
    // Note that both contents and errors(if any) are scaled.
    // This function uses the services of histogram_1d<T, Statistic, AxisType>::add
    //
    // IMPORTANT NOTE: If you intend to use the errors of this histogram later
    // you should call sumw2 before making this operation.
    // This is particularly important if you fit the histogram after histogram_1d<T, Statistic, AxisType>::Scale
    //
    // One can scale an histogram such that the bins integral is equal to
    // the normalization parameter via histogram_1d<T, Statistic, AxisType>::Scale(T norm), where norm
    // is the desired normalization divided by the integral of the histogram.
    //
    // If option contains "width" the bin contents and errors are divided
    // by the bin width.
    statistic_type ent = m_counts;
    if (width)
        add(this,this,c1,-1);
    else                       
        add(this,this,c1,0);
    m_counts = ent;    
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::add(const histogram_1d* h1, const histogram_1d* h2, Statistic c1, Statistic c2 )
{
    //   -*-*-*Replace contents of this histogram by the addition of h1 and h2*-*-*
    //         ===============================================================
    //
    //   this = c1*h1 + c2*h2
    //   if errors are defined (see TH1::sumw2), errors are also recalculated
    //   Note that if h1 or h2 have sumw2 set, sumw2 is automatically called for this
    //   if not already set.
    //
    // IMPORTANT NOTE: If you intend to use the errors of this histogram later
    // you should call sumw2 before making this operation.
    // This is particularly important if you fit the histogram after TH1::add
    if (!h1 || !h2)
        return;
        
    bool normWidth = false;
    if (h1 == h2 && c2 < 0)
    {
        c2 = 0;
        normWidth = true;
    }

    std::size_t nbinsx = get_number_bins();
        
    //    Create sumw2 if h1 or h2 have sumw2 set
    if (!m_sumw2.empty() && (!h1->m_sumw2.empty() || !h2->m_sumw2.empty())) 
        sumw2();

    //   - add statistics
    statistic_type nEntries = std::abs( c1*h1->get_counts() + c2*h2->get_counts() );

    std::vector<statistic_type> s1(4, statistic_type()), s2(4, statistic_type()), s3(4, statistic_type());
    int i;
    for (i=0;i<4;++i) {s1[i] = s2[i] = s3[i] = 0;}
    h1->get_stats(s1);
    h2->get_stats(s2);
    for (i=0;i<4;++i)
    {
        if (i == 1)
            s3[i] = c1*c1*s1[i] + c2*c2*s2[i];
        else
            s3[i] = std::abs(c1)*s1[i] + std::abs(c2)*s2[i];
    }

    //   - Loop on bins (including underflows/overflows)
    std::size_t bin;
    statistic_type cu;
    for (bin=0;bin <= nbinsx+1; ++bin) 
    {
        axis_type wx = h1->m_axis.get_bin_width(bin);
        
        // case of histogram Addition 
        if (normWidth) 
        {
            axis_type w = wx;
            cu  = c1*h1->get_bin_content(bin)/w;
            set_bin_content(bin, boost::numeric_cast<T>(cu));
            if(!m_sumw2.empty()) 
            {
                statistic_type e1 = h1->get_bin_error(bin)/w;
                m_sumw2[bin] = c1*c1*e1*e1;
            }
        } 
        else 
        {
            cu  = c1*h1->get_bin_content(bin)+ c2*h2->get_bin_content(bin);
            set_bin_content(bin, boost::numeric_cast<T>(cu));
            if (!m_sumw2.empty()) 
            {
                statistic_type e1 = h1->get_bin_error(bin);
                statistic_type e2 = h2->get_bin_error(bin);
                m_sumw2[bin] = c1*c1*e1*e1 + c2*c2*e2*e2;
            }
        }
    }

    put_stats(s3);
    set_counts(nEntries);
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_mean() const
{
    //   -*-*-*-*-*-*Return mean value of this histogram along the X axis*-*-*-*-*
    std::vector<statistic_type> stats(4,statistic_type());
    get_stats(stats);
    if (stats[0] == statistic_type()) 
        return statistic_type();
    
    return stats[2]/stats[0];

    if( m_totalSumWeights == 0 )
        return statistic_type();

    return m_totalSumWeightsSquaresX / m_totalSumWeights;
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_mean_error() const
{
    //   -*-*-*-*-*-*Return standard error of mean of this histogram along the X axis*-*-*-*-*
    //               ====================================================
    //  Note that the mean value/RMS is computed using the bins in the currently
    //  defined range (see axis<T>::SetRange). By default the range includes
    //  all bins from 1 to nbins included, excluding underflows and overflows.
    //  To force the underflows and overflows in the computation, one must
    //  call the static function histogram_1d<T, Statistic, AxisType>::StatOverflows(true) before filling
    //  the histogram.
    //  Also note, that although the definition of standard error doesn't include the
    //  assumption of normality, many uses of this feature implicitly assume it.

    // mean error = sigma / sqrt( Neff )
    statistic_type sigma = get_sigma();
    statistic_type neff = get_effective_counts();
    return ( neff > statistic_type() ? sigma/std::sqrt(neff) : statistic_type() );
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_variance() const
{
    std::vector<statistic_type> stats(4,statistic_type());
    get_stats(stats);
    if (stats[0] == statistic_type())
        return statistic_type();
    statistic_type x = stats[2]/stats[0];
    statistic_type variance = std::abs(stats[3]/stats[0] -x*x);
    return variance;
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_sigma() const
{
    statistic_type variance = get_variance();
    return std::sqrt(variance);
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_sigma_error() const
{
    //  Return error of RMS estimation for Normal distribution
    //
    //  Note that the mean value/RMS is computed using the bins in the currently
    //  defined range (see axis<T>::SetRange). By default the range includes
    //  all bins from 1 to nbins included, excluding underflows and overflows.
    //  To force the underflows and overflows in the computation, one must
    //  call the static function histogram_1d<T, Statistic, AxisType>::StatOverflows(true) before filling
    //  the histogram.
    //  Value returned is standard deviation of sample standard deviation.
    //  Note that it is an approximated value which is valid only in the case that the
    //  original data distribution is Normal. The correct one would require
    //  the 4-th momentum value, which cannot be accurately estimated from an histogram since
    //  the x-information for all entries is not kept.

    // The right formula for RMS error depends on 4th momentum (see Kendall-Stuart Vol 1 pag 243)
    // formula valid for only gaussian distribution ( 4-th momentum =  3 * sigma^4 )
    statistic_type variance = get_variance();
    statistic_type neff = get_effective_counts();
    return ( neff > statistic_type() ? std::sqrt(variance/(2*neff)) : statistic_type() );
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_effective_counts() const
{
    // number of effective entries of the histogram,
    // i.e. the number of unweighted entries a histogram would need to
    // have the same statistical power as this histogram with possibly
    // weighted entries (i.e. <= TH1::get_counts())
    std::vector<statistic_type> s(4,statistic_type());
    get_stats(s);// s[1] sum of squares of weights, s[0] sum of weights
    return (s[1] ? s[0]*s[0]/s[1] : std::abs(s[0]) );
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_sum_of_weights() const
{
    //   -*-*-*-*-*-*Return the sum of weights excluding under/overflows*-*-*-*-*
    //               ===================================================
    return std::accumulate( ++m_bins.begin(), --m_bins.end(), statistic_type() );
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::integral(bool width) const
{
    //Return integral of bin contents. Only bins in the bins range are considered.
    // By default the integral is computed as the sum of bin contents in the range.
    // if option "width" is specified, the integral is the sum of
    // the bin contents multiplied by the bin width in x.

    return integral(m_axis.get_first_bin(), m_axis.get_last_bin(), width);
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::integral(std::size_t binx1, std::size_t binx2, bool width) const
{
    //Return integral of bin contents in range [binx1,binx2]
    // By default the integral is computed as the sum of bin contents in the range.
    // if option "width" is specified, the integral is the sum of
    // the bin contents multiplied by the bin width in x.
    Statistic err = 0;
    return do_integral(binx1, binx2, err, width);
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::integral_and_error(std::size_t binx1, std::size_t binx2, Statistic& error, bool width) const
{
    //Return integral of bin contents in range [binx1,binx2] and its error
    // By default the integral is computed as the sum of bin contents in the range.
    // if option "width" is specified, the integral is the sum of
    // the bin contents multiplied by the bin width in x.
    // the error is computed using error propagation from the bin errors assuming that
    // all the bins are uncorrelated
    return do_integral(binx1,binx2,error,width,true);
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::do_integral(std::size_t binx1, std::size_t binx2, Statistic& error, bool width, bool doError) const
{
    // internal function compute integral and optionally the error  between the limits
    // specified by the bin number values working for all histograms (1D, 2D and 3D)

    std::size_t nbinsx = get_number_bins();
    if (binx2 > nbinsx+1 || binx2 < binx1)
        binx2 = nbinsx+1;
    
    //   - Loop on bins in specified range
    statistic_type dx = 1;
    statistic_type integral = 0;
    statistic_type igerr2 = 0;
    for (std::size_t binx = binx1; binx <= binx2; ++binx) 
    {
        if (width) 
        {
            dx = m_axis.get_bin_width(binx);
            integral += get_bin_content(binx)*dx;
        }
        else       
            integral += get_bin_content(binx);
        
        if (doError) 
        {
            if (width)  
                igerr2 += get_bin_error(binx)*get_bin_error(binx)*dx*dx;
            else   
                igerr2 += get_bin_error(binx)*get_bin_error(binx);
        }
    }

    if (doError) 
        error = std::sqrt(igerr2);
    return integral;
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::set_bins(std::size_t nx, AxisType xmin, AxisType xmax)
{
	//   -*-*-*-*-*-*-*Redefine  x axis parameters*-*-*-*-*-*-*-*-*-*-*-*
	//                 ===========================
	// The X axis parameters are modified.
	// The bins content array is resized
	m_axis.set(nx,xmin,xmax);
	std::vector<T>(nx+2,T()).swap(m_bins);
	m_sumw2.clear();
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::set_bins(std::size_t nx, const AxisType* xBins)
{
	//   -*-*-*-*-*-*-*Redefine  x axis parameters with variable bin sizes *-*-*-*-*-*-*-*-*-*
	//                 ===================================================
	// The X axis parameters are modified.
	// The bins content array is resized
	m_axis.set(nx,xBins);
	std::vector<T>(nx+2,T()).swap(m_bins);
	m_sumw2.clear();
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::set_content(const T *content)
{
    //   -*-*-*-*-*-*Replace bin contents by the contents of array content*-*-*-*
    //               =====================================================
    T bincontent;
    for(std::size_t bin=0; bin< m_bins.size(); ++bin)
    {
        bincontent = *(content + bin);
        set_bin_content(bin, bincontent);
    }
}

template <typename T, typename Statistic, typename AxisType>
inline T histogram_1d<T, Statistic, AxisType>::get_maximum(T maxval) const
{
    //  Return maximum value smaller than maxval of bins in the range,
    //  unless the value has been overridden by histogram_1d<T, Statistic, AxisType>::SetMaximum,
    //  in which case it returns that value. (This happens, for example,
    //  when the histogram is drawn and the y or z axis limits are changed
    //
    //  To get the maximum value of bins in the histogram regardless of
    //  whether the value has been overridden, use
    //      h->get_bin_content(h->get_maximum_bin())
    T maximum = -(std::numeric_limits<T>::max)();
    for(std::size_t i = m_axis.get_first_bin(); i <= m_axis.get_last_bin(); ++i )
    {
        T value = m_bins[i];
        if (value > maximum && value < maxval) 
            maximum = value;
    }

    return maximum;
}

template <typename T, typename Statistic, typename AxisType>
inline std::size_t histogram_1d<T, Statistic, AxisType>::get_maximum_bin() const
{
    //   -*-*-*-*-*Return location of bin with maximum value in the range*-*
    //             ======================================================
    std::size_t locmax;
    return get_maximum_bin(locmax);
}

template <typename T, typename Statistic, typename AxisType>
inline std::size_t histogram_1d<T, Statistic, AxisType>::get_maximum_bin(std::size_t &locmax) const
{
    //   -*-*-*-*-*Return location of bin with maximum value in the range*-*
    //             ======================================================
    T maximum = -(std::numeric_limits<T>::max)();
    std::size_t locm = 0;    
    for(std::size_t i = m_axis.get_first_bin(); i <= m_axis.get_last_bin(); ++i )
    {
        T value = m_bins[i];
        if (value > maximum) 
        {
            maximum = value;
            locm    = i;
        }
    }
    return locm;
}

template <typename T, typename Statistic, typename AxisType>
inline T histogram_1d<T, Statistic, AxisType>::get_minimum(T minval) const
{
    //  Return minimum value smaller than maxval of bins in the range,
    //  unless the value has been overridden by histogram_1d<T, Statistic, AxisType>::SetMinimum,
    //  in which case it returns that value. (This happens, for example,
    //  when the histogram is drawn and the y or z axis limits are changed
    //
    //  To get the minimum value of bins in the histogram regardless of
    //  whether the value has been overridden, use
    //     h->get_bin_content(h->get_minimum_bin())
    T minimum=(std::numeric_limits<T>::max)();
    for(std::size_t i = m_axis.get_first_bin(); i <= m_axis.get_last_bin(); ++i )
    {
        T value = m_bins[i];
        if (value < minimum && value > minval) 
            minimum = value;
    }
    return minimum;
}

template <typename T, typename Statistic, typename AxisType>
inline std::size_t histogram_1d<T, Statistic, AxisType>::get_minimum_bin() const
{
    //   -*-*-*-*-*Return location of bin with minimum value in the range*-*
    //             ======================================================
    std::size_t locmix;
    return get_minimum_bin(locmix);
}

template <typename T, typename Statistic, typename AxisType>
inline std::size_t histogram_1d<T, Statistic, AxisType>::get_minimum_bin(std::size_t &locmix) const
{
    //   -*-*-*-*-*Return location of bin with minimum value in the range*-*
    //             ======================================================
    T minimum = (std::numeric_limits<T>::max)();
    std::size_t locm = 0;    
    for(std::size_t i = m_axis.get_first_bin(); i <= m_axis.get_last_bin(); ++i )
    {
        T value = m_bins[i];
        if (value < minimum)
        {
            minimum = value;
            locm    = i;                    
        }
    }

    return locm;
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::set_error(const Statistic *error)
{
    //   -*-*-*-*-*-*-*Replace bin errors by values in array error*-*-*-*-*-*-*-*-*
    statistic_type binerror;
    for(std::size_t bin=0; bin< m_bins.size(); ++bin) 
    {
        binerror = error[bin];
        set_bin_error(bin, binerror);
    }
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::sumw2()
{
    // Create structure to store sum of squares of weights*-*-*-*-*-*-*-*
    //
    //     if histogram is already filled, the sum of squares of weights
    //     is filled with the existing bin contents
    //
    //     The error per bin will be computed as sqrt(sum of squares of weight)
    //     for each bin.
    //
    //  This function is automatically called when the histogram is created
    //  if the static function histogram_1d<T, Statistic, AxisType>::SetDefaultSumw2 has been called before.

    if (m_sumw2.size() == m_bins.size()) 
        return;
    
    m_sumw2.resize(m_bins.size());

    if ( m_counts > 0 )
        for (std::size_t bin=0; bin<m_bins.size(); ++bin)
            m_sumw2[bin] = std::abs(get_bin_content(bin));        
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_bin_error(std::size_t bin) const
{
    //   -*-*-*-*-*Return value of error associated to bin number bin*-*-*-*-*
    //             ==================================================
    //
    //    if the sum of squares of weights has been defined (via sumw2),
    //    this function returns the sqrt(sum of w2).
    //    otherwise it returns the sqrt(contents) for this bin.
    //
    //   -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
	if( m_bins.empty() )
		return statistic_type();

    if (bin >= m_bins.size())
		bin = m_bins.size()-1;
    if(!m_sumw2.empty()) 
    {
        statistic_type err2 = m_sumw2[bin];
        return std::sqrt(err2);
    }
    
    statistic_type error2 = std::abs(get_bin_content(bin));
    return std::sqrt(error2);
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::set_bin_error(std::size_t bin, Statistic error)
{
    // see convention for numbering bins at top.
    if (m_sumw2.empty())
        sumw2();
    if (bin>= m_sumw2.size())
        return;
    m_sumw2[bin] = error*error;
}

template <typename T, typename Statistic, typename AxisType>
inline T histogram_1d<T, Statistic, AxisType>::get_bin_content(std::size_t bin) const
{
	if (m_bins.empty()) 
		return 0;

    // see convention for numbering bins at top.
    if (bin >= m_bins.size())
		bin = m_bins.size()-1;

    return T (m_bins[bin]);
}

template <typename T, typename Statistic, typename AxisType>
inline void histogram_1d<T, Statistic, AxisType>::set_bin_content(std::size_t bin, T content)
{
    // set bin content
    // see convention for numbering bins at top.
    // In case the bin number is greater than the number of bins and
    // the time display option is set or the kCanRebin bit is set,
    // the number of bins is automatically doubled to accommodate the new bin

    ++m_counts;
    m_totalSumWeights = 0;
    m_bins[bin] = content;
}

template <typename T, typename Statistic, typename AxisType>
inline Statistic histogram_1d<T, Statistic, AxisType>::get_integral() const
{
	if (!m_integral.empty())
	{
		std::size_t nbinsx = get_number_bins();
		if (m_integral[nbinsx + 1] != m_counts)
			return compute_integral();
		else
			return m_integral[nbinsx];
	}
	else
		return compute_integral();
}

namespace {
	inline double probks(double alam)
	{
		const double EPS1 = 0.001;
		const double EPS2 = 1.0e-8;
		int j;
		double a2, fac = 2.0, sum = 0.0, term, termbf = 0.0;
		a2 = -2.0*alam*alam;
		for (j = 1; j <= 100; ++j)
		{
			term = fac * std::exp(a2*j*j);
			sum += term;
			if (std::abs(term) <= EPS1 * termbf || std::abs(term) <= EPS2 * sum) 
				return sum;
			fac = -fac;//! Alternating signs in sum.
			termbf = std::abs(term);
		}
		return 1.0;//! Get here only by failing to converge.
	}
}

/*
//! From numerical recipes.
template <typename T, typename Statistic, typename AxisType>
inline std::pair<double, double> histogram_1d<T, Statistic, AxisType>::ks_test(histogram_1d<T, Statistic, AxisType> const& o) const
{
	unsigned long j1 = 1, j2 = 1;
	double d1, d2, dt, en1, en2, en, fn1 = 0.0, fn2 = 0.0;

	std::vector<double> data1(get_number_bins(), 0.0);
	for (int i = 1; i <= get_number_bins(); ++i)
		data1[i - 1] = get_bin_content(i);
	std::vector<double> data2(o.get_number_bins(), 0.0);
	for (int i = 1; i <= o.get_number_bins(); ++i)
		data2[i - 1] = o.get_bin_content(i);

	std::sort(data1.begin(), data1.end());
	std::sort(data2.begin(), data2.end());
	en1 = data1.size();
	en2 = data2.size();
	auto d = 0.0;
	while (j1 <= data1.size() && j2 <= data2.size())
	{
		if ((d1 = data1[j1-1]) <= (d2 = data2[j2-1])) fn1 = j1++ / en1; //! Next step is in data1.
		if (d2 <= d1) fn2 = j2++ / en2;//! Next step is in data2.
		if ((dt = std::abs(fn2 - fn1)) > d) d = dt;
	}
	en = std::sqrt(en1*en2 / (en1 + en2));
	auto prob = probks((en + 0.12 + 0.11 / en)*(d));//! Compute significance.

	return { d, prob };
}
*/

//! From numerical recipes.
template <typename T, typename Statistic, typename AxisType>
inline std::pair<double, double> histogram_1d<T, Statistic, AxisType>::ks_test(histogram_1d<T, Statistic, AxisType> const& o) const
{
	double prob = 0;
	auto ncx1 = get_number_bins();
	auto ncx2 = o.get_number_bins();

	if(ncx1 != ncx2)
	{
		GEOMETRIX_ASSERT(false);
		return { std::numeric_limits<double>::infinity(), 0.0 };
	}

	// Check consistency in channel edges
	double difprec = 1e-5;
	double diff1 = std::abs(get_xmin() - o.get_xmin());
	double diff2 = std::abs(get_xmax() - o.get_xmax());
	if (diff1 > difprec || diff2 > difprec) 
	{
		GEOMETRIX_ASSERT(false);
		return { std::numeric_limits<double>::infinity(), 0.0 };
	}

	auto afunc1 = false;
	auto afunc2 = false;
	double sum1 = 0, sum2 = 0;
	double ew1, ew2, w1 = 0, w2 = 0;
	int bin;
	int ifirst = 1;
	int ilast = ncx1;
	for (bin = ifirst; bin <= ilast; bin++) 
	{
		sum1 += o.get_bin_content(bin);
		sum2 += o.get_bin_content(bin);
		ew1 = get_bin_error(bin);
		ew2 = o.get_bin_error(bin);
		w1 += ew1 * ew1;
		w2 += ew2 * ew2;
	}

	if (sum1 == 0 || sum2 == 0)
	{
		GEOMETRIX_ASSERT(false);
		return { std::numeric_limits<double>::infinity(), 0.0 };
	}

	// calculate the effective entries.
	// the case when errors are zero (w1 == 0 or w2 ==0) are equivalent to
	// compare to a function. In that case the rescaling is done only on sqrt(esum2) or sqrt(esum1)
	double esum1 = 0, esum2 = 0;
	if (w1 > 0)
		esum1 = sum1 * sum1 / w1;
	else
		afunc1 = true;    // use later for calculating z

	if (w2 > 0)
		esum2 = sum2 * sum2 / w2;
	else
		afunc2 = true;    // use later for calculating z

	if (afunc2 && afunc1) 
	{
		GEOMETRIX_ASSERT(false);
		return { std::numeric_limits<double>::infinity(), 0.0 };
	}

	double s1 = 1 / sum1;
	double s2 = 1 / sum2;

	// Find largest difference for Kolmogorov Test
	double dfmax = 0, rsum1 = 0, rsum2 = 0;

	for (bin = ifirst; bin <= ilast; ++bin) 
	{
		rsum1 += s1 * get_bin_content(bin);
		rsum2 += s2 * o.get_bin_content(bin);
		dfmax = (std::max)(dfmax, std::abs(rsum1 - rsum2));
	}

	// Get Kolmogorov probability
	double z, prb1 = 0, prb2 = 0, prb3 = 0;

	// case this is exact (has zero errors)
	if (afunc1)
		z = dfmax * std::sqrt(esum2);
	// case o has zero errors
	else if (afunc2)
		z = dfmax * std::sqrt(esum1);
	else
		// for comparison between two data sets
		z = dfmax * std::sqrt(esum1*esum2 / (esum1 + esum2));

	prob = probks(z);

	// This numerical error condition should never occur
	GEOMETRIX_ASSERT(std::abs(rsum1 - 1.0) <= 0.002);
	GEOMETRIX_ASSERT(std::abs(rsum2 - 1.0) <= 0.002);

	return { dfmax, prob };
}

}//! namespace stk;

