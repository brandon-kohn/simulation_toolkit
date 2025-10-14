#pragma once

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/utility/associative_insert.hpp>

#include <geometrix/algorithm/point_in_polygon.hpp>
#include <geometrix/algorithm/intersection/segment_triangle_intersection.hpp>
#include <geometrix/algorithm/line_intersection.hpp>
#include <geometrix/algorithm/is_segment_in_range.hpp>
#include <geometrix/algorithm/euclidean_distance.hpp>

#include <geometrix/test/test.hpp>

namespace stk
{
	namespace detail
	{
		struct segment_compare
		{
			segment_compare(const point2& start)
				: m_start(start)
				, mCmp(make_tolerance_policy())
			{}

			inline bool lexicographical_less(const segment2& lhs, const segment2& rhs) const
			{
				using namespace geometrix;

				if( lexicographically_less_than( lhs.get_start(), rhs.get_start(), mCmp ) ){
					return true;
				}

				if( numeric_sequence_equals_2d( lhs.get_start(), rhs.get_start(), mCmp ) ){
					return lexicographically_less_than( lhs.get_end(), rhs.get_end(), mCmp );
				}

				return false;
			}

			bool operator()(const segment2& lhs, const segment2& rhs) const 
			{		
				using namespace geometrix;

				auto d1 = point_point_distance_sqrd( m_start, lhs.get_start() );
				auto d2 = point_point_distance_sqrd( m_start, rhs.get_start() );
				if( d1 < d2 )
					return true;
				
				if( d1 == d2 )
				{
					d1 = point_point_distance_sqrd( m_start, lhs.get_end() );
					d2 = point_point_distance_sqrd( m_start, rhs.get_end() );
					if( d1 < d2 )
						return true;

					if( d1 == d2 && lexicographical_less( lhs, rhs ) )
						return true;
				}
				
				return false;
			}

			point2 m_start;
			tolerance_policy mCmp;
		};
	}//! namespace detail;

	template <typename Visitor>
	struct visible_segment_search
	{
		visible_segment_search( const point2& origin, const mesh2& mesh, const segment2& target, Visitor&& v )
			: m_mesh( mesh )
			, m_origin( origin )
			, m_target( target )
			, m_visitor( std::forward<Visitor>( v ) )
		{}

		template <typename Edge>
		bool visit( const Edge& item )
		{		
			return visit_triangle( m_mesh.get_triangle_vertices( item.to ), item );
		}

		template <typename Edge>
		bool operator()( const Edge& item )
		{
			return visit( item );
		}

		template <typename Edge>
		bool visit_triangle( const std::array<point2,3>& points, const Edge& item )
		{
			using namespace geometrix;
			point2 xPoints[2];
			auto cmp = make_tolerance_policy( 1e-10 );
			std::size_t numberIntersections = segment_triangle_intersect( m_target.get_start(), m_target.get_end(), points[0], points[1], points[2], xPoints, cmp );
			if( numberIntersections == 2 )
			{			
				segment2 xSegment( xPoints[0], xPoints[1] );
				if( is_segment_in_range_2d( xSegment, item.lo, item.hi, m_origin, xPoints, cmp ) )
				{
					if( point_point_distance_sqrd( m_target.get_start(), xPoints[1] ) < point_point_distance_sqrd( m_target.get_start(), xPoints[0] ) )
						std::swap( xPoints[1], xPoints[0] );
					m_visitor( segment2( xPoints[0], xPoints[1] ) );
				}
			}
			else if( numberIntersections == 1 )
			{
				if( point_in_triangle( m_target.get_start(), points[0], points[1], points[2], cmp ) )
				{
					segment2 xSegment( m_target.get_start(), xPoints[0] );
					if( is_segment_in_range_2d( xSegment, item.lo, item.hi, m_origin, xPoints, cmp ) )
					{
						if( point_point_distance_sqrd( m_target.get_start(), xPoints[1] ) < point_point_distance_sqrd( m_target.get_start(), xPoints[0] ) )
							std::swap( xPoints[1], xPoints[0] );
						m_visitor( segment2( xPoints[0], xPoints[1] ) );
					}
				}
				else if( point_in_triangle( m_target.get_end(), points[0], points[1], points[2], cmp ) )
				{
					segment2 xSegment( xPoints[0], m_target.get_end() );
					if( is_segment_in_range_2d( xSegment, item.lo, item.hi, m_origin, xPoints, cmp ) )
					{
						if( point_point_distance_sqrd( m_target.get_start(), xPoints[1] ) < point_point_distance_sqrd( m_target.get_start(), xPoints[0] ) )
							std::swap( xPoints[1], xPoints[0] );
						m_visitor( segment2( xPoints[0], xPoints[1] ) );
					}
				} 
				else
				{
					//! Degenerate triangle case.
					m_visitor( segment2( xPoints[0], xPoints[0] ) );
				}
			}
			
			return true;		
		}
		
	private:

		const mesh2& m_mesh;
		point2       m_origin;
		segment2     m_target;
		Visitor      m_visitor;

	};

	template <typename Visitor>
	auto make_visible_segment_search(const point2& origin, const mesh2& mesh, const segment2& target, Visitor&& v)
	{
		return visible_segment_search<Visitor>( origin, mesh, target, std::forward<Visitor>( v ) );
	}

	struct visible_segment_search_visitor
	{
		visible_segment_search_visitor( const point2& origin, const mesh2& mesh, const segment2& target )
			: m_mesh( mesh )
			, m_origin( origin )
			, m_target( target )
			, m_visibleSegs( detail::segment_compare( target.get_start() ) )
		{}

		template <typename Edge>
		bool visit( const Edge& item )
		{		
			return visit_triangle( m_mesh.get_triangle_vertices( item.to ), item );
		}

		template <typename Edge>
		bool operator()( const Edge& item )
		{
			return visit( item );
		}

		template <typename Edge>
		bool visit_triangle( const std::array<point2,3>& points, const Edge& item )
		{
			using namespace geometrix;
			point2 xPoints[2];
			auto cmp = make_tolerance_policy( 1e-10 );
			std::size_t numberIntersections = segment_triangle_intersect( m_target.get_start(), m_target.get_end(), points[0], points[1], points[2], xPoints, cmp );
			if( numberIntersections == 2 )
			{			
				segment2 xSegment( xPoints[0], xPoints[1] );
				if( is_segment_in_range_2d( xSegment, item.lo, item.hi, m_origin, xPoints, cmp ) )
				{
					if( point_point_distance_sqrd( m_target.get_start(), xPoints[1] ) < point_point_distance_sqrd( m_target.get_start(), xPoints[0] ) )
						std::swap( xPoints[1], xPoints[0] );
					m_visibleSegs.insert( segment2( xPoints[0], xPoints[1] ) );
				}
			}
			else if( numberIntersections == 1 )
			{
				if( point_in_triangle( m_target.get_start(), points[0], points[1], points[2], cmp ) )
				{
					segment2 xSegment( m_target.get_start(), xPoints[0] );
					if( is_segment_in_range_2d( xSegment, item.lo, item.hi, m_origin, xPoints, cmp ) )
					{
						if( point_point_distance_sqrd( m_target.get_start(), xPoints[1] ) < point_point_distance_sqrd( m_target.get_start(), xPoints[0] ) )
							std::swap( xPoints[1], xPoints[0] );
						m_visibleSegs.insert( segment2( xPoints[0], xPoints[1] ) );
					}
				}
				else 
				{
					GEOMETRIX_ASSERT( point_in_triangle( m_target.get_end(), points[0], points[1], points[2], cmp ) );
					segment2 xSegment( xPoints[0], m_target.get_end() );
					if( is_segment_in_range_2d( xSegment, item.lo, item.hi, m_origin, xPoints, cmp ) )
					{
						if( point_point_distance_sqrd( m_target.get_start(), xPoints[1] ) < point_point_distance_sqrd( m_target.get_start(), xPoints[0] ) )
							std::swap( xPoints[1], xPoints[0] );
						m_visibleSegs.insert( segment2( xPoints[0], xPoints[1] ) );
					}
				}
			}
			
			return true;		
		}

		std::vector<segment2> get_visible_segments() { return std::vector<segment2>( m_visibleSegs.begin(), m_visibleSegs.end() ); }
		
	private:

		const mesh2&                                                  m_mesh;
		point2                                                        m_origin;
		segment2                                                      m_target;
		boost::container::flat_set<segment2, detail::segment_compare> m_visibleSegs;

	};
}//! namespace stk;