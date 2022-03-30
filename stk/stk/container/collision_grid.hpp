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
        void visit( Geometry&& geometry, Visitor&& visitor )
        {
			using namespace geometrix;
			visit(std::forward<Geometry>(geometry), std::forward<Visitor>(visitor), typename geometrix::geometry_tag_of<Geometry>::type());
		}
        
        template <typename Geometry, typename Visitor>
        void visit( Geometry&& geometry, const stk::units::length& radius, Visitor&& visitor )
        {
			using namespace geometrix;
			visit(std::forward<Geometry>(geometry), radius, std::forward<Visitor>(visitor), typename geometrix::geometry_tag_of<Geometry>::type());
		}

    protected:
		
        template <typename Point, typename Visitor>
		void visit(Point&& p, Visitor&& visitor, geometrix::geometry_tags::point_tag)
		{
			using namespace geometrix;
            auto* pCell = m_grid.find_cell(p);
            if(pCell)
                visitor(*pCell);
		}

		template <typename Segment, typename Visitor>
		void visit(Segment&& s, Visitor&& visitor, geometrix::geometry_tags::segment_tag)
		{
			using namespace geometrix;
	        fast_voxel_grid_traversal( m_grid.get_traits(), s, [&,this]( std::uint32_t i, std::uint32_t j ){ visitor(m_grid.get_cell( i, j )); }, make_tolerance_policy() );
		}

		template <typename Polyline, typename Visitor>
		void visit(Polyline&& p, Visitor&& visitor, geometrix::geometry_tags::polyline_tag)
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polyline>::type>::point_type;

            for(auto i = 0ULL, j= 1ULL; j < p.size(); i = j++)
				visit( segment<point_t>{ p[i], p[j] }, std::forward<Visitor>(visitor), geometrix::geometry_tags::segment_tag{} );
		}

		template <typename Polygon, typename Visitor>
		void visit(Polygon&& p, Visitor&& visitor, geometrix::geometry_tags::polygon_tag)
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polygon>::type>::point_type;

            for(std::size_t j = std::size_t{}, i = p.size() - 1; j < p.size(); i = j++)
				visit( segment<point_t>{ p[i], p[j] }, std::forward<Visitor>(visitor), geometrix::geometry_tags::segment_tag{} );
		}
		
		template <typename Polygon, typename Visitor>
		void visit(Polygon&& p, Visitor&& visitor, geometrix::geometry_tags::polygon_with_holes_tag)
		{
			using namespace geometrix;
			using point_t = typename geometric_traits<Polygon>::point_type;

			visit(p.get_outer(), std::forward<Visitor>(visitor));
			auto& holes = p.get_holes();
            for(auto& h : holes)
                visit(h, std::forward<Visitor>(visitor));
		}
        
        template <typename Point, typename Visitor>
		void visit(Point&& p, const stk::units::length& radius, Visitor&& visitor, geometrix::geometry_tags::point_tag)
		{
			using namespace geometrix;

            visit_cells<1>( m_grid, p, [&, this]( int i, int j ) -> void 
            {
                if( i >= 0 && j >= 0 && static_cast<unsigned int>(i) < m_grid.get_traits().get_width() && static_cast<unsigned int>(j) < m_grid.get_traits().get_height() ){
                    if(auto* pCell = m_grid.find_cell( i, j ); pCell )
                        visitor(*pCell);
                }
            } );
		}

		template <typename Segment, typename Visitor>
		void visit(Segment&& s, const stk::units::length& radius, Visitor&& visitor, geometrix::geometry_tags::segment_tag)
		{
			using namespace geometrix;
	        fast_voxel_grid_traversal( m_grid.get_traits(), s, [&,this]( std::uint32_t i, std::uint32_t j ){ visitor(m_grid.get_cell( i, j )); }, make_tolerance_policy() );
            auto v = normalize(s.get_end()-s.get_start());

            segment2 left{ s.get_start() + radius * left_normal(v), s.get_end() + radius * left_normal(v) };
            fast_voxel_grid_traversal(m_grid.get_traits(), left, [&, this](std::uint32_t i, std::uint32_t j) { visitor(m_grid.get_cell(i, j)); }, make_tolerance_policy() );

            segment2 right{ s.get_start() + radius * right_normal(v), s.get_end() + radius * right_normal(v) };
            fast_voxel_grid_traversal(m_grid.get_traits(), right, [&, this](std::uint32_t i, std::uint32_t j) { visitor(m_grid.get_cell(i, j)); }, make_tolerance_policy() );

		}

		template <typename Polyline, typename Visitor>
		void visit(Polyline&& p, const stk::units::length& radius, Visitor&& visitor, geometrix::geometry_tags::polyline_tag)
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polyline>::type>::point_type;

            for(auto i = 0ULL, j= 1ULL; j < p.size(); i = j++)
				visit( segment<point_t>{ p[i], p[j] }, radius, geometrix::geometry_tags::segment_tag{} );
		}

		template <typename Polygon, typename Visitor>
		void visit(Polygon&& p, const stk::units::length& radius, Visitor&& visitor, geometrix::geometry_tags::polygon_tag)
		{
			using namespace geometrix;
			using point_t = typename point_sequence_traits<typename std::decay<Polygon>::type>::point_type;

            for(std::size_t j = std::size_t{}, i = p.size() - 1; j < p.size(); i = j++)
				visit( segment<point_t>{ p[i], p[j] }, radius, geometrix::geometry_tags::segment_tag{} );
		}
		
		template <typename Polygon, typename Visitor>
		void visit(Polygon&& p, const stk::units::length& radius, Visitor&& visitor, geometrix::geometry_tags::polygon_with_holes_tag)
		{
			using namespace geometrix;
			using point_t = typename geometric_traits<Polygon>::point_type;

			visit(p.get_outer(), radius, std::forward<Visitor>(visitor));
			auto& holes = p.get_holes();
            for(auto& h : holes)
                visit(h, radius, std::forward<Visitor>(visitor));
		}

        using grid_type = stk::concurrent_hash_grid_2d<cell_type, geometrix::grid_traits<stk::units::length>, cell_alloc>;

        mutable junction::QSBR m_qsbr;
        grid_type              m_grid;
    };

}//namespace stk;

