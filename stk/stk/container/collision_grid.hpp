#pragma once

#include <junction/QSBR.h>
#include <stk/container/concurrent_hash_grid.hpp>
#include <stk/container/detail/grid_traverser.hpp>
#include <geometrix/algorithm/fast_voxel_grid_traversal.hpp>
#include <geometrix/algorithm/point_in_polygon.hpp>
#include <geometrix/arithmetic/vector/perp.hpp>
#include <stk/geometry/primitive/polygon.hpp>
#include <stk/geometry/primitive/polyline.hpp>
#include <stk/geometry/primitive/polygon_with_holes.hpp>
#include <stk/geometry/primitive/segment.hpp>
#include <geometrix/tags.hpp>

#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>

#include <cstdint>
#include <type_traits>

namespace stk {

    namespace detail {
        template <typename Cell>
        struct cell_allocator 
        {
            using cell_type = Cell;

            cell_type* construct()
            {
                return new cell_type{};
            }

            void destroy( cell_type* c )
            {
                delete c;
            }

            cell_type* operator()()
            {
                return construct();
            }

            void operator()( cell_type* c )
            {
                destroy( c );
            }
        };

        template <typename Grid>
		struct find_cell_accessor
		{
			find_cell_accessor( Grid& grid )
				: grid( &grid )
			{}

			template <typename Visitor>
			auto operator()( std::uint32_t i, std::uint32_t j, Visitor&& v ) const
			{
				if( auto* pCell = grid->find_cell( i, j ); pCell )
					v( *pCell );
			}

			template <typename Point, typename Visitor>
			auto operator()( Point const& p, Visitor&& v ) const
			{
				if( auto* pCell = grid->find_cell( p ); pCell )
					v( *pCell );
			}

			Grid* grid;
		};

        template <typename Grid>
		struct modify_cell_accessor
		{
			modify_cell_accessor( Grid& grid )
				: grid( &grid )
			{}

			template <typename Visitor>
			auto operator()( std::uint32_t i, std::uint32_t j, Visitor&& v ) const
			{
				auto& cell = grid->get_cell( i, j );
				v( cell );
			}

			template <typename Point, typename Visitor>
			auto operator()( Point const& p, Visitor&& v ) const
			{
				auto& cell = grid->get_cell( p );
				v( cell );
			}

			Grid* grid;
		};
    }//! namespace detail;

    template <typename Cell, typename CellAllocator = detail::cell_allocator<Cell> >
    class collision_grid 
    {
    public:

        using cell_type = Cell;
        using cell_alloc = CellAllocator;

        template <typename GridTraits>
        collision_grid( GridTraits&& traits, CellAllocator&& cellAlloc = CellAllocator{} )
	    : m_qsbr{}
        , m_grid( traits, cellAlloc, junction::QSBRMemoryReclamationPolicy( &m_qsbr ) )
        {}

        virtual ~collision_grid()
        {
            m_grid.clear();
            m_qsbr.flush();
        }

        void quiesce() 
        {
            m_qsbr.flush(); 
        }

        template <typename Geometry, typename Visitor>
        void visit( Geometry&& geometry, Visitor&& visitor ) const
        {
			using namespace geometrix;
			visit_impl( std::forward<Geometry>( geometry ), std::forward<Visitor>( visitor ), detail::find_cell_accessor{ m_grid }, typename geometrix::geometry_tag_of<Geometry>::type() );
		}
        
        template <typename Geometry, typename Visitor>
        void visit( Geometry&& geometry, const stk::units::length& radius, Visitor&& visitor ) const
        {
			using namespace geometrix;
			visit_impl(std::forward<Geometry>(geometry), radius, std::forward<Visitor>(visitor), detail::find_cell_accessor{ m_grid }, typename geometrix::geometry_tag_of<Geometry>::type());
		}
        
        template <typename Geometry, typename Visitor>
        void modify( Geometry&& geometry, Visitor&& visitor ) const
        {
			using namespace geometrix;
			visit_impl(std::forward<Geometry>(geometry), std::forward<Visitor>(visitor), detail::modify_cell_accessor{ m_grid }, typename geometrix::geometry_tag_of<Geometry>::type());
		}
        
        template <typename Geometry, typename Visitor>
        void modify( Geometry&& geometry, const stk::units::length& radius, Visitor&& visitor ) const
        {
			using namespace geometrix;
			visit_impl(std::forward<Geometry>(geometry), radius, std::forward<Visitor>(visitor), detail::modify_cell_accessor{ m_grid }, typename geometrix::geometry_tag_of<Geometry>::type());
		}

    protected:
		
        template <typename Point, typename Visitor, typename CellAccessor>
		void visit_impl(Point&& p, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::point_tag) const
		{
			using namespace geometrix;
			accessor( std::forward<Point>(p), std::forward<Visitor>( visitor ) );
		}

		template <typename Segment, typename Visitor, typename CellAccessor>
		void visit_impl(Segment&& s, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::segment_tag) const
		{
			using namespace geometrix;
	        fast_voxel_grid_traversal( m_grid.get_traits(), s, [&,this]( std::uint32_t i, std::uint32_t j )
                { accessor( i, j, std::forward<Visitor>( visitor ) ); }, make_tolerance_policy() );
		}

		template <typename Polyline, typename Visitor, typename CellAccessor>
		void visit_impl(Polyline&& p, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::polyline_tag) const
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polyline>::type>::point_type;

            for(auto i = 0ULL, j= 1ULL; j < p.size(); i = j++)
				visit_impl( segment<point_t>{ p[i], p[j] }, std::forward<Visitor>( visitor ), std::forward<CellAccessor>(accessor), geometrix::geometry_tags::segment_tag{} );
		}

		template <typename Polygon, typename Visitor, typename CellAccessor>
		void visit_impl(Polygon&& p, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::polygon_tag) const
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polygon>::type>::point_type;

            for(std::size_t j = std::size_t{}, i = p.size() - 1; j < p.size(); i = j++)
				visit_impl( segment<point_t>{ p[i], p[j] }, std::forward<Visitor>(visitor), std::forward<CellAccessor>(accessor), geometrix::geometry_tags::segment_tag{} );
		}
		
		template <typename Polygon, typename Visitor, typename CellAccessor>
		void visit_impl(Polygon&& p, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::polygon_with_holes_tag) const
		{
			using namespace geometrix;
			using point_t = typename geometric_traits<Polygon>::point_type;

			visit_impl(p.get_outer(), std::forward<Visitor>(visitor), std::forward<CellAccessor>(accessor), geometrix::geometry_tags::polygon_tag());
			auto& holes = p.get_holes();
            for(auto& h : holes)
                visit_impl(h, std::forward<Visitor>(visitor), std::forward<CellAccessor>(accessor), geometrix::geometry_tags::polygon_tag());
		}
        
        template <typename Point, typename Visitor, typename CellAccessor>
		void visit_impl(Point&& p, const stk::units::length& radius, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::point_tag) const
		{
			using namespace geometrix;

            visit_cells<1>( m_grid, p, [&, this]( int i, int j ) -> void 
            {
                if( i >= 0 && j >= 0 && static_cast<unsigned int>(i) < m_grid.get_traits().get_width() && static_cast<unsigned int>(j) < m_grid.get_traits().get_height() ){
					accessor( i, j, std::forward<Visitor>(visitor) );                }
            } );
		}

		template <typename Segment, typename Visitor, typename CellAccessor>
		void visit_impl(Segment&& s, const stk::units::length& radius, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::segment_tag) const
		{
			using namespace geometrix;
			fast_voxel_grid_traversal( m_grid.get_traits(), s, [&, this]( std::uint32_t i, std::uint32_t j )
				{ accessor( i, j, std::forward<Visitor>( visitor ) ); }, make_tolerance_policy() );
            auto v = normalize(s.get_end()-s.get_start());

            segment2 left{ s.get_start() + radius * left_normal(v), s.get_end() + radius * left_normal(v) };
			fast_voxel_grid_traversal( m_grid.get_traits(), left, [&, this]( std::uint32_t i, std::uint32_t j )
				{ accessor( i, j, std::forward<Visitor>( visitor ) ); }, make_tolerance_policy() );

            segment2 right{ s.get_start() + radius * right_normal(v), s.get_end() + radius * right_normal(v) };
			fast_voxel_grid_traversal( m_grid.get_traits(), right, [&, this]( std::uint32_t i, std::uint32_t j )
				{ accessor( i, j, std::forward<Visitor>( visitor ) ); }, make_tolerance_policy() );
		}

		template <typename Polyline, typename Visitor, typename CellAccessor>
		void visit_impl(Polyline&& p, const stk::units::length& radius, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::polyline_tag) const
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polyline>::type>::point_type;

            for(auto i = 0ULL, j= 1ULL; j < p.size(); i = j++)
				visit_impl( segment<point_t>{ p[i], p[j] }, radius, std::forward<Visitor>(visitor), std::forward<CellAccessor>(accessor), geometrix::geometry_tags::segment_tag{} );
		}

		template <typename Polygon, typename Visitor, typename CellAccessor>
		void visit_impl(Polygon&& p, const stk::units::length& radius, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::polygon_tag) const
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polygon>::type>::point_type;

            for(std::size_t j = std::size_t{}, i = p.size() - 1; j < p.size(); i = j++)
				visit_impl( segment<point_t>{ p[i], p[j] }, radius, std::forward<Visitor>(visitor), std::forward<CellAccessor>( accessor ), geometrix::geometry_tags::segment_tag{} );
		}
		
		template <typename Polygon, typename Visitor, typename CellAccessor>
		void visit_impl(Polygon&& p, const stk::units::length& radius, Visitor&& visitor, CellAccessor&& accessor, geometrix::geometry_tags::polygon_with_holes_tag) const
		{
			using namespace geometrix;
			using point_t = typename geometric_traits<Polygon>::point_type;

			visit_impl(p.get_outer(), radius, std::forward<Visitor>(visitor), std::forward<CellAccessor>(accessor), geometrix::geometry_tags::polygon_tag() );
			auto& holes = p.get_holes();
            for(auto& h : holes)
                visit_impl(h, radius, std::forward<Visitor>(visitor), std::forward<CellAccessor>(accessor), geometrix::geometry_tags::polygon_tag{} );
		}

        using grid_type = stk::concurrent_hash_grid_2d<cell_type, geometrix::grid_traits<stk::units::length>, cell_alloc>;

        mutable junction::QSBR m_qsbr;
        mutable grid_type      m_grid;
    };

}//namespace stk;

