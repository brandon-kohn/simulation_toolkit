
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <poly2tri/poly2tri.h>
#include <geometrix/algorithm/point_sequence/is_polygon_simple.hpp>
#include <geometrix/algorithm/polygon_with_holes_as_segment_range.hpp>
#include <geometrix/algorithm/hyperplane_partition_policies.hpp>
#include <geometrix/algorithm/solid_leaf_bsp_tree.hpp>
#include <geometrix/algorithm/distance/point_segment_closest_point.hpp>
#include <geometrix/algorithm/point_in_polygon.hpp>
#include <stk/geometry/geometry_kernel.hpp>
#include <stk/geometry/space_partition/rtree_triangle_cache.ipp>
#include <stk/utility/scope_exit.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/dynamic_bitset.hpp>
#include <exception>

namespace stk {

    inline mesh2 generate_mesh( const stk::polygon_with_holes2& polygon, const std::vector<stk::point2>& steinerPoints )
    {
        using namespace geometrix;
        if (polygon.get_outer().empty() || !is_polygon_simple(polygon.get_outer(), make_tolerance_policy()))
            throw std::invalid_argument("polygon not simple");

        for (const auto& hole : polygon.get_holes()) {
            if (!is_polygon_simple(hole, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");
        }

        std::vector<p2t::Point*> polygon_, memory;
        STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

        for( const auto& p : polygon.get_outer() ){
            polygon_.push_back( new p2t::Point( get<0>( p ).value(), get<1>( p ).value() ) );
            memory.push_back( polygon_.back() );
        }

        p2t::CDT cdt( polygon_ );

        //! Add the holes
        for( const auto& hole : polygon.get_holes() ){
            std::vector<p2t::Point*> hole_;
            for( const auto& p : hole ) {
                auto p_ = new p2t::Point( get<0>( p ).value(), get<1>( p ).value() );
                hole_.push_back( p_ );
                memory.push_back( p_ );
            }

            cdt.AddHole( hole_ );
        }

        //! Add the points
        for (const auto& p : steinerPoints) {
            auto p_ = new p2t::Point(get<0>(p).value(), get<1>(p).value());
            memory.push_back(p_);
            cdt.AddPoint(p_);
        }

        std::map<p2t::Point*, std::size_t> indices;
        auto& cdtPoints = cdt.GetPoints();
        for( std::size_t i = 0; i < cdtPoints.size(); ++i )
            indices[cdtPoints[i]] = i;

        cdt.Triangulate();

        std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();
        std::vector<std::size_t> iArray;
        polygon2 points( indices.size() );
        for( const auto& item : indices ){
            geometrix::assign( points[item.second], item.first->x * units::si::meters, item.first->y * units::si::meters);
        }

        for( auto* triangle : triangles ){
            for( int i = 0; i < 3; ++i ){
                auto* p = triangle->GetPoint( i );
                auto it = indices.find( p );
                GEOMETRIX_ASSERT( it != indices.end() );
                iArray.push_back( it->second );
            }
        }

        return mesh2( points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder() );
    }

	struct triangle_area_distance_weight_policy
	{
		using weight_type = double;
		using normalized_type = double;

		triangle_area_distance_weight_policy(solid_bsp2* pBSP, const stk::units::length& distanceSaturation, double attractionStrength)
			: pBSP(pBSP)
			, distanceSaturation(distanceSaturation * distanceSaturation)
			, attractionStrength(attractionStrength)
		{}

		template <typename Triangle>
		weight_type get_weight(Triangle&& trig) const
		{
			using namespace geometrix;
			using std::exp;

			auto area = get_area(std::forward<Triangle>(trig));
			std::size_t idx;
			auto distanceSqrd = pBSP->get_min_distance_sqrd_to_solid(get_centroid(trig), idx, make_tolerance_policy());
			auto d2 = std::max(distanceSqrd.value(), distanceSaturation.value());
			double f = area.value() * exp(-attractionStrength * d2);
			return f;
		}

		normalized_type normalize(const weight_type& a, const weight_type& total) const
		{
			return a / total;
		}

		weight_type initial_weight() const
		{
			return weight_type{};
		}

		solid_bsp2* pBSP{ nullptr };
		stk::units::area distanceSaturation{ 1.0 * stk::units::si::square_meters };
		double attractionStrength{ 1.0 };
		
	};

	template <typename WeightPolicy>
    inline geometrix::mesh_2d<stk::units::length, geometrix::mesh_traits<stk::rtree_triangle_cache, WeightPolicy>> generate_weighted_mesh( const stk::polygon_with_holes2& polygon, std::vector<stk::point2>& steinerPoints, WeightPolicy&& weightPolicy)
    {
        using namespace geometrix;
        if (polygon.get_outer().empty() || !is_polygon_simple(polygon.get_outer(), make_tolerance_policy()))
            throw std::invalid_argument("polygon not simple");

        for (const auto& hole : polygon.get_holes()) {
            if (!is_polygon_simple(hole, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");
        }

        std::vector<p2t::Point*> polygon_, memory;
        STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

        for( const auto& p : polygon.get_outer() ){
            polygon_.push_back( new p2t::Point( get<0>( p ).value(), get<1>( p ).value() ) );
            memory.push_back( polygon_.back() );
        }

        p2t::CDT cdt( polygon_ );

        //! Add the holes
        for( const auto& hole : polygon.get_holes() ){
            std::vector<p2t::Point*> hole_;
            for( const auto& p : hole ) {
                auto p_ = new p2t::Point( get<0>( p ).value(), get<1>( p ).value() );
                hole_.push_back( p_ );
                memory.push_back( p_ );
            }

            cdt.AddHole( hole_ );
        }

        //! Add the points
        for (const auto& p : steinerPoints) {
            auto p_ = new p2t::Point(get<0>(p).value(), get<1>(p).value());
            memory.push_back(p_);
            cdt.AddPoint(p_);
        }

        std::map<p2t::Point*, std::size_t> indices;
        auto& cdtPoints = cdt.GetPoints();
        for( std::size_t i = 0; i < cdtPoints.size(); ++i )
            indices[cdtPoints[i]] = i;

        cdt.Triangulate();

        std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();
        std::vector<std::size_t> iArray;
        polygon2 points( indices.size() );
        for( const auto& item : indices ){
            geometrix::assign( points[item.second], item.first->x * units::si::meters, item.first->y * units::si::meters);
        }

        for( auto* triangle : triangles ){
            for( int i = 0; i < 3; ++i ){
                auto* p = triangle->GetPoint( i );
                auto it = indices.find( p );
                GEOMETRIX_ASSERT( it != indices.end() );
                iArray.push_back( it->second );
            }
        }

        return geometrix::mesh_2d<stk::units::length, geometrix::mesh_traits<stk::rtree_triangle_cache, WeightPolicy>>( points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder(), std::forward<WeightPolicy>(weightPolicy) );
    }

	/*
    template <typename Polygon, typename Points>
    inline mesh2 generate_mesh(const Polygon& polygon, const Points& steinerPoints)
    {
        using namespace geometrix;
        if (polygon.empty() || !is_polygon_simple(polygon, make_tolerance_policy()))
            throw std::exception("polygon not simple");

        std::vector<p2t::Point*> polygon_, memory;
        STK_SCOPE_EXIT(for (auto p : memory) delete p; );

        for (const auto& p : polygon)
        {
            polygon_.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
            memory.push_back(polygon_.back());
        }

        p2t::CDT cdt(polygon_);

        for (const auto& p : steinerPoints)
        {
            auto p_ = new p2t::Point(get<0>(p).value(), get<1>(p).value());
            memory.push_back(p_);
            cdt.AddPoint(p_);
        }

        std::map<p2t::Point*, std::size_t> indices;
        auto& cdtPoints = cdt.GetPoints();
        for (std::size_t i = 0; i < cdtPoints.size(); ++i)
            indices[cdtPoints[i]] = i;

        cdt.Triangulate();

        std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();
        std::vector<std::size_t> iArray;
        polygon2 points(indices.size());
        for (const auto& item : indices)
            geometrix::assign(points[item.second], item.first->x * units::si::meters, item.first->y * units::si::meters);

        for (auto* triangle : triangles)
        {
            for (int i = 0; i < 3; ++i)
            {
                auto* p = triangle->GetPoint(i);
                auto it = indices.find(p);
                GEOMETRIX_ASSERT(it != indices.end());
                iArray.push_back(it->second);
            }
        }

        return mesh2(points, iArray, make_tolerance_policy(), mesh2::weight_policy(), stk::rtree_triangle_cache_builder());
    }
	*/

    inline polygon2 get_outer_polygon(point2 ll, point2 ur, units::length offset)
    {
        using namespace geometrix;

        ll = ll - vector2{ offset, offset };
        ur = ur + vector2{ offset, offset };

        return { ll, point2{ get<0>(ur), get<1>(ll) }, ur, point2{ get<0>(ll), get<1>(ur) } };
    }

	inline std::vector<point2> generate_fine_steiner_points(const polygon_with_holes2& pgon, const stk::units::length& cell, const solid_bsp2& bsp)
	{
		using namespace geometrix;
		std::set<point2> results;

		auto cmp = make_tolerance_policy();
		auto obounds = get_bounds(pgon.get_outer(), cmp);
		auto grid = grid_traits<stk::units::length>(obounds, cell);
		auto mesh = generate_mesh(pgon, std::vector<point2>{});

		for (auto q = 0UL; q < mesh.get_number_triangles(); ++q)
		{
			auto& trig = mesh.get_triangle_vertices(q);

			stk::units::length xmin, xmax, ymin, ymax;
			std::tie(xmin, xmax, ymin, ymax) = get_bounds(trig, cmp);

			std::uint32_t imin = grid.get_x_index(xmin), imax = grid.get_x_index(xmax), jmin = grid.get_y_index(ymin), jmax = grid.get_y_index(ymax);
			for (auto j = jmin; j <= jmax; ++j)
			{
				for (auto i = imin; i <= imax; ++i)
				{
					auto c = grid.get_cell_centroid(i, j);
					std::size_t idx;
					auto d2 = bsp.get_min_distance_sqrd_to_solid(c, idx, cmp);
					if (d2 > 1.0 * stk::units::si::square_meters && point_in_triangle(c, trig[0], trig[1], trig[2], cmp))
						results.insert(c);
				}
			}
		}

		return std::vector<point2>(results.begin(), results.end());
	}

}//! namespace stk;


#include <geometrix/utility/scope_timer.ipp>
TEST(weighted_mesh_test_suite, start)
{
    using namespace geometrix;
    using namespace stk;
    using boost::adaptors::transformed;

	GEOMETRIX_MEASURE_SCOPE_TIME("weighted mesh");

    auto cmp = make_tolerance_policy();
    std::vector<polygon2> areas = {
        //  (polygon2{ { -59.842988908174448 * boost::units::si::meters, 161.51553271198645 * boost::units::si::meters },{ -59.567088058101945 * boost::units::si::meters, 162.04732614941895 * boost::units::si::meters },{ -59.101964120694902 * boost::units::si::meters, 162.10330136166885 * boost::units::si::meters },{ -59.098964120668825 * boost::units::si::meters, 162.10430136183277 * boost::units::si::meters },{ -12.036964120692573 * boost::units::si::meters, 162.41030136169866 * boost::units::si::meters },{ -11.979964120662771 * boost::units::si::meters, 162.3063013618812 * boost::units::si::meters },{ -13.347964120679535 * boost::units::si::meters, 160.199301361572 * boost::units::si::meters },{ -13.346964120690245 * boost::units::si::meters, 160.19830136187375 * boost::units::si::meters },{ -6.7819641206879169 * boost::units::si::meters, 160.20030136173591 * boost::units::si::meters },{ -6.7589641207014211 * boost::units::si::meters, 162.30430136201903 * boost::units::si::meters },{ -6.7009641206823289 * boost::units::si::meters, 162.36130136158317 * boost::units::si::meters },{ 1.9070358793251216 * boost::units::si::meters, 162.47630136180669 * boost::units::si::meters },{ 6.1220358792925254 * boost::units::si::meters, 162.52130136173218 * boost::units::si::meters },{ 6.1810358793009073 * boost::units::si::meters, 162.46130136167631 * boost::units::si::meters },{ 6.1700358793023042 * boost::units::si::meters, 160.32830136176199 * boost::units::si::meters },{ 6.171035879291594 * boost::units::si::meters, 160.32730136159807 * boost::units::si::meters },{ 13.375935879303142 * boost::units::si::meters, 160.3863013619557 * boost::units::si::meters },{ 13.376935879292432 * boost::units::si::meters, 160.38730136165395 * boost::units::si::meters },{ 13.353935879305936 * boost::units::si::meters, 162.48130136169493 * boost::units::si::meters },{ 13.410935879335739 * boost::units::si::meters, 162.5403013615869 * boost::units::si::meters },{ 23.190447447297629 * boost::units::si::meters, 162.66430199984461 * boost::units::si::meters },{ 23.073172725096811 * boost::units::si::meters, 163.8163536824286 * boost::units::si::meters },{ 13.443935879331548 * boost::units::si::meters, 163.73030136199668 * boost::units::si::meters },{ 13.384935879323166 * boost::units::si::meters, 163.78730136202648 * boost::units::si::meters },{ 13.326935879304074 * boost::units::si::meters, 166.49730136198923 * boost::units::si::meters },{ 6.2540358793339692 * boost::units::si::meters, 166.39930136175826 * boost::units::si::meters },{ 6.2240358793060295 * boost::units::si::meters, 163.82730136159807 * boost::units::si::meters },{ 6.1660358792869374 * boost::units::si::meters, 163.77030136156827 * boost::units::si::meters },{ 1.9030358793097548 * boost::units::si::meters, 163.70730136195198 * boost::units::si::meters },{ -5.1529641206725501 * boost::units::si::meters, 163.65030136192217 * boost::units::si::meters },{ -5.2129641206702217 * boost::units::si::meters, 163.70930136181414 * boost::units::si::meters },{ -5.2129641206702217 * boost::units::si::meters, 167.92330136196688 * boost::units::si::meters },{ -5.1549641207093373 * boost::units::si::meters, 167.98230136185884 * boost::units::si::meters },{ 2.0060358793125488 * boost::units::si::meters, 168.02830136194825 * boost::units::si::meters },{ 2.0090358793386258 * boost::units::si::meters, 169.41630136175081 * boost::units::si::meters },{ 2.0080358792911284 * boost::units::si::meters, 169.41730136191472 * boost::units::si::meters },{ -6.8139641206944361 * boost::units::si::meters, 169.3523013619706 * boost::units::si::meters },{ -6.7209641207009554 * boost::units::si::meters, 163.82730136159807 * boost::units::si::meters },{ -6.7769641206832603 * boost::units::si::meters, 163.76830136170611 * boost::units::si::meters },{ -11.881964120664634 * boost::units::si::meters, 163.54130136175081 * boost::units::si::meters },{ -11.88696412066929 * boost::units::si::meters, 163.54130136175081 * boost::units::si::meters },{ -12.470964120700955 * boost::units::si::meters, 163.57230136170983 * boost::units::si::meters },{ -59.089964120707009 * boost::units::si::meters, 163.16130136186257 * boost::units::si::meters },{ -59.106964120699558 * boost::units::si::meters, 163.16030136169866 * boost::units::si::meters },{ -59.138964120706078 * boost::units::si::meters, 163.16030136169866 * boost::units::si::meters },{ -59.140964120684657 * boost::units::si::meters, 163.15930136200041 * boost::units::si::meters },{ -60.326964120671619 * boost::units::si::meters, 163.09630136191845 * boost::units::si::meters },{ -60.363964120682795 * boost::units::si::meters, 163.10930136172101 * boost::units::si::meters },{ -62.098964120668825 * boost::units::si::meters, 164.67330136196688 * boost::units::si::meters },{ -62.115964120661374 * boost::units::si::meters, 164.71030136197805 * boost::units::si::meters },{ -62.157964120677207 * boost::units::si::meters, 165.68630136176944 * boost::units::si::meters },{ -62.157964120677207 * boost::units::si::meters, 165.68730136193335 * boost::units::si::meters },{ -62.191964120662306 * boost::units::si::meters, 168.94830136187375 * boost::units::si::meters },{ -62.133964120701421 * boost::units::si::meters, 169.00830136192963 * boost::units::si::meters },{ -57.079964120697696 * boost::units::si::meters, 169.05130136199296 * boost::units::si::meters },{ -57.073964120703749 * boost::units::si::meters, 170.63330136192963 * boost::units::si::meters },{ -57.074964120693039 * boost::units::si::meters, 170.63430136162788 * boost::units::si::meters },{ -60.910964120703284 * boost::units::si::meters, 170.6023013619706 * boost::units::si::meters },{ -62.163964120671153 * boost::units::si::meters, 170.5903013618663 * boost::units::si::meters },{ -62.222964120679535 * boost::units::si::meters, 170.6473013618961 * boost::units::si::meters },{ -62.299964120669756 * boost::units::si::meters, 175.73630136158317 * boost::units::si::meters },{ -62.241964120708872 * boost::units::si::meters, 175.7953013619408 * boost::units::si::meters },{ -60.893964120710734 * boost::units::si::meters, 175.824301361572 * boost::units::si::meters },{ -60.894964120700024 * boost::units::si::meters, 175.824301361572 * boost::units::si::meters },{ -60.970964120700955 * boost::units::si::meters, 181.81730136182159 * boost::units::si::meters },{ -61.634964120690711 * boost::units::si::meters, 181.80530136171728 * boost::units::si::meters },{ -61.640964120684657 * boost::units::si::meters, 181.80530136171728 * boost::units::si::meters },{ -62.615964120661374 * boost::units::si::meters, 181.92230136180297 * boost::units::si::meters },{ -62.66596412070794 * boost::units::si::meters, 181.97930136183277 * boost::units::si::meters },{ -62.658964120666496 * boost::units::si::meters, 185.99130136193708 * boost::units::si::meters },{ -62.645964120689314 * boost::units::si::meters, 186.02530136192217 * boost::units::si::meters },{ -62.269964120700024 * boost::units::si::meters, 186.44030136195943 * boost::units::si::meters },{ -62.267964120663237 * boost::units::si::meters, 189.92230136180297 * boost::units::si::meters },{ -62.628964120696764 * boost::units::si::meters, 190.39830136159435 * boost::units::si::meters },{ -62.639964120695367 * boost::units::si::meters, 190.42930136201903 * boost::units::si::meters },{ -62.718964120664168 * boost::units::si::meters, 196.25930136162788 * boost::units::si::meters },{ -62.690964120673016 * boost::units::si::meters, 196.30730136157945 * boost::units::si::meters },{ -62.316964120662306 * boost::units::si::meters, 196.51230136165395 * boost::units::si::meters },{ -62.29396412067581 * boost::units::si::meters, 196.51830136170611 * boost::units::si::meters },{ -60.749964120681398 * boost::units::si::meters, 196.57830136176199 * boost::units::si::meters },{ -60.749964120681398 * boost::units::si::meters, 196.5793013619259 * boost::units::si::meters },{ -60.829964120697696 * boost::units::si::meters, 202.53330136183649 * boost::units::si::meters },{ -62.360964120714925 * boost::units::si::meters, 202.47330136178061 * boost::units::si::meters },{ -62.421964120701887 * boost::units::si::meters, 202.53130136197433 * boost::units::si::meters },{ -62.571964120666962 * boost::units::si::meters, 208.10330136166885 * boost::units::si::meters },{ -62.571964120666962 * boost::units::si::meters, 208.10530136199668 * boost::units::si::meters },{ -62.881964120664634 * boost::units::si::meters, 208.65630136197433 * boost::units::si::meters },{ -62.888964120706078 * boost::units::si::meters, 208.67930136201903 * boost::units::si::meters },{ -62.91696412069723 * boost::units::si::meters, 209.83830136200413 * boost::units::si::meters },{ -62.898964120715391 * boost::units::si::meters, 209.87830136157572 * boost::units::si::meters },{ -62.523964120715391 * boost::units::si::meters, 210.21230136184022 * boost::units::si::meters },{ -62.259964120690711 * boost::units::si::meters, 211.41430136188865 * boost::units::si::meters },{ -62.206964120676275 * boost::units::si::meters, 211.45830136165023 * boost::units::si::meters },{ -59.493964120687451 * boost::units::si::meters, 211.5403013615869 * boost::units::si::meters },{ -59.492964120698161 * boost::units::si::meters, 211.5403013615869 * boost::units::si::meters },{ -59.506964120664634 * boost::units::si::meters, 212.72630136180669 * boost::units::si::meters },{ -64.735964120714925 * boost::units::si::meters, 212.68430136190727 * boost::units::si::meters },{ -64.998964120692108 * boost::units::si::meters, 211.46430136170238 * boost::units::si::meters },{ -65.001964120659977 * boost::units::si::meters, 211.45830136165023 * boost::units::si::meters },{ -65.001964120659977 * boost::units::si::meters, 211.45730136195198 * boost::units::si::meters },{ -65.002964120707475 * boost::units::si::meters, 211.44630136201158 * boost::units::si::meters },{ -65.003964120696764 * boost::units::si::meters, 211.44030136195943 * boost::units::si::meters },{ -65.000964120670687 * boost::units::si::meters, 211.4313013618812 * boost::units::si::meters },{ -64.936964120715857 * boost::units::si::meters, 202.31130136176944 * boost::units::si::meters },{ -64.935964120668359 * boost::units::si::meters, 202.30830136174336 * boost::units::si::meters },{ -64.860964120714925 * boost::units::si::meters, 196.02730136178434 * boost::units::si::meters },{ -64.716964120685589 * boost::units::si::meters, 182.10430136183277 * boost::units::si::meters },{ -64.689964120683726 * boost::units::si::meters, 175.37630136171356 * boost::units::si::meters },{ -64.655964120698627 * boost::units::si::meters, 168.41130136186257 * boost::units::si::meters },{ -64.655964120698627 * boost::units::si::meters, 168.41030136169866 * boost::units::si::meters },{ -64.66796412068652 * boost::units::si::meters, 165.70330136176199 * boost::units::si::meters },{ -64.66896412067581 * boost::units::si::meters, 165.70130136189982 * boost::units::si::meters },{ -64.646964120678604 * boost::units::si::meters, 164.42530136182904 * boost::units::si::meters },{ -64.371964120713528 * boost::units::si::meters, 163.34530136175454 * boost::units::si::meters },{ -64.189964120683726 * boost::units::si::meters, 162.70730136195198 * boost::units::si::meters },{ -64.188964120694436 * boost::units::si::meters, 162.7043013619259 * boost::units::si::meters },{ -63.38696412066929 * boost::units::si::meters, 161.4313013618812 * boost::units::si::meters },{ -62.644964120700024 * boost::units::si::meters, 160.78430136200041 * boost::units::si::meters },{ -61.757964120712131 * boost::units::si::meters, 160.40330136194825 * boost::units::si::meters },{ -61.751964120659977 * boost::units::si::meters, 160.40130136162043 * boost::units::si::meters },{ -61.009964120690711 * boost::units::si::meters, 160.19530136184767 * boost::units::si::meters },{ -61.008964120701421 * boost::units::si::meters, 160.19530136184767 * boost::units::si::meters },{ -61.004964120686054 * boost::units::si::meters, 160.19430136168376 * boost::units::si::meters },{ -59.743964120687451 * boost::units::si::meters, 159.95230136159807 * boost::units::si::meters },{ -59.739964120672084 * boost::units::si::meters, 159.95230136159807 * boost::units::si::meters },{ -59.236964120704215 * boost::units::si::meters, 159.9153013615869 * boost::units::si::meters } })
        //, (polygon2{ { -126.81296412070515 * boost::units::si::meters, 159.53230136167258 * boost::units::si::meters },{ -126.77596412069397 * boost::units::si::meters, 159.54730136180297 * boost::units::si::meters },{ -126.14496412070002 * boost::units::si::meters, 159.54830136196688 * boost::units::si::meters },{ -125.30996412067907 * boost::units::si::meters, 159.56030136160553 * boost::units::si::meters },{ -123.14796412066789 * boost::units::si::meters, 159.57030136184767 * boost::units::si::meters },{ -120.15096412069397 * boost::units::si::meters, 159.59130136156455 * boost::units::si::meters },{ -119.14996412070468 * boost::units::si::meters, 159.59530136175454 * boost::units::si::meters },{ -118.70996412070235 * boost::units::si::meters, 161.21830136189237 * boost::units::si::meters },{ -118.58896412071772 * boost::units::si::meters, 161.24230136163533 * boost::units::si::meters },{ -117.92296412069118 * boost::units::si::meters, 160.37830136157572 * boost::units::si::meters },{ -117.9209641207126 * boost::units::si::meters, 160.37630136171356 * boost::units::si::meters },{ -116.9429641207098 * boost::units::si::meters, 159.88230136176571 * boost::units::si::meters },{ -116.93896412069444 * boost::units::si::meters, 159.88030136190355 * boost::units::si::meters },{ -115.40296412067255 * boost::units::si::meters, 159.59430136159062 * boost::units::si::meters },{ -115.37896412069676 * boost::units::si::meters, 161.9433013619855 * boost::units::si::meters },{ -115.31896412069909 * boost::units::si::meters, 162.00130136171356 * boost::units::si::meters },{ -101.30396412068512 * boost::units::si::meters, 161.86930136196315 * boost::units::si::meters },{ -101.30196412070654 * boost::units::si::meters, 161.86830136179924 * boost::units::si::meters },{ -101.24196412070887 * boost::units::si::meters, 161.86830136179924 * boost::units::si::meters },{ -83.41596412070794 * boost::units::si::meters, 161.8863013619557 * boost::units::si::meters },{ -81.687964120705146 * boost::units::si::meters, 161.94030136195943 * boost::units::si::meters },{ -81.627964120707475 * boost::units::si::meters, 161.88430136162788 * boost::units::si::meters },{ -81.535964120703284 * boost::units::si::meters, 159.71730136172846 * boost::units::si::meters },{ -80.141964120673947 * boost::units::si::meters, 159.71430136170238 * boost::units::si::meters },{ -80.137964120716788 * boost::units::si::meters, 159.71230136184022 * boost::units::si::meters },{ -80.119964120676741 * boost::units::si::meters, 159.71430136170238 * boost::units::si::meters },{ -80.105964120710269 * boost::units::si::meters, 159.71430136170238 * boost::units::si::meters },{ -80.103964120673481 * boost::units::si::meters, 159.7153013618663 * boost::units::si::meters },{ -78.94796412071446 * boost::units::si::meters, 159.81930136168376 * boost::units::si::meters },{ -78.945964120677672 * boost::units::si::meters, 159.81930136168376 * boost::units::si::meters },{ -78.246964120713528 * boost::units::si::meters, 159.96230136184022 * boost::units::si::meters },{ -78.240964120661374 * boost::units::si::meters, 159.96430136170238 * boost::units::si::meters },{ -77.574964120693039 * boost::units::si::meters, 160.25030136201531 * boost::units::si::meters },{ -77.005964120675344 * boost::units::si::meters, 160.62630136171356 * boost::units::si::meters },{ -75.845964120700955 * boost::units::si::meters, 161.59930136194453 * boost::units::si::meters },{ -75.843964120664168 * boost::units::si::meters, 161.60130136180669 * boost::units::si::meters },{ -75.361964120704215 * boost::units::si::meters, 162.34930136194453 * boost::units::si::meters },{ -75.360964120714925 * boost::units::si::meters, 162.35130136180669 * boost::units::si::meters },{ -74.972964120679535 * boost::units::si::meters, 163.81930136168376 * boost::units::si::meters },{ -74.90096412069397 * boost::units::si::meters, 165.29630136163905 * boost::units::si::meters },{ -74.90096412069397 * boost::units::si::meters, 165.29830136196688 * boost::units::si::meters },{ -74.860964120714925 * boost::units::si::meters, 165.70830136165023 * boost::units::si::meters },{ -74.859964120667428 * boost::units::si::meters, 165.71130136167631 * boost::units::si::meters },{ -74.859964120667428 * boost::units::si::meters, 165.72630136180669 * boost::units::si::meters },{ -74.857964120688848 * boost::units::si::meters, 165.74430136196315 * boost::units::si::meters },{ -74.859964120667428 * boost::units::si::meters, 165.74830136168748 * boost::units::si::meters },{ -74.978964120673481 * boost::units::si::meters, 194.2153013618663 * boost::units::si::meters },{ -74.979964120662771 * boost::units::si::meters, 194.21630136156455 * boost::units::si::meters },{ -75.021964120678604 * boost::units::si::meters, 200.42930136201903 * boost::units::si::meters },{ -75.049964120669756 * boost::units::si::meters, 206.92830136185512 * boost::units::si::meters },{ -75.053964120685123 * boost::units::si::meters, 210.60630136169493 * boost::units::si::meters },{ -75.04296412068652 * boost::units::si::meters, 212.5973013616167 * boost::units::si::meters },{ -75.04396412067581 * boost::units::si::meters, 212.59830136178061 * boost::units::si::meters },{ -77.55796412070049 * boost::units::si::meters, 212.58230136195198 * boost::units::si::meters },{ -77.562964120705146 * boost::units::si::meters, 211.20730136195198 * boost::units::si::meters },{ -77.567964120709803 * boost::units::si::meters, 211.18430136190727 * boost::units::si::meters },{ -77.838964120717719 * boost::units::si::meters, 210.6473013618961 * boost::units::si::meters },{ -77.861964120704215 * boost::units::si::meters, 210.62430136185139 * boost::units::si::meters },{ -78.385964120680001 * boost::units::si::meters, 210.35930136172101 * boost::units::si::meters },{ -78.40296412067255 * boost::units::si::meters, 210.35430136183277 * boost::units::si::meters },{ -79.453964120708406 * boost::units::si::meters, 210.23030136199668 * boost::units::si::meters },{ -79.459964120702352 * boost::units::si::meters, 210.23030136199668 * boost::units::si::meters },{ -80.584964120702352 * boost::units::si::meters, 210.24430136196315 * boost::units::si::meters },{ -80.596964120690245 * boost::units::si::meters, 210.24630136182532 * boost::units::si::meters },{ -81.818964120699093 * boost::units::si::meters, 210.56130136176944 * boost::units::si::meters },{ -81.798964120680466 * boost::units::si::meters, 207.36230136174709 * boost::units::si::meters },{ -81.796964120701887 * boost::units::si::meters, 207.36030136188492 * boost::units::si::meters },{ -80.715964120696299 * boost::units::si::meters, 207.23030136199668 * boost::units::si::meters },{ -79.055964120663702 * boost::units::si::meters, 207.02730136178434 * boost::units::si::meters },{ -77.837964120670222 * boost::units::si::meters, 206.88230136176571 * boost::units::si::meters },{ -77.787964120681863 * boost::units::si::meters, 206.824301361572 * boost::units::si::meters },{ -77.848964120668825 * boost::units::si::meters, 201.26530136168003 * boost::units::si::meters },{ -77.909964120713994 * boost::units::si::meters, 201.20730136195198 * boost::units::si::meters },{ -82.40296412067255 * boost::units::si::meters, 201.31930136168376 * boost::units::si::meters },{ -82.398964120715391 * boost::units::si::meters, 194.9773013619706 * boost::units::si::meters },{ -81.448964120703749 * boost::units::si::meters, 194.69130136165768 * boost::units::si::meters },{ -81.409964120713994 * boost::units::si::meters, 194.63730136165395 * boost::units::si::meters },{ -81.425964120717254 * boost::units::si::meters, 193.08730136184022 * boost::units::si::meters },{ -77.592964120674878 * boost::units::si::meters, 191.83430136181414 * boost::units::si::meters },{ -77.554964120674413 * boost::units::si::meters, 191.78230136167258 * boost::units::si::meters },{ -77.338964120717719 * boost::units::si::meters, 165.67730136169121 * boost::units::si::meters },{ -77.339964120707009 * boost::units::si::meters, 165.66730136191472 * boost::units::si::meters },{ -77.517964120663237 * boost::units::si::meters, 164.7453013616614 * boost::units::si::meters },{ -77.539964120660443 * boost::units::si::meters, 164.71130136167631 * boost::units::si::meters },{ -80.036964120692573 * boost::units::si::meters, 163.08330136165023 * boost::units::si::meters },{ -80.062964120705146 * boost::units::si::meters, 163.074301361572 * boost::units::si::meters },{ -83.303964120685123 * boost::units::si::meters, 162.97630136180669 * boost::units::si::meters },{ -83.305964120663702 * boost::units::si::meters, 162.97630136180669 * boost::units::si::meters },{ -101.46796412067488 * boost::units::si::meters, 163.1933013619855 * boost::units::si::meters },{ -101.46996412071167 * boost::units::si::meters, 163.19230136182159 * boost::units::si::meters },{ -115.51396412070608 * boost::units::si::meters, 163.21630136156455 * boost::units::si::meters },{ -115.52296412066789 * boost::units::si::meters, 163.21730136172846 * boost::units::si::meters },{ -118.52396412071539 * boost::units::si::meters, 163.79730136180297 * boost::units::si::meters },{ -118.56996412068838 * boost::units::si::meters, 163.8523013619706 * boost::units::si::meters },{ -118.51296412071679 * boost::units::si::meters, 180.76930136187002 * boost::units::si::meters },{ -118.51396412070608 * boost::units::si::meters, 180.77030136156827 * boost::units::si::meters },{ -123.33196412067628 * boost::units::si::meters, 180.91330136172473 * boost::units::si::meters },{ -123.33296412066557 * boost::units::si::meters, 180.91230136202648 * boost::units::si::meters },{ -123.30896412068978 * boost::units::si::meters, 168.43930136179551 * boost::units::si::meters },{ -123.2159641206963 * boost::units::si::meters, 163.1883013616316 * boost::units::si::meters },{ -123.27396412071539 * boost::units::si::meters, 163.12930136173964 * boost::units::si::meters },{ -126.88896412070608 * boost::units::si::meters, 163.06130136176944 * boost::units::si::meters },{ -126.88996412069537 * boost::units::si::meters, 163.06130136176944 * boost::units::si::meters },{ -135.26654051680816 * boost::units::si::meters, 163.07025714917108 * boost::units::si::meters },{ -136.06069755629869 * boost::units::si::meters, 159.61333827115595 * boost::units::si::meters },{ -135.27796412067255 * boost::units::si::meters, 159.60630136169493 * boost::units::si::meters },{ -129.1249641206814 * boost::units::si::meters, 159.54930136166513 * boost::units::si::meters },{ -126.82096412067767 * boost::units::si::meters, 159.52430136175826 * boost::units::si::meters } })
        //, (polygon2{ { -70.234964120667428 * boost::units::si::meters, 119.72530136164278 * boost::units::si::meters },{ -69.461964120680932 * boost::units::si::meters, 120.24930136185139 * boost::units::si::meters },{ -69.458964120713063 * boost::units::si::meters, 120.25230136187747 * boost::units::si::meters },{ -69.160964120703284 * boost::units::si::meters, 120.76330136181787 * boost::units::si::meters },{ -69.160964120703284 * boost::units::si::meters, 120.76430136198178 * boost::units::si::meters },{ -69.132964120712131 * boost::units::si::meters, 122.36530136177316 * boost::units::si::meters },{ -69.147964120667893 * boost::units::si::meters, 122.82230136170983 * boost::units::si::meters },{ -69.147964120667893 * boost::units::si::meters, 122.82330136187375 * boost::units::si::meters },{ -69.207964120665565 * boost::units::si::meters, 125.8403013618663 * boost::units::si::meters },{ -69.207964120665565 * boost::units::si::meters, 125.84130136156455 * boost::units::si::meters },{ -69.213964120717719 * boost::units::si::meters, 128.53030136181042 * boost::units::si::meters },{ -69.229964120662771 * boost::units::si::meters, 131.92530136182904 * boost::units::si::meters },{ -69.282964120677207 * boost::units::si::meters, 134.21830136189237 * boost::units::si::meters },{ -69.342964120674878 * boost::units::si::meters, 137.07530136173591 * boost::units::si::meters },{ -69.342964120674878 * boost::units::si::meters, 137.07730136159807 * boost::units::si::meters },{ -69.30796412070049 * boost::units::si::meters, 138.7153013618663 * boost::units::si::meters },{ -69.51196412066929 * boost::units::si::meters, 141.08430136181414 * boost::units::si::meters },{ -69.512964120716788 * boost::units::si::meters, 141.08630136167631 * boost::units::si::meters },{ -70.024964120704681 * boost::units::si::meters, 141.49130136193708 * boost::units::si::meters },{ -70.029964120709337 * boost::units::si::meters, 141.4953013616614 * boost::units::si::meters },{ -70.030964120698627 * boost::units::si::meters, 141.49630136182532 * boost::units::si::meters },{ -70.04096412070794 * boost::units::si::meters, 141.49430136196315 * boost::units::si::meters },{ -70.15196412068326 * boost::units::si::meters, 141.49830136168748 * boost::units::si::meters },{ -70.659964120713994 * boost::units::si::meters, 141.09430136159062 * boost::units::si::meters },{ -71.225964120705612 * boost::units::si::meters, 140.32530136173591 * boost::units::si::meters },{ -71.458964120713063 * boost::units::si::meters, 139.29430136177689 * boost::units::si::meters },{ -71.458964120713063 * boost::units::si::meters, 139.2903013615869 * boost::units::si::meters },{ -71.491964120708872 * boost::units::si::meters, 137.9653013618663 * boost::units::si::meters },{ -71.491964120708872 * boost::units::si::meters, 137.96330136200413 * boost::units::si::meters },{ -71.486964120704215 * boost::units::si::meters, 137.32830136176199 * boost::units::si::meters },{ -71.335964120691642 * boost::units::si::meters, 129.36530136177316 * boost::units::si::meters },{ -71.314964120683726 * boost::units::si::meters, 127.40330136194825 * boost::units::si::meters },{ -71.228964120673481 * boost::units::si::meters, 123.4313013618812 * boost::units::si::meters },{ -71.174964120669756 * boost::units::si::meters, 121.26330136181787 * boost::units::si::meters },{ -71.174964120669756 * boost::units::si::meters, 121.26030136179179 * boost::units::si::meters },{ -71.203964120708406 * boost::units::si::meters, 120.3403013618663 * boost::units::si::meters },{ -71.203964120708406 * boost::units::si::meters, 120.33830136200413 * boost::units::si::meters },{ -70.949964120693039 * boost::units::si::meters, 119.96830136189237 * boost::units::si::meters },{ -70.946964120666962 * boost::units::si::meters, 119.96630136156455 * boost::units::si::meters },{ -70.366964120708872 * boost::units::si::meters, 119.75130136171356 * boost::units::si::meters },{ -70.262964120716788 * boost::units::si::meters, 119.72930136183277 * boost::units::si::meters },{ -70.260964120680001 * boost::units::si::meters, 119.72830136166885 * boost::units::si::meters },{ -70.256964120664634 * boost::units::si::meters, 119.7273013619706 * boost::units::si::meters },{ -70.239964120672084 * boost::units::si::meters, 119.72330136178061 * boost::units::si::meters } })
        (polygon2{ { -70.155964120698627 * boost::units::si::meters, 104.33330136165023 * boost::units::si::meters },{ -70.15396412066184 * boost::units::si::meters, 104.33730136184022 * boost::units::si::meters },{ -69.719964120711666 * boost::units::si::meters, 104.66930136177689 * boost::units::si::meters },{ -69.410964120703284 * boost::units::si::meters, 105.07830136176199 * boost::units::si::meters },{ -69.250964120670687 * boost::units::si::meters, 105.51330136181787 * boost::units::si::meters },{ -69.239964120672084 * boost::units::si::meters, 107.16830136161298 * boost::units::si::meters },{ -69.249964120681398 * boost::units::si::meters, 109.04130136175081 * boost::units::si::meters },{ -69.550964120717254 * boost::units::si::meters, 109.91830136161298 * boost::units::si::meters },{ -70.169964120665099 * boost::units::si::meters, 110.32330136187375 * boost::units::si::meters },{ -70.172964120691177 * boost::units::si::meters, 110.32530136173591 * boost::units::si::meters },{ -70.174964120669756 * boost::units::si::meters, 110.32630136189982 * boost::units::si::meters },{ -70.177964120695833 * boost::units::si::meters, 110.32530136173591 * boost::units::si::meters },{ -70.553964120685123 * boost::units::si::meters, 110.21730136172846 * boost::units::si::meters },{ -71.014964120695367 * boost::units::si::meters, 109.94030136195943 * boost::units::si::meters },{ -71.271964120678604 * boost::units::si::meters, 109.39430136187002 * boost::units::si::meters },{ -71.272964120667893 * boost::units::si::meters, 109.38930136198178 * boost::units::si::meters },{ -71.264964120695367 * boost::units::si::meters, 108.33530136197805 * boost::units::si::meters },{ -71.253964120696764 * boost::units::si::meters, 106.74930136185139 * boost::units::si::meters },{ -71.241964120708872 * boost::units::si::meters, 105.1653013615869 * boost::units::si::meters },{ -71.240964120661374 * boost::units::si::meters, 105.15930136200041 * boost::units::si::meters },{ -71.148964120715391 * boost::units::si::meters, 104.80230136169121 * boost::units::si::meters },{ -70.230964120710269 * boost::units::si::meters, 104.34930136194453 * boost::units::si::meters },{ -70.174964120669756 * boost::units::si::meters, 104.32130136201158 * boost::units::si::meters } })
        //, (polygon2{ { -78.002138914249372 * boost::units::si::meters, 95.595670043956488 * boost::units::si::meters },{ -77.446964120666962 * boost::units::si::meters, 96.066301361657679 * boost::units::si::meters },{ -77.438964120694436 * boost::units::si::meters, 96.072301361709833 * boost::units::si::meters },{ -76.706964120676275 * boost::units::si::meters, 96.485301361884922 * boost::units::si::meters },{ -76.700964120682329 * boost::units::si::meters, 96.487301361747086 * boost::units::si::meters },{ -76.691964120662306 * boost::units::si::meters, 96.494301361963153 * boost::units::si::meters },{ -76.686964120715857 * boost::units::si::meters, 96.49730136198923 * boost::units::si::meters },{ -76.68396412068978 * boost::units::si::meters, 96.504301361739635 * boost::units::si::meters },{ -76.241964120708872 * boost::units::si::meters, 96.963301362004131 * boost::units::si::meters },{ -75.835964120691642 * boost::units::si::meters, 97.670301361940801 * boost::units::si::meters },{ -75.350964120705612 * boost::units::si::meters, 98.876301361713558 * boost::units::si::meters },{ -75.348964120668825 * boost::units::si::meters, 98.882301361765712 * boost::units::si::meters },{ -75.047964120691177 * boost::units::si::meters, 99.965301361866295 * boost::units::si::meters },{ -75.047964120691177 * boost::units::si::meters, 99.970301361754537 * boost::units::si::meters },{ -74.619964120676741 * boost::units::si::meters, 101.73330136202276 * boost::units::si::meters },{ -74.616964120708872 * boost::units::si::meters, 101.73930136160925 * boost::units::si::meters },{ -74.616964120708872 * boost::units::si::meters, 101.74330136179924 * boost::units::si::meters },{ -74.615964120661374 * boost::units::si::meters, 101.75330136157572 * boost::units::si::meters },{ -74.613964120682795 * boost::units::si::meters, 101.76230136165395 * boost::units::si::meters },{ -74.615964120661374 * boost::units::si::meters, 101.76930136187002 * boost::units::si::meters },{ -74.587964120670222 * boost::units::si::meters, 103.0903013618663 * boost::units::si::meters },{ -74.586964120680932 * boost::units::si::meters, 103.09230136172846 * boost::units::si::meters },{ -74.586964120680932 * boost::units::si::meters, 103.11130136158317 * boost::units::si::meters },{ -74.575964120682329 * boost::units::si::meters, 105.05930136190727 * boost::units::si::meters },{ -74.565964120673016 * boost::units::si::meters, 107.01230136165395 * boost::units::si::meters },{ -74.550964120717254 * boost::units::si::meters, 108.30730136157945 * boost::units::si::meters },{ -74.539964120660443 * boost::units::si::meters, 110.0403013615869 * boost::units::si::meters },{ -74.523964120715391 * boost::units::si::meters, 111.02330136159435 * boost::units::si::meters },{ -74.509964120690711 * boost::units::si::meters, 112.11430136160925 * boost::units::si::meters },{ -74.500964120670687 * boost::units::si::meters, 112.12930136173964 * boost::units::si::meters },{ -74.479964120662771 * boost::units::si::meters, 113.08830136200413 * boost::units::si::meters },{ -74.514964120695367 * boost::units::si::meters, 114.93230136157945 * boost::units::si::meters },{ -74.57296412071446 * boost::units::si::meters, 120.61830136179924 * boost::units::si::meters },{ -74.571964120666962 * boost::units::si::meters, 120.6203013616614 * boost::units::si::meters },{ -74.840964120696299 * boost::units::si::meters, 141.39830136159435 * boost::units::si::meters },{ -74.881964120664634 * boost::units::si::meters, 141.39530136156827 * boost::units::si::meters },{ -74.945964120677672 * boost::units::si::meters, 141.4543013619259 * boost::units::si::meters },{ -74.944964120688383 * boost::units::si::meters, 142.98930136160925 * boost::units::si::meters },{ -74.962964120670222 * boost::units::si::meters, 144.60530136199668 * boost::units::si::meters },{ -75.022964120667893 * boost::units::si::meters, 145.25530136190355 * boost::units::si::meters },{ -75.228964120673481 * boost::units::si::meters, 146.07630136189982 * boost::units::si::meters },{ -75.784964120713994 * boost::units::si::meters, 146.81130136176944 * boost::units::si::meters },{ -75.786964120692573 * boost::units::si::meters, 146.8133013616316 * boost::units::si::meters },{ -76.457964120665565 * boost::units::si::meters, 147.47930136183277 * boost::units::si::meters },{ -77.095964120700955 * boost::units::si::meters, 147.69530136184767 * boost::units::si::meters },{ -78.27896412066184 * boost::units::si::meters, 147.87730136187747 * boost::units::si::meters },{ -78.279964120709337 * boost::units::si::meters, 147.87730136187747 * boost::units::si::meters },{ -79.738964120682795 * boost::units::si::meters, 148.03430136200041 * boost::units::si::meters },{ -79.744964120676741 * boost::units::si::meters, 148.03730136202648 * boost::units::si::meters },{ -79.759964120690711 * boost::units::si::meters, 148.03730136202648 * boost::units::si::meters },{ -79.773964120715391 * boost::units::si::meters, 148.03930136188865 * boost::units::si::meters },{ -79.77896412066184 * boost::units::si::meters, 148.03730136202648 * boost::units::si::meters },{ -85.266964120673947 * boost::units::si::meters, 148.00530136190355 * boost::units::si::meters },{ -90.124964120681398 * boost::units::si::meters, 147.99330136179924 * boost::units::si::meters },{ -101.40196412068326 * boost::units::si::meters, 147.96030136197805 * boost::units::si::meters },{ -119.15796412067721 * boost::units::si::meters, 147.97330136178061 * boost::units::si::meters },{ -119.18696412071586 * boost::units::si::meters, 147.98430136172101 * boost::units::si::meters },{ -120.32396412070375 * boost::units::si::meters, 147.96630136156455 * boost::units::si::meters },{ -121.18896412069444 * boost::units::si::meters, 147.95130136189982 * boost::units::si::meters },{ -122.18796412070515 * boost::units::si::meters, 147.93530136160553 * boost::units::si::meters },{ -123.18696412071586 * boost::units::si::meters, 147.91830136161298 * boost::units::si::meters },{ -123.66596412070794 * boost::units::si::meters, 147.91030136169866 * boost::units::si::meters },{ -123.76596412068466 * boost::units::si::meters, 146.71430136170238 * boost::units::si::meters },{ -123.83296412066557 * boost::units::si::meters, 139.81430136179551 * boost::units::si::meters },{ -121.95707778737415 * boost::units::si::meters, 141.13007202837616 * boost::units::si::meters },{ -120.19732916232897 * boost::units::si::meters, 141.58186023682356 * boost::units::si::meters },{ -119.50396412069676 * boost::units::si::meters, 141.81630136165768 * boost::units::si::meters },{ -103.97096412070096 * boost::units::si::meters, 143.40030136192217 * boost::units::si::meters },{ -103.96796412067488 * boost::units::si::meters, 143.40130136162043 * boost::units::si::meters },{ -103.72396412066882 * boost::units::si::meters, 143.70030136173591 * boost::units::si::meters },{ -103.71896412066417 * boost::units::si::meters, 143.70630136178806 * boost::units::si::meters },{ -103.10396412067348 * boost::units::si::meters, 144.2613013619557 * boost::units::si::meters },{ -103.10296412068419 * boost::units::si::meters, 144.2613013619557 * boost::units::si::meters },{ -102.4879641206935 * boost::units::si::meters, 144.79230136191472 * boost::units::si::meters },{ -102.48596412071493 * boost::units::si::meters, 144.79430136177689 * boost::units::si::meters },{ -101.76096412068 * boost::units::si::meters, 145.34230136172846 * boost::units::si::meters },{ -101.75596412067534 * boost::units::si::meters, 145.34530136175454 * boost::units::si::meters },{ -100.71396412071772 * boost::units::si::meters, 145.95730136195198 * boost::units::si::meters },{ -100.70396412070841 * boost::units::si::meters, 145.96130136167631 * boost::units::si::meters },{ -100.32296412071446 * boost::units::si::meters, 146.08530136197805 * boost::units::si::meters },{ -100.31996412068838 * boost::units::si::meters, 146.08630136167631 * boost::units::si::meters },{ -99.749964120681398 * boost::units::si::meters, 146.2273013619706 * boost::units::si::meters },{ -99.741964120708872 * boost::units::si::meters, 146.22830136166885 * boost::units::si::meters },{ -98.950964120682329 * boost::units::si::meters, 146.28630136186257 * boost::units::si::meters },{ -98.93296412070049 * boost::units::si::meters, 146.28430136200041 * boost::units::si::meters },{ -98.360964120714925 * boost::units::si::meters, 146.11130136158317 * boost::units::si::meters },{ -79.787964120681863 * boost::units::si::meters, 146.13530136179179 * boost::units::si::meters },{ -79.729964120662771 * boost::units::si::meters, 146.0793013619259 * boost::units::si::meters },{ -79.577964120660909 * boost::units::si::meters, 142.55430136201903 * boost::units::si::meters },{ -77.605964120710269 * boost::units::si::meters, 141.21830136189237 * boost::units::si::meters },{ -77.582964120665565 * boost::units::si::meters, 141.17630136199296 * boost::units::si::meters },{ -77.105964120710269 * boost::units::si::meters, 121.06930136168376 * boost::units::si::meters },{ -77.105964120710269 * boost::units::si::meters, 121.06730136182159 * boost::units::si::meters },{ -77.235964120714925 * boost::units::si::meters, 113.0633013616316 * boost::units::si::meters },{ -77.234964120667428 * boost::units::si::meters, 113.06130136176944 * boost::units::si::meters },{ -77.256964120664634 * boost::units::si::meters, 105.50830136192963 * boost::units::si::meters },{ -77.316964120662306 * boost::units::si::meters, 105.449301361572 * boost::units::si::meters },{ -81.91796412068652 * boost::units::si::meters, 105.50830136192963 * boost::units::si::meters },{ -81.91896412067581 * boost::units::si::meters, 105.50830136192963 * boost::units::si::meters },{ -82.019964120700024 * boost::units::si::meters, 103.42230136180297 * boost::units::si::meters },{ -78.326191454019863 * boost::units::si::meters, 103.27319269487634 * boost::units::si::meters },{ -78.731748787336983 * boost::units::si::meters, 102.51086069503799 * boost::units::si::meters },{ -79.848197454004548 * boost::units::si::meters, 101.75863936170936 * boost::units::si::meters },{ -81.370752787333913 * boost::units::si::meters, 100.3044140287675 * boost::units::si::meters },{ -80.251964120659977 * boost::units::si::meters, 98.745301361661404 * boost::units::si::meters },{ -80.255964120675344 * boost::units::si::meters, 98.744301361963153 * boost::units::si::meters },{ -80.263964120706078 * boost::units::si::meters, 98.737301361747086 * boost::units::si::meters },{ -80.267964120663237 * boost::units::si::meters, 98.734301361721009 * boost::units::si::meters },{ -80.280964120698627 * boost::units::si::meters, 98.725301361642778 * boost::units::si::meters },{ -80.282964120677207 * boost::units::si::meters, 98.721301361918449 * boost::units::si::meters },{ -81.253449456591625 * boost::units::si::meters, 97.910997248720378 * boost::units::si::meters } })
        //, (polygon2{ { 22.701023126370274 * boost::units::si::meters, 148.6442247191444 * boost::units::si::meters },{ 18.437935879337601 * boost::units::si::meters, 148.60430136183277 * boost::units::si::meters },{ 2.057035879290197 * boost::units::si::meters, 148.47930136183277 * boost::units::si::meters },{ -60.465964120696299 * boost::units::si::meters, 148.19730136170983 * boost::units::si::meters },{ -60.468964120664168 * boost::units::si::meters, 148.19830136187375 * boost::units::si::meters },{ -60.482964120688848 * boost::units::si::meters, 148.19730136170983 * boost::units::si::meters },{ -60.503964120696764 * boost::units::si::meters, 148.19730136170983 * boost::units::si::meters },{ -60.505964120675344 * boost::units::si::meters, 148.19630136201158 * boost::units::si::meters },{ -61.949964120693039 * boost::units::si::meters, 148.0903013618663 * boost::units::si::meters },{ -61.955964120686986 * boost::units::si::meters, 148.0903013618663 * boost::units::si::meters },{ -63.04196412069723 * boost::units::si::meters, 147.97930136183277 * boost::units::si::meters },{ -63.694964120688383 * boost::units::si::meters, 147.8133013616316 * boost::units::si::meters },{ -64.194964120688383 * boost::units::si::meters, 147.50330136157572 * boost::units::si::meters },{ -64.91896412067581 * boost::units::si::meters, 146.73630136158317 * boost::units::si::meters },{ -65.547964120691177 * boost::units::si::meters, 145.87730136187747 * boost::units::si::meters },{ -65.960964120691642 * boost::units::si::meters, 144.93630136176944 * boost::units::si::meters },{ -65.962964120670222 * boost::units::si::meters, 144.92830136185512 * boost::units::si::meters },{ -66.032964120677207 * boost::units::si::meters, 144.23430136172101 * boost::units::si::meters },{ -66.032964120677207 * boost::units::si::meters, 144.22830136166885 * boost::units::si::meters },{ -66.050964120717254 * boost::units::si::meters, 143.33930136170238 * boost::units::si::meters },{ -66.050964120717254 * boost::units::si::meters, 143.33730136184022 * boost::units::si::meters },{ -66.049964120669756 * boost::units::si::meters, 141.53830136172473 * boost::units::si::meters },{ -66.0569641207112 * boost::units::si::meters, 140.30130136199296 * boost::units::si::meters },{ -66.0569641207112 * boost::units::si::meters, 140.30030136182904 * boost::units::si::meters },{ -66.047964120691177 * boost::units::si::meters, 139.29730136180297 * boost::units::si::meters },{ -66.038964120671153 * boost::units::si::meters, 137.30130136199296 * boost::units::si::meters },{ -66.030964120698627 * boost::units::si::meters, 135.30430136201903 * boost::units::si::meters },{ -66.02696412068326 * boost::units::si::meters, 133.97430136194453 * boost::units::si::meters },{ -66.019964120700024 * boost::units::si::meters, 132.20030136173591 * boost::units::si::meters },{ -66.017964120663237 * boost::units::si::meters, 131.18730136193335 * boost::units::si::meters },{ -66.014964120695367 * boost::units::si::meters, 130.03030136181042 * boost::units::si::meters },{ -66.010964120680001 * boost::units::si::meters, 128.70630136178806 * boost::units::si::meters },{ -66.004964120686054 * boost::units::si::meters, 127.19430136168376 * boost::units::si::meters },{ -65.987964120693505 * boost::units::si::meters, 125.53330136183649 * boost::units::si::meters },{ -65.970964120700955 * boost::units::si::meters, 123.87130136182532 * boost::units::si::meters },{ -65.960964120691642 * boost::units::si::meters, 122.76530136168003 * boost::units::si::meters },{ -65.945964120677672 * boost::units::si::meters, 121.28830136172473 * boost::units::si::meters },{ -65.930964120663702 * boost::units::si::meters, 118.76630136184394 * boost::units::si::meters },{ -65.897964120667893 * boost::units::si::meters, 108.49930136185139 * boost::units::si::meters },{ -65.897964120667893 * boost::units::si::meters, 108.44630136201158 * boost::units::si::meters },{ -65.858964120678138 * boost::units::si::meters, 107.03530136169866 * boost::units::si::meters },{ -65.858964120678138 * boost::units::si::meters, 107.03430136200041 * boost::units::si::meters },{ -65.845964120700955 * boost::units::si::meters, 105.39130136184394 * boost::units::si::meters },{ -65.812964120705146 * boost::units::si::meters, 104.27130136173218 * boost::units::si::meters },{ -65.812964120705146 * boost::units::si::meters, 104.26730136200786 * boost::units::si::meters },{ -65.844964120711666 * boost::units::si::meters, 103.58330136165023 * boost::units::si::meters },{ -65.845964120700955 * boost::units::si::meters, 103.57730136159807 * boost::units::si::meters },{ -66.097964120679535 * boost::units::si::meters, 102.11830136179924 * boost::units::si::meters },{ -66.105964120710269 * boost::units::si::meters, 102.1023013619706 * boost::units::si::meters },{ -66.105964120710269 * boost::units::si::meters, 102.09930136194453 * boost::units::si::meters },{ -66.083964120713063 * boost::units::si::meters, 102.09630136191845 * boost::units::si::meters },{ -64.060964120668359 * boost::units::si::meters, 101.23730136174709 * boost::units::si::meters },{ -64.050964120717254 * boost::units::si::meters, 101.23130136169493 * boost::units::si::meters },{ -62.847970449423883 * boost::units::si::meters, 100.34976993687451 * boost::units::si::meters },{ -55.39924044336658 * boost::units::si::meters, 102.66576161934063 * boost::units::si::meters },{ -56.109964120667428 * boost::units::si::meters, 104.60430136183277 * boost::units::si::meters },{ -56.112964120693505 * boost::units::si::meters, 104.62130136182532 * boost::units::si::meters },{ -56.191964120662306 * boost::units::si::meters, 108.58730136184022 * boost::units::si::meters },{ -62.440964120673016 * boost::units::si::meters, 108.51730136200786 * boost::units::si::meters },{ -62.499964120681398 * boost::units::si::meters, 108.57530136173591 * boost::units::si::meters },{ -62.503964120696764 * boost::units::si::meters, 111.62130136182532 * boost::units::si::meters },{ -62.44796412071446 * boost::units::si::meters, 111.68030136171728 * boost::units::si::meters },{ -59.846964120690245 * boost::units::si::meters, 111.7563013616018 * boost::units::si::meters },{ -59.80896412068978 * boost::units::si::meters, 114.71130136167631 * boost::units::si::meters },{ -59.80896412068978 * boost::units::si::meters, 114.71330136200413 * boost::units::si::meters },{ -59.753964120696764 * boost::units::si::meters, 115.98530136188492 * boost::units::si::meters },{ -61.958964120713063 * boost::units::si::meters, 115.99030136177316 * boost::units::si::meters },{ -61.997964120702818 * boost::units::si::meters, 116.00830136192963 * boost::units::si::meters },{ -63.169964120665099 * boost::units::si::meters, 117.36130136158317 * boost::units::si::meters },{ -63.1819641207112 * boost::units::si::meters, 117.38530136179179 * boost::units::si::meters },{ -63.44796412071446 * boost::units::si::meters, 118.89330136170611 * boost::units::si::meters },{ -63.448964120703749 * boost::units::si::meters, 118.90230136178434 * boost::units::si::meters },{ -63.434964120679069 * boost::units::si::meters, 127.70230136159807 * boost::units::si::meters },{ -63.379964120686054 * boost::units::si::meters, 127.76030136179179 * boost::units::si::meters },{ -61.977964120684192 * boost::units::si::meters, 127.82630136189982 * boost::units::si::meters },{ -61.944964120688383 * boost::units::si::meters, 130.16230136202648 * boost::units::si::meters },{ -61.888964120706078 * boost::units::si::meters, 130.22030136175454 * boost::units::si::meters },{ -60.053964120685123 * boost::units::si::meters, 130.28130136197433 * boost::units::si::meters },{ -60.039964120660443 * boost::units::si::meters, 134.35430136183277 * boost::units::si::meters },{ -60.04096412070794 * boost::units::si::meters, 134.35530136199668 * boost::units::si::meters },{ -62.619964120676741 * boost::units::si::meters, 134.26730136200786 * boost::units::si::meters },{ -62.680964120663702 * boost::units::si::meters, 134.324301361572 * boost::units::si::meters },{ -62.816964120662306 * boost::units::si::meters, 141.58830136200413 * boost::units::si::meters },{ -62.758964120701421 * boost::units::si::meters, 141.64830136159435 * boost::units::si::meters },{ -60.465964120696299 * boost::units::si::meters, 141.69530136184767 * boost::units::si::meters },{ -60.462964120670222 * boost::units::si::meters, 141.69530136184767 * boost::units::si::meters },{ -55.813964120694436 * boost::units::si::meters, 141.49130136193708 * boost::units::si::meters },{ -55.767964120663237 * boost::units::si::meters, 146.27530136192217 * boost::units::si::meters },{ -55.709964120702352 * boost::units::si::meters, 146.33330136165023 * boost::units::si::meters },{ -39.65096412069397 * boost::units::si::meters, 146.42330136196688 * boost::units::si::meters },{ 0.75180854595964774 * boost::units::si::meters, 144.11540202843025 * boost::units::si::meters },{ 7.1890558793093078 * boost::units::si::meters, 141.00939002865925 * boost::units::si::meters },{ 11.930407879292034 * boost::units::si::meters, 139.42538402834907 * boost::units::si::meters },{ 22.31368148420006 * boost::units::si::meters, 137.0862382221967 * boost::units::si::meters } })
    };

    auto bounds = get_bounds(areas[0], cmp);
    for (std::size_t i = 1; i < areas.size(); ++i) {
        bounds = update_bound(bounds, get_bounds(areas[i], cmp));
    }

    point2 ll = { std::get<e_xmin>(bounds), std::get<e_ymin>(bounds) };
    point2 ur = { std::get<e_xmax>(bounds), std::get<e_ymax>(bounds) };

    auto outer = get_outer_polygon(ll, ur, 100.0 * units::si::meters);

    bounds = get_bounds(outer, cmp);
    ll = { std::get<e_xmin>(bounds), std::get<e_ymin>(bounds) };
    ur = { std::get<e_xmax>(bounds), std::get<e_ymax>(bounds) };

    std::vector<polygon2> holes;
    boost::copy(areas | transformed([](const polygon2& pgon) { return reverse(pgon); }), std::back_inserter(holes));

    auto poly = polygon_with_holes2{outer, holes};
    auto segs = polygon_with_holes_as_segment_range<segment2>(poly);
	auto holeSegs = polygon_as_segment_range<segment2>(areas[0]);
    auto partitionPolicy = partition_policies::autopartition_policy();
    auto holebsp = solid_bsp2{ holeSegs, partitionPolicy, cmp };

	stk::units::length granularity = 4.0 * units::si::meters;
	stk::units::length distSaturation = 1.0 * units::si::meters;
	double attractionStrength = 0.1;
	std::vector<point2> spoints = generate_fine_steiner_points(poly, granularity, holebsp);

    auto mesh = generate_weighted_mesh(poly, spoints, triangle_area_distance_weight_policy(&holebsp, distSaturation, attractionStrength));

    std::vector<polygon2> trigs;
	auto const& adjMatrx = mesh.get_adjacency_matrix();
    for(auto i = 0; i < mesh.get_number_triangles(); ++i)
    {
        auto& trig = mesh.get_triangle_vertices(i);
        trigs.push_back(polygon2({trig[0], trig[1], trig[2]}));
    }

    std::size_t idx;
    auto bsp = solid_bsp2{ segs, partitionPolicy, cmp };
    auto d = bsp.get_min_distance_to_solid(ll, idx, cmp);
    auto cp = point_segment_closest_point(ll, segs[idx]);
    EXPECT_EQ(0, idx);
    EXPECT_TRUE(cmp.equals(d, 0.0 * units::si::meters));
    EXPECT_TRUE(numeric_sequence_equals(ll, cp, cmp));

    d = bsp.get_min_distance_to_solid(ur, idx, cmp);
    cp = point_segment_closest_point(ur, segs[idx]);
    EXPECT_EQ(1, idx);
    EXPECT_TRUE(cmp.equals(d, 0.0 * units::si::meters));
    EXPECT_TRUE(numeric_sequence_equals(ur, cp, cmp));

    auto p = areas[0][0];
    d = bsp.get_min_distance_to_solid(p, idx, cmp);
    cp = point_segment_closest_point(p, segs[idx]);
    EXPECT_EQ(25, idx);
    EXPECT_TRUE(cmp.equals(d, 0.0 * units::si::meters));
    EXPECT_TRUE(numeric_sequence_equals(p, cp, cmp));

	//std::vector<circle2> rs;
    //random_real_generator<> rnd;

	//for (auto i = 0; i < 500; ++i)
	//	rs.emplace_back(mesh.get_random_position(rnd(), rnd(), rnd()), 0.1 * units::si::meters);


    //EXPECT_EQ(25, idx);
}

bool in_diametral_circle(stk::point2 const& p, stk::point2 const& o, stk::point2 const& d)
{
	using namespace geometrix;
	//! If the angle between OP and DP is obtuse, then P is inside the diametral circle of OD.
	//! two vectors have obtuse angle if dot product is negative.
	auto dp = dot_product(o - p, d - p);
	return dp < constants::zero<decltype(dp)>();
}

TEST(weighted_mesh_test_suite, diametral_lens)
{
	using namespace stk;

	EXPECT_TRUE(in_diametral_circle({0.0* units::si::meters, 0.0* units::si::meters}, {0.5 * units::si::meters, -1.0 * units::si::meters}, {0.5 * units::si::meters, 1.0 * units::si::meters}));
	EXPECT_TRUE(in_diametral_circle({-0.4* units::si::meters, 0.0* units::si::meters}, {0.5 * units::si::meters, -1.0 * units::si::meters}, {0.5 * units::si::meters, 1.0 * units::si::meters}));
	EXPECT_FALSE(in_diametral_circle({-1.0* units::si::meters, 0.0* units::si::meters}, {0.5 * units::si::meters, -1.0 * units::si::meters}, {0.5 * units::si::meters, 1.0 * units::si::meters}));
	EXPECT_FALSE(in_diametral_circle({1.5* units::si::meters, 0.0* units::si::meters}, {0.5 * units::si::meters, -1.0 * units::si::meters}, {0.5 * units::si::meters, 1.0 * units::si::meters}));
}

bool in_diametral_lens(stk::units::angle const& theta, stk::point2 const& o, stk::point2 const& d, const stk::point2& p)
{
	using namespace geometrix;
	using std::cos;

	stk::vector2 op = o - p;
	stk::vector2 dp = d - p;
	auto dt = dot_product(op, dp);
	auto cos_theta = cos(theta);
	auto v2_cos_theta2_1 = 2.0 * cos_theta * cos_theta - 1.0;
	return (dt * dt) >= (v2_cos_theta2_1 * v2_cos_theta2_1 * magnitude_sqrd(op) * magnitude_sqrd(dp));
}
