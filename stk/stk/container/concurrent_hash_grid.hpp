//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <geometrix/numeric/number_comparison_policy.hpp>
#include <geometrix/primitive/point.hpp>
#include <geometrix/algorithm/grid_traits.hpp>
#include <geometrix/algorithm/grid_2d.hpp>
#include <geometrix/algorithm/hash_grid_2d.hpp>
#include <geometrix/algorithm/orientation_grid_traversal.hpp>
#include <geometrix/algorithm/fast_voxel_grid_traversal.hpp>
#include <geometrix/primitive/segment.hpp>
#include <junction/ConcurrentMap_Leapfrog.h>
#include <stk/random/xorshift1024starphi_generator.hpp>
#include <stk/geometry/geometry_kernel.hpp>
#include <stk/utility/compressed_integer_pair.hpp>
#include <stk/utility/boost_unique_ptr.hpp>
#include <junction/Core.h>
#include <turf/Util.h>
#include <chrono>

namespace stk {
    struct compressed_integer_pair_key_traits
    {
        typedef std::uint64_t Key;
        typedef turf::util::BestFit<std::uint64_t>::Unsigned Hash;
		static const Key NullKey = UINT64_MAX;// (std::numeric_limits<Key>::max)();
        static const Hash NullHash = Hash(NullKey);
        static Hash hash(Key key)
        {
            return turf::util::avalanche(Hash(key));
        }
        static Key dehash(Hash hash)
        {
            return (Key)turf::util::deavalanche(hash);
        }
    };

    template <typename T>
    struct pointer_value_traits
    {
        typedef T* Value;
        typedef typename turf::util::BestFit<T*>::Unsigned IntType;
        static const IntType NullValue = 0;
        static const IntType Redirect = 1;
    };

    namespace detail {
	template <typename T>
	struct DefaultDataAllocator
	{
		T* construct()
		{
			return new T;
		}

		void destroy(T* t)
		{
			delete t;
		}

		T* operator()()
		{
			return construct();
		}

		void operator()(T* t)
		{
			destroy(t);
		}
	};
    }//! namespace detail;

    template<typename Data, typename GridTraits, typename DataAllocator = detail::DefaultDataAllocator<Data>, typename MemoryReclamationPolicy = junction::QSBRMemoryReclamationPolicy>
    class concurrent_hash_grid_2d
    {
    public:

        typedef Data* data_ptr;
        typedef GridTraits traits_type;
        typedef stk::compressed_integer_pair key_type;
        typedef junction::ConcurrentMap_Leapfrog<std::uint64_t, Data*, stk::compressed_integer_pair_key_traits, stk::pointer_value_traits<Data>, MemoryReclamationPolicy> grid_type;

        concurrent_hash_grid_2d( const GridTraits& traits, const DataAllocator& dataAlloc = DataAllocator(), const MemoryReclamationPolicy& reclaimer = MemoryReclamationPolicy() )
            : m_gridTraits(traits)
            , m_dataAlloc(dataAlloc)
            , m_grid(reclaimer)
        {}

        ~concurrent_hash_grid_2d()
        {
            clear();
        }

        template <typename Point>
        data_ptr find_cell(const Point& point) const
        {
            BOOST_CONCEPT_ASSERT( (geometrix::Point2DConcept<Point>) );
            GEOMETRIX_ASSERT( is_contained( point ) );
            boost::uint32_t i = m_gridTraits.get_x_index(geometrix::get<0>(point));
            boost::uint32_t j = m_gridTraits.get_y_index(geometrix::get<1>(point));
            return find_cell(i,j);
        }

        data_ptr find_cell(boost::uint32_t i, boost::uint32_t j) const
        {
            stk::compressed_integer_pair p = { i, j };
            auto iter = m_grid.find(p.to_uint64());
            GEOMETRIX_ASSERT(iter.getValue() != (data_ptr)stk::pointer_value_traits<Data>::Redirect);
            return iter.getValue();
        }

        template <typename Point>
        Data& get_cell(const Point& point)
        {
            BOOST_CONCEPT_ASSERT( (geometrix::Point2DConcept<Point>) );
            GEOMETRIX_ASSERT( is_contained( point ) );
            boost::uint32_t i = m_gridTraits.get_x_index(geometrix::get<0>(point));
            boost::uint32_t j = m_gridTraits.get_y_index(geometrix::get<1>(point));
            return get_cell(i,j);
        }

        Data& get_cell(boost::uint32_t i, boost::uint32_t j)
        {
            stk::compressed_integer_pair p = { i, j };
            auto mutator = m_grid.insertOrFind(p.to_uint64());
            auto result = mutator.getValue();
            if (result == nullptr)
            {
				result = m_dataAlloc.construct();
				auto oldData = mutator.exchangeValue(result);
				if (oldData)
					m_grid.getMemoryReclaimer().reclaim_via_callable(m_dataAlloc, oldData);

				//! If a new value has been put into the key which isn't result.. get it.
				bool wasInserted = oldData != result;
				if (!wasInserted)
                {
                    m_dataAlloc.destroy(result);
					result = mutator.getValue();
                }
            }

            GEOMETRIX_ASSERT(result != nullptr);
            return *result;
        }

        const traits_type& get_traits() const { return m_gridTraits; }

        template <typename Point>
        bool is_contained( const Point& p ) const
        {
            BOOST_CONCEPT_ASSERT( (geometrix::Point2DConcept<Point>) );
            return m_gridTraits.is_contained( p );
        }

        void erase(boost::uint32_t i, boost::uint32_t j)
        {
            stk::compressed_integer_pair p = { i, j };
            auto iter = m_grid.find(p.to_uint64());
            if (iter.isValid())
            {
                auto pValue = iter.eraseValue();
				if(pValue != (data_ptr)pointer_value_traits<Data>::NullValue)
					m_grid.getMemoryReclaimer().reclaim_via_callable(m_dataAlloc, pValue);
            }
        }

        void clear()
        {
            auto it = typename grid_type::Iterator(m_grid);
            while(it.isValid())
            {
                auto pValue = m_grid.erase(it.getKey());
				if(pValue != (data_ptr)pointer_value_traits<Data>::NullValue)
					m_grid.getMemoryReclaimer().reclaim_via_callable(m_dataAlloc, pValue);
                it.next();
            };
        }

        //! Visit each key and its associated value. Their values are copies of the underlying data in the grid.
        template <typename Fn>
        void for_each(Fn&& fn) const
        {
            auto it = typename grid_type::Iterator(m_grid);
            while(it.isValid())
            {
                fn(it.getKey(), it.getValue());
            }
        }

        //! This should be called when the grid is not being modified to allow threads to reclaim memory from deletions.
        void quiesce()
        {
            m_grid.getMemoryReclaimer().quiesce();
        }

    private:

        traits_type m_gridTraits;
        mutable DataAllocator m_dataAlloc;
        mutable grid_type m_grid;

    };

}//! namespace stk;

