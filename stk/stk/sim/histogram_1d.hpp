//! Copyright © 2018
//! Brandon Kohn 
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
/////////////////////////////////////////////////////////////////////////////
#pragma once

#include <geometrix/utility/assert.hpp>
#include <boost/optional.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/math/distributions/chi_squared.hpp>
#include <stk/geometry/tolerance_policy.hpp>
#include <algorithm>
#include <random>
#include <ostream>

namespace stk {
	

template <typename T>
class histogram_1d
{
	class axis
	{
	public:

		axis() = default;

		axis(std::size_t nbins, T xmin, T xmax)
			: m_numBins(nbins)
			, m_min(xmin)
			, m_max(xmax)
			, m_binWidth((xmax - xmin) / nbins)
		{}

		std::size_t get_number_bins() const { return m_numBins; }

		std::size_t find_bin(T x) const
		{
			if (x >= m_min && x < m_max)
				return boost::numeric_cast<std::size_t>(m_numBins*(x - m_min) / (m_max - m_min));

			if (x == m_max)
				return m_numBins - 1;

			return invalid_bin;
		}
		
		T get_bin_width(std::size_t bin) const
		{
			return m_binWidth;
		}

		T get_bin_low_edge(std::size_t bin) const
		{
			return m_min + bin * m_binWidth;
		}

		T get_bin_center(std::size_t bin) const
		{
		   return get_bin_low_edge(bin) + 0.5 * m_binWidth;
		}

		T get_bin_hi_edge(std::size_t bin) const
		{
		   return m_min + (bin+1) * m_binWidth;
		}

		T           get_min() const {return m_min;}
		T           get_max() const {return m_max;}

		template <typename NumberComparisonPolicy>
		bool equals(axis const& rhs, const NumberComparisonPolicy& cmp) const
		{
			return m_numBins == rhs.m_numBins && cmp.equals(m_min, rhs.m_min) && cmp.equals(m_max, rhs.m_max);
		}

	private:
		
		std::size_t    m_numBins = 1;
		T              m_min = 0;
		T              m_max = 1;
		T              m_binWidth = 1;
	};
public:

	using value_type = T;
	static const std::size_t invalid_bin = (std::numeric_limits<std::size_t>::max)(); 

	histogram_1d(std::size_t nbins, T xlo, T xhi)
		: m_axis(nbins, xlo, xhi)
		, m_bins(nbins, T{})
	{
		GEOMETRIX_ASSERT(nbins != 0);
	}

	T           get_counts() const
	{
		return m_counts;
	}

	T           get_max() const
	{
		return m_axis.get_max();
	}

	T           get_min() const
	{
		return m_axis.get_min();
	}

	std::size_t get_number_bins() const
	{
		return m_axis.get_number_bins();
	}

	std::size_t fill(T x, T w = T(1))
	{
		++m_counts;
		std::size_t bin = m_axis.find_bin(x);
		GEOMETRIX_ASSERT(bin < m_bins.size());
		add_bin_weight(bin, w);
		return bin;
	}

	std::size_t find_bin(T x) const
	{
		return m_axis.find_bin(x);
	}

	T           get_bin_center(std::size_t bin) const
	{
		return m_axis.get_bin_center(bin);
	}

	T           get_bin_weight(std::size_t bin) const
	{
		GEOMETRIX_ASSERT(bin < m_bins.size());
		return m_bins[bin];
	}

	void        set_bin_weight(std::size_t bin, T weight)
	{
		GEOMETRIX_ASSERT(bin < m_bins.size());
		invalidate_cdf();  ++m_counts; m_bins[bin] = weight;
	}

	T           add_bin_weight(std::size_t bin, T w = T(1))
	{
		GEOMETRIX_ASSERT(bin < m_bins.size());
		invalidate_cdf(); return m_bins[bin] += w;
	}

	T           get_bin_error(std::size_t bin) const
	{
		using std::sqrt;
		using std::abs;
		return sqrt(abs(get_bin_weight(bin)));
	}

	T           get_bin_low_edge(std::size_t bin) const
	{
		return m_axis.get_bin_low_edge(bin);
	}

	T           get_bin_hi_edge(std::size_t bin) const
	{
		return m_axis.get_bin_hi_edge(bin);
	}

	T           get_bin_width(std::size_t bin) const
	{
		return m_axis.get_bin_width(bin);
	}

	template <typename Engine>
	T           sample(Engine& eng) const
	{
		if (!create_cdf()) 
		{
			GEOMETRIX_ASSERT(false);//! sampling from an empty histogram.
			return T{};
		}

		std::uniform_real_distribution<> U();
		auto r = U(eng);
		auto it = std::lower_bound(m_cdf.begin(), m_cdf.end(), r);
		std::size_t ibin = std::distance(m_cdf.begin(), it);
		if (it == m_cdf.end() || *it != r)
			--ibin;

		if (ibin >= get_number_bins())
			ibin = get_number_bins() - 1;

		auto x = get_bin_low_edge(ibin + 1);
		if (r > m_cdf[ibin])
			x += get_bin_width(ibin + 1)*(r - m_cdf[ibin]) / (m_cdf[ibin + 1] - m_cdf[ibin]);

		return x;
	}

	T           integral() const { return integral(0, get_number_bins()-1); }

    T           integral(std::size_t first, std::size_t last) const
	{
		GEOMETRIX_ASSERT(last < get_number_bins() && last >= first);
		return std::accumulate(m_bins.begin() + first, m_bins.begin() + last + 1, T{});
	}

	void        scale(T factor)
	{
		invalidate_cdf();
		std::for_each(m_bins.begin(), m_bins.end(), [factor](T& w)
		{
			w *= factor;
		});
	}

	//! Test to decide if this histogram and histogram o were filled from the same distribution.
	//! Returns a pair containing {chi2, p_value} where p_value is the result: 1 - Chi-Squared Probability Function gamma_p(0.5*ndf, 0.5*chi2).
	//! If p_value is less than some threshold, then the null hypothesis that the two histograms come from the same distribution is improbable.
	//! NOTE: This implementation assumes unweighted counts in the bins of both histograms. This means the weights are whole numbers.
	std::pair<T, double> chi_squared_test(histogram_1d<T> const& o, std::size_t dfThresh = 1000) const
	{
		using std::abs;
		using std::sqrt;

		//! This histograms should have the same number of bins and domain.
		GEOMETRIX_ASSERT(m_axis.equals(o.m_axis, make_tolerance_policy(1e-6)));
		if (!m_axis.equals(o.m_axis, make_tolerance_policy(1e-6)))
			return { std::numeric_limits<double>::infinity(), 0.0 };

		auto cmp = make_tolerance_policy();
		auto nbins = get_number_bins();
		auto chi2 = 0.0;
		auto sum0 = 0.0;
		auto sum1 = 0.0;

		//! First sum the weights for all the events.
		for (auto i = 0UL; i < nbins; ++i)
		{
			sum0 += get_bin_weight(i);
			sum1 += get_bin_weight(i);
		}
		
		//! Histograms should not be empty.
		GEOMETRIX_ASSERT(sum0 != 0 && sum1 != 0);
		if (sum0 == 0 || sum1 == 0)
			return { std::numeric_limits<double>::infinity(), 0.0 };

		auto numDegreeFreedom = nbins - 1;
		for (auto i = 0UL; i < nbins; ++i)
		{
			auto h0 = get_bin_weight(i);
			auto h1 = o.get_bin_weight(i);
			if (static_cast<int>(h0) != 0 || static_cast<int>(h1) != 0)
			{
				auto dh = sum1 * h0 - sum0 * h1;
				chi2 += dh * dh / (h0 + h1);
			}
			else
			{
				--numDegreeFreedom;
				continue;
			}
		}

		chi2 /= sum0 * sum1;

		auto p = 0.0;
		if (numDegreeFreedom < dfThresh)
		{
			boost::math::chi_squared_distribution<> ch2dist(numDegreeFreedom);
			auto gammap = boost::math::cdf(ch2dist, chi2);
			return { chi2, 1.0 - gammap };
		}
		
		boost::math::normal_distribution<> approxDist(numDegreeFreedom, sqrt(2.0 * numDegreeFreedom));
		auto approxGammaP= boost::math::cdf(approxDist, chi2);
		return { chi2, 1.0 - approxGammaP };
	}

private:

	void invalidate_cdf() { m_cdf.clear(); }

	void cdf_valid() { return !m_cdf.empty(); }

	bool create_cdf() const
	{
		std::size_t nbins = get_number_bins();
		m_cdf.resize(nbins + 1, T{});
		for (auto i = 1UL; i <= nbins; ++i)
			m_cdf[i] = m_cdf[i - 1] + get_bin_weight(i-1);

		if (m_cdf[nbins] == 0)
		{
			invalidate_cdf();
			return false;
		}

		auto inv = 1.0 / m_cdf[nbins];
		for (auto i = 1UL; i <= nbins; ++i)
			m_cdf[bin] *= inv;

		return true;
	}

	T get_cdf() const
	{
		if (cdf_valid())
			return m_cdf[get_number_bins()];
		else
			return compute_cdf_array();
	}

    axis           m_axis;
	T              m_counts = 0;
    std::vector<T> m_bins;

	//! Cache the discrete cdf array for sampling.
    mutable std::vector<T> m_cdf;
};

}//namespace stk;

template <typename T>
inline std::ostream& operator << (std::ostream& os, const stk::histogram_1d<T>& hist)
{
	os << hist.get_number_bins() << "\n";
	os << hist.get_min() << "\n";
	os << hist.get_max() << "\n";

	for (std::size_t i = 0; i < hist.get_number_bins(); ++i)
		os << (i ? ", " : "") << hist.get_bin_weight(i);

	os << std::endl;

	return os;
}

