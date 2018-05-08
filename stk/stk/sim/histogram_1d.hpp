//
//! Copyright © 2015
//! Brandon Kohn - Based on TH1F from the ROOT libraries at root.cern.ch.
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
/////////////////////////////////////////////////////////////////////////////
#pragma once

#include <boost/optional.hpp>
#include <numeric>

#if defined(STK_USE_BOOST_SERIALIZATION)
#include <stk/utility/serialization.hpp>
#endif

namespace stk {
	
template <typename T>
class axis
{
    template <typename T2>
    friend class axis;

public:

	typedef T value_type;
    
    axis();    
    axis(std::size_t nbins, T xmin, T xmax);
    axis(std::size_t nbins, const T *xbins);
    
    template <typename T2>
    axis( const axis<T2>& other );

    virtual ~axis();

    std::size_t find_bin(T x) const;
    T           get_bin_center(std::size_t bin) const;
    T           get_bin_center_log(std::size_t bin) const;
    T           get_bin_low_edge(std::size_t bin) const;
    T           get_bin_up_edge(std::size_t bin) const;
    T           get_bin_width(std::size_t bin) const;
    void        get_center(T *center) const;
    void        get_low_edge(T *edge) const;
    std::size_t get_number_bins() const { return m_numBins; }
    T           get_min() const {return m_min;}
    T           get_max() const {return m_max;}
    bool        is_variable_bin_size() const {return (!m_bins.empty());}
    void        set(std::size_t nbins, T xmin, T xmax);
    void        set(std::size_t nbins, const T *xbins);
    void        set_limits(T xmin, T xmax);    

    std::size_t get_first_bin() const { return m_first; }
    std::size_t get_last_bin() const { return m_last; }

    template <typename T2>
    axis& operator =(const axis<T2>& rhs);

private:

#if defined(STK_USE_BOOST_SERIALIZATION)
	friend class boost::serialization::access;

	template <typename Archive>
	void save(Archive& ar, unsigned int /*version*/) const;

	template <typename Archive>
	void load(Archive& ar, unsigned int version);

	BOOST_SERIALIZATION_SPLIT_MEMBER();
#endif
	
	std::size_t    m_numBins;
    T              m_min;           //low edge of first bin
    T              m_max;           //upper edge of last bin
    std::vector<T> m_bins;          //Bin edges array if dynamically sized.    

    //! Add range bins (to allow dealing with overflow properly).
    std::size_t    m_first;
    std::size_t    m_last;

};

//      Convention for numbering bins
//      =============================
//      For all histogram types: nbins, xlow, xup
//        bin = 0;       underflow bin
//        bin = 1;       first bin with low-edge xlow INCLUDED
//        bin = nbins;   last bin with upper-edge xup EXCLUDED
//        bin = nbins+1; overflow bin
template <typename T, typename Statistic = T, typename AxisType = T>
class histogram_1d
{
    template <typename T2, typename Statistic2, typename AxisType2>
    friend class histogram_1d;

public:

	typedef T value_type;
    typedef Statistic statistic_type;
    typedef AxisType axis_type;

    histogram_1d();
    histogram_1d(std::size_t nbinsx, AxisType xlow, AxisType xup);
    histogram_1d(std::size_t nbinsx, const AxisType* xbins);

	template <typename T2, typename S2, typename A2>
	histogram_1d( const histogram_1d<T2, S2, A2>& other );
    virtual ~histogram_1d();    
    
    void        add_bin_content(std::size_t bin) {++m_bins[bin];}
    void        add_bin_content(std::size_t bin, T w){m_bins[bin] += w;}
    Statistic   compute_integral() const;
    std::size_t fill(axis_type x);
    std::size_t fill(axis_type x, T w);
    std::size_t find_bin(axis_type x) const { return m_axis.find_bin(x); }
    axis_type   get_bin_center(std::size_t bin) const {return m_axis.get_bin_center(bin);}
    T           get_bin_content(std::size_t bin) const;
    Statistic   get_bin_error(std::size_t bin) const;
    axis_type   get_bin_low_edge(std::size_t bin) const {return m_axis.get_bin_low_edge(bin);}
	axis_type   get_bin_up_edge(std::size_t bin) const { return m_axis.get_bin_up_edge(bin); }
    axis_type   get_bin_width(std::size_t bin) const {return m_axis.get_bin_width(bin);}
    Statistic   get_counts() const;
    void        set_counts(statistic_type c ) { m_counts = c; }
    void        get_low_edge(axis_type *edge) const {m_axis.get_low_edge(edge);}
    T           get_maximum(T maxval=(std::numeric_limits<T>::max)()) const;
    axis_type   get_xmax() const { return m_axis.get_max(); }
    std::size_t get_maximum_bin() const;
    std::size_t get_maximum_bin(std::size_t &locmax) const;
    T           get_minimum(T minval=-(std::numeric_limits<T>::max)()) const;
    axis_type   get_xmin() const { return m_axis.get_min(); }
    std::size_t get_minimum_bin() const;
    std::size_t get_minimum_bin(std::size_t &locmix) const;
    Statistic   get_mean() const;
    Statistic   get_mean_error() const;
    std::size_t get_number_bins() const {return m_axis.get_number_bins();}
    Statistic   get_norm_factor() const {return m_normFactor;}
    T           get_random(T r1) const;
	Statistic   cdf(Statistic x) const;
	Statistic   quantile(Statistic proportion) const;
    Statistic   get_sum_of_weights() const;
    Statistic   get_sigma() const;
    Statistic   get_sigma_error() const;
    Statistic   integral(bool width=false) const;
    Statistic   integral(std::size_t binx1, std::size_t binx2, bool width = false) const;
    Statistic   integral_and_error(std::size_t binx1, std::size_t binx2, statistic_type& err, bool width=false) const;
    T           interpolate(axis_type x);
    void        scale(statistic_type c1=1, bool width=false);
    void        set_bin_content(std::size_t bin, T content);
    void        set_bin_error(std::size_t bin, statistic_type error);
    void        set_bins(std::size_t nx, axis_type xmin, axis_type xmax);
    void        set_bins(std::size_t nx, const axis_type *xBins);
    void        set_content(const T*content);
    void        set_error(const statistic_type* error);
    void        set_norm_factor(statistic_type factor=1) {m_normFactor = factor;}
    void        sumw2();
    Statistic   get_variance() const;
    Statistic   get_effective_counts() const;

    template <typename U, typename S2, typename A2>
    bool operator == (const histogram_1d<U, S2, A2>& rhs) const;

    template <typename T2, typename S2, typename A2>
    histogram_1d& operator = (const histogram_1d<T2, S2, A2>& rhs);

    void get_stats(std::vector<Statistic>& stats) const;
    void put_stats(const std::vector<Statistic>& stats);


private:
        
    Statistic do_integral(std::size_t ix1, std::size_t ix2, Statistic& err, bool width = false, bool doerr = false) const;
	Statistic get_integral() const;
    void   add(const histogram_1d* h1, const histogram_1d* h2, Statistic c1, Statistic c2 );

#if defined(STK_USE_BOOST_SERIALIZATION)
	friend class boost::serialization::access;

	template <typename Archive>
	void save(Archive& ar, unsigned int /*version*/) const;

	template <typename Archive>
	void load(Archive& ar, unsigned int version);

	BOOST_SERIALIZATION_SPLIT_MEMBER();
#endif

    axis<axis_type>        m_axis;
    Statistic              m_counts;                  // Number of entries
    Statistic              m_totalSumWeights;         // Total Sum of weights
    Statistic              m_totalSumWeightsSquares;  // Total Sum of squares of weights
    Statistic              m_totalSumWeightsSquaresX; // Total Sum of weight*X
    Statistic              m_totalSumWeightsSquaresX2;// Total Sum of weight*X*X
    Statistic              m_normFactor;              // Normalization factor
    std::vector<Statistic> m_sumw2;                   // Array of sum of squares of weights
    std::vector<T>         m_bins;                    // weights
    mutable std::vector<Statistic> m_integral;        // integral of bins used by get_random
};

}//namespace stk;

#if defined(STK_USE_BOOST_SERIALIZATION)
STK_CLASS_VERSION_TMPL( stk::axis, 1, 1 );
STK_CLASS_VERSION_TMPL( stk::histogram_1d, 3, 1 );
#endif

#include <ostream>
template <typename T>
inline std::ostream& operator << (std::ostream& os, const stk::histogram_1d<T>& hist)
{
	os << hist.get_number_bins() << "\n";
	os << hist.get_xmin() << "\n";
	os << hist.get_xmax() << "\n";
	for (std::size_t i = 0; i < hist.get_number_bins(); ++i)
	{
		os << (i ? ", " : "") << hist.get_bin_content(i+1);
	}

	os << std::endl;

	return os;
}

