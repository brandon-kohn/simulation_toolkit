#pragma once

#include <stk/geometry/primitive/polygon.hpp>
#include <stk/geometry/space_partition/bsp_tree.hpp>
#include <stk/geometry/space_partition/rtree_triangle_cache.ipp>
#include <stk/geometry/space_partition/poly2tri_mesh.hpp>
#include <geometrix/algorithm/mesh_2d.hpp>
#include <geometrix/utility/assert.hpp>
#include <geometrix/algorithm/hyperplane_partition_policies.hpp>
#include <random>
#include <vector>

namespace stk {

    class biased_position_generator 
    {
        struct triangle_area_distance_weight_policy
        {
            using weight_type = double;
            using normalized_type = double;

            triangle_area_distance_weight_policy(const solid_bsp2* pBSP, const stk::units::length& distanceSaturation, double attractionStrength)
                : pBSP(pBSP)
                , distanceSaturation(distanceSaturation * distanceSaturation)
                , attractionStrength(attractionStrength)
            {}

            template <typename Triangle>
            weight_type get_weight(Triangle&& trig) const
            {
                using namespace geometrix;
                using namespace stk;
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

            const solid_bsp2* pBSP{ nullptr };
            stk::units::area distanceSaturation{ 1.0 * stk::units::si::square_meters };
            double attractionStrength{ 1.0 };

        };

        using mesh_type = geometrix::mesh_2d<stk::units::length, geometrix::mesh_traits<stk::rtree_triangle_cache, triangle_area_distance_weight_policy>>;

    public:

        //! Generates points within the simple polygonal boundary with a bias towards the geometry in attractiveSegments. 
        //! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
        //! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
        //! Attraction factor is a quantity specifying the strength of the attraction.
        template <typename Polygon, typename Segments>
        biased_position_generator(const Polygon& boundary, const Segments& attractiveSegments, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor)
        {
            using namespace stk;
            using namespace geometrix;
            struct identity_extractor
            {
                segment2 const& operator()(const segment2& a) const
                {
                    return a;
                }
            };
            auto partitionPolicy = partition_policies::scored_selector_policy<identity_extractor, stk::tolerance_policy>(identity_extractor());
            auto bsp = solid_bsp2{attractiveSegments, partitionPolicy, make_tolerance_policy()};
            std::vector<point2> spoints = generate_fine_steiner_points(boundary, granularity, bsp);
            m_mesh = generate_weighted_mesh(boundary, spoints, triangle_area_distance_weight_policy(&bsp, distanceSaturation, attractionFactor));
            m_mesh->get_adjacency_matrix();//! cache the adjacency matrix.
        }

        //! Generates points within the simple polygonal boundary with holes with a bias towards the geometry in attractiveSegments. 
        //! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
        //! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
        //! Attraction factor is a quantity specifying the strength of the attraction.
        template <typename Polygon, typename Holes, typename Segments>
        biased_position_generator(const Polygon& boundary, const Holes& holes, const Segments& attractiveSegments, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor)
        {
            using namespace stk;
            using namespace geometrix;
            struct identity_extractor
            {
                segment2 const& operator()(const segment2& a) const
                {
                    return a;
                }
            };
            auto partitionPolicy = partition_policies::scored_selector_policy<identity_extractor, stk::tolerance_policy>(identity_extractor());
            auto bsp = solid_bsp2{attractiveSegments, partitionPolicy, make_tolerance_policy()};
            std::vector<point2> spoints = generate_fine_steiner_points(boundary, granularity, bsp);
            m_mesh = generate_weighted_mesh(boundary, holes, spoints, triangle_area_distance_weight_policy(&bsp, distanceSaturation, attractionFactor));
            m_mesh->get_adjacency_matrix();//! cache the adjacency matrix.
        }

        //! Construct a generator which uses a reference to an external BSP containing attractive geometry. 
        //! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
        //! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
        //! Attraction factor is a quantity specifying the strength of the attraction.
        template <typename Polygon>
        biased_position_generator(const Polygon& boundary, const solid_bsp2& attractiveBSP, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor)
        {
            using namespace stk;
            using namespace geometrix;
            std::vector<point2> spoints = generate_fine_steiner_points(boundary, granularity, attractiveBSP);
            m_mesh = generate_weighted_mesh(boundary, spoints, triangle_area_distance_weight_policy(&attractiveBSP, distanceSaturation, attractionFactor));
            m_mesh->get_adjacency_matrix();//! cache the adjacency matrix.
        }

        //! Generate a random position. random0, random1, and random2 should be uniformly distributed random values in the range of [0.0, 1.0].
        template <typename Point>
        Point get_random_position(double random0, double random1, double random2) 
        {
            return geometrix::construct<Point>(m_mesh->get_random_position(random0, random1, random2));
        }
        
        template <typename Point, typename Generator>
        Point get_random_position(Generator& gen)
        {
            std::uniform_real_distribution<> U;
            return geometrix::construct<Point>(m_mesh->get_random_position(U(gen), U(gen), U(gen)));
        }

    private:

        template <typename Polygon>
        std::vector<stk::point2> generate_fine_steiner_points(const Polygon& pgon, const stk::units::length& cell, const solid_bsp2& bsp)
        {
            using namespace geometrix;
            using namespace stk;

            std::set<point2> results;
            auto cmp = make_tolerance_policy();
            auto obounds = get_bounds(pgon, cmp);
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

        template <typename Polygon>
        std::unique_ptr<mesh_type> generate_weighted_mesh(const Polygon& polygon, std::vector<stk::point2>& steinerPoints, const triangle_area_distance_weight_policy& weightPolicy)
        {
            using namespace stk;
            using namespace geometrix;
            if (polygon.empty() || !is_polygon_simple(polygon, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");

            std::vector<p2t::Point*> polygon_, memory;
            STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

            for( const auto& p : polygon )
            {
                polygon_.push_back( new p2t::Point( get<0>( p ).value(), get<1>( p ).value() ) );
                memory.push_back( polygon_.back() );
            }

            p2t::CDT cdt( polygon_ );

            for (const auto& p : steinerPoints) 
            {
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
            for( const auto& item : indices )
                geometrix::assign( points[item.second], item.first->x * units::si::meters, item.first->y * units::si::meters);

            for( auto* triangle : triangles )
            {
                for( int i = 0; i < 3; ++i )
                {
                    auto* p = triangle->GetPoint( i );
                    auto it = indices.find( p );
                    GEOMETRIX_ASSERT( it != indices.end() );
                    iArray.push_back( it->second );
                }
            }

            return boost::make_unique<mesh_type>(points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder(), weightPolicy);
        }

        template <typename Polygon>
        std::unique_ptr<mesh_type> generate_weighted_mesh(const Polygon& polygon, const std::vector<Polygon>& holes, std::vector<stk::point2>& steinerPoints, const triangle_area_distance_weight_policy& weightPolicy)
        {
            using namespace stk;
            using namespace geometrix;
            if (polygon.empty() || !is_polygon_simple(polygon, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");

            std::vector<p2t::Point*> polygon_, memory;
            STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

            for( const auto& p : polygon )
            {
                polygon_.push_back( new p2t::Point( get<0>( p ).value(), get<1>( p ).value() ) );
                memory.push_back( polygon_.back() );
            }

            p2t::CDT cdt( polygon_ );
		
            for( const auto& hole : holes )
            {
                if (hole.empty() || !is_polygon_simple(hole, make_tolerance_policy()))
                    throw std::invalid_argument("polygon not simple");

                std::vector<p2t::Point*> hole_;
                for( const auto& p : hole )	
                {
                    auto p_ = new p2t::Point( get<0>( p ).value(), get<1>( p ).value() );
                    hole_.push_back( p_ );
                    memory.push_back( p_ );
                }

                cdt.AddHole( hole_ );
            }
		
            for (const auto& p : steinerPoints) 
            {
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
            for( const auto& item : indices )
                geometrix::assign( points[item.second], item.first->x * units::si::meters, item.first->y * units::si::meters);

            for( auto* triangle : triangles )
            {
                for( int i = 0; i < 3; ++i )
                {
                    auto* p = triangle->GetPoint( i );
                    auto it = indices.find( p );
                    GEOMETRIX_ASSERT( it != indices.end() );
                    iArray.push_back( it->second );
                }
            }

            return boost::make_unique<mesh_type>(points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder(), weightPolicy);
        }

        std::unique_ptr<mesh_type> m_mesh;
    };
}//! namespace stk;

