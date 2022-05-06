#pragma once

#include <stk/geometry/primitive/polygon.hpp>
#include <stk/geometry/space_partition/bsp_tree.hpp>
#include <stk/geometry/space_partition/rtree_triangle_cache.ipp>
#include <stk/geometry/space_partition/poly2tri_mesh.hpp>

#include <geometrix/algorithm/intersection/polyline_polyline_intersection.hpp>
#include <geometrix/algorithm/point_sequence/is_polygon_simple.hpp>
#include <geometrix/algorithm/mesh_2d.hpp>
#include <geometrix/utility/assert.hpp>
#include <geometrix/algorithm/hyperplane_partition_policies.hpp>

#include <boost/range/numeric.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/optional.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <geometrix/arithmetic/math_kernel.hpp>
#include <stk/math/math.hpp>

#include <vector>
#include <mutex>
#include <exception>

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

                auto area = get_area(std::forward<Triangle>(trig));
                std::size_t idx;
                auto distanceSqrd = pBSP->get_min_distance_sqrd_to_solid(get_centroid(trig), idx, make_tolerance_policy());
                auto d2 = std::max(distanceSqrd.value(), distanceSaturation.value());
                weight_type f = area.value() * stk::exp(-attractionStrength * d2);
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

        using mesh_type = geometrix::mesh_2d<stk::units::length, geometrix::mesh_traits<stk::rtree_triangle_cache>>;

    public:

        //! Generates points within the simple polygonal boundary with a bias towards the geometry in attractiveSegments. 
        //! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
        //! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
        //! Attraction factor is a quantity specifying the strength of the attraction.
        template <typename Polygon, typename Segments, typename Executor>
        biased_position_generator(const Polygon& boundary, const Segments& attractiveSegments, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor, Executor&& exec)
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
            m_mesh = generate_weighted_mesh(boundary, granularity, bsp, triangle_area_distance_weight_policy(&bsp, distanceSaturation, attractionFactor), std::forward<Executor>(exec));
            m_mesh->get_adjacency_matrix();//! cache the adjacency matrix.
        }

        //! Generates points within the simple polygonal boundary with holes with a bias towards the geometry in attractiveSegments. 
        //! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
        //! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
        //! Attraction factor is a quantity specifying the strength of the attraction.
        template <typename Polygon, typename Holes, typename Segments, typename Executor>
        biased_position_generator(const Polygon& boundary, const Holes& holes, const Segments& attractiveSegments, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor, Executor&& exec)
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
            m_mesh = generate_weighted_mesh(boundary, holes, granularity, bsp, triangle_area_distance_weight_policy(&bsp, distanceSaturation, attractionFactor), std::forward<Executor>(exec));
            m_mesh->get_adjacency_matrix();//! cache the adjacency matrix.
        }

        //! Construct a generator which uses a reference to an external BSP containing attractive geometry. 
        //! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
        //! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
        //! Attraction factor is a quantity specifying the strength of the attraction.
        template <typename Polygon, typename Executor>
        biased_position_generator(const Polygon& boundary, const solid_bsp2& attractiveBSP, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor, Executor&& exec)
        {
            using namespace stk;
            using namespace geometrix;
            m_mesh = generate_weighted_mesh(boundary, granularity, attractiveBSP, triangle_area_distance_weight_policy(&attractiveBSP, distanceSaturation, attractionFactor), std::forward<Executor>(exec));
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
            boost::random::uniform_real_distribution<> U;
            return geometrix::construct<Point>(m_mesh->get_random_position(U(gen), U(gen), U(gen)));
        }

		mesh_type const& get_mesh() const
		{
			return *m_mesh;
		}

    private:

		template <typename NumberComparisonPolicy>
		typename geometrix::bounds_tuple<point2>::type bounds(const polygon2& pgon, const NumberComparisonPolicy& compare)
		{
			return geometrix::get_bounds(pgon, compare);
		}

		template <typename NumberComparisonPolicy>
		typename geometrix::bounds_tuple<point2>::type bounds(const polygon_with_holes2& pgon, const NumberComparisonPolicy& compare)
		{
			return geometrix::get_bounds(pgon.get_outer(), compare);
		}

        template <typename Polygon>
        std::vector<stk::point2> generate_fine_steiner_points(const Polygon& pgon, const stk::units::length& cell, const solid_bsp2& bsp)
        {
            using namespace geometrix;
            using namespace stk;

            std::set<point2> results;
            auto cmp = make_tolerance_policy();
            auto obounds = bounds(pgon, cmp);
            auto grid = grid_traits<stk::units::length>(obounds, cell);
			auto mesh = generate_mesh(pgon);

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

        template <typename Polygon, typename Executor>
        std::unique_ptr<mesh_type> generate_weighted_mesh(const Polygon& polygon, const stk::units::length& granularity, const solid_bsp2& bsp, const triangle_area_distance_weight_policy& weightPolicy, Executor&& /*exec*/)
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

            std::vector<point2> steinerPoints = generate_fine_steiner_points(polygon, granularity, bsp);
			std::vector<p2t::Point*> steinerPoints_;
			for (const auto& p : steinerPoints) 
			{
				auto p_ = new p2t::Point(get<0>(p).value(), get<1>(p).value());
				memory.push_back(p_);
				steinerPoints_.push_back(memory.back());
			}

			do 
			{
				try 
				{
					p2t::CDT cdt( polygon_ );

					for (const auto& p : steinerPoints_) 
						cdt.AddPoint(p);

					std::map<p2t::Point*, std::size_t> indices;
					auto& cdtPoints = cdt.GetPoints();
					for( std::size_t i = 0; i < cdtPoints.size(); ++i )
						indices[cdtPoints[i]] = i;

					cdt.Triangulate();

					std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();
					std::vector<std::size_t> iArray;
					polygon2 points( indices.size() );
					for(const auto& item : indices)
						geometrix::assign( points[item.second], item.first->x * units::si::meters, item.first->y * units::si::meters);

					for(auto* triangle : triangles)
					{
						for(int i = 0; i < 3; ++i)
						{
							auto* p = triangle->GetPoint(i);
							auto it = indices.find(p);
							GEOMETRIX_ASSERT(it != indices.end());
							iArray.push_back(it->second);
						}
					}

					return boost::make_unique<mesh_type>(points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder(), weightPolicy);
				}
				catch (p2t::collinear_points_exception const & e) 
				{
			        auto cmp = make_tolerance_policy();
					auto is_bad_point = [&e, &cmp](const p2t::Point* p)
					{
						return (cmp.equals(p->x, e.a.x) && cmp.equals(p->y, e.a.y))
							|| (cmp.equals(p->x, e.b.x) && cmp.equals(p->y, e.b.y))
							|| (cmp.equals(p->x, e.c.x) && cmp.equals(p->y, e.c.y));
					};
					auto oldSize = steinerPoints_.size();
					boost::remove_erase_if(steinerPoints_, is_bad_point);
					if (steinerPoints_.size() == oldSize)
						throw e;
				}
			} while (true);
        }

        template <typename Polygon, typename Executor>
        std::unique_ptr<mesh_type> generate_weighted_mesh(const Polygon& polygon, const std::vector<Polygon>& holes, const stk::units::length& granularity, const solid_bsp2& bsp, const triangle_area_distance_weight_policy& weightPolicy, Executor&& /*exec*/)
        {
            using namespace stk;
            using namespace geometrix;
            if (polygon.empty() || !is_polygon_simple(polygon, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");

            std::vector<p2t::Point*> polygon_, memory;
			std::vector<std::vector<p2t::Point*>> holes_;
            STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

			auto cmp = make_tolerance_policy();
            for(const auto& p : polygon)
            {
                polygon_.push_back( new p2t::Point( get<0>( p ).value(), get<1>( p ).value() ) );
                memory.push_back( polygon_.back() );
            }
            
			for(const auto& hole : holes)
            {
                if (hole.empty() || !is_polygon_simple(hole, make_tolerance_policy()))
                    throw std::invalid_argument("polygon not simple");

				holes_.emplace_back();
                for(const auto& p : hole)	
                {
                    auto p_ = new p2t::Point( get<0>( p ).value(), get<1>( p ).value() );
                    holes_.back().push_back(p_);
                    memory.push_back(p_);
                }
            }
		
            std::vector<point2> steinerPoints = generate_fine_steiner_points(polygon, granularity, bsp);

			std::vector<p2t::Point*> steinerPoints_;
			for (const auto& p : steinerPoints) 
			{
				auto p_ = new p2t::Point(get<0>(p).value(), get<1>(p).value());
				memory.push_back(p_);
				steinerPoints_.push_back(p_);
			}

			do
			{
				try 
				{
					p2t::CDT cdt(polygon_);

					for (const auto& hole_ : holes_)
						cdt.AddHole(hole_);

					for (const auto& p : steinerPoints_)
						cdt.AddPoint(p);

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

					return boost::make_unique<mesh_type>(points, iArray, cmp, stk::rtree_triangle_cache_builder(), weightPolicy);
				} 
				catch (p2t::collinear_points_exception const & e) 
				{
					auto is_bad_point = [&e, &cmp](const p2t::Point* p)
					{
						return (cmp.equals(p->x, e.a.x) && cmp.equals(p->y, e.a.y))
							|| (cmp.equals(p->x, e.b.x) && cmp.equals(p->y, e.b.y))
							|| (cmp.equals(p->x, e.c.x) && cmp.equals(p->y, e.c.y));
					};
					auto oldSize = steinerPoints_.size();
					boost::remove_erase_if(steinerPoints_, is_bad_point);
					if (steinerPoints_.size() == oldSize)
						throw e;
				}
			}
			while (true);
        }

		template <typename Executor>
		std::unique_ptr<mesh_type> generate_weighted_mesh(const std::vector<stk::polygon_with_holes2>& polygons, const stk::units::length& granularity, const solid_bsp2& bsp, const triangle_area_distance_weight_policy& weightPolicy, Executor&& exec)
		{
			using namespace geometrix;
			using namespace stk;
			std::mutex memmutx;
			std::vector<p2t::Point*> memory;
			std::map<point2, std::size_t> allIndices;
			std::vector<point2> pArray;
			std::vector<std::size_t> tArray;
			auto cmp = make_tolerance_policy();
			auto getIndex = [&](const point2& p) -> std::size_t
			{
				auto it = allIndices.lower_bound(p);
				if (map_lower_bound_contains(p, allIndices, it))
					return it->second;

				auto newIndex = allIndices.size();
				pArray.push_back(p);
				allIndices.insert(it, std::make_pair(p, newIndex));
				return newIndex;
			};
			
			std::mutex mtx;
			auto addTriangle = [&](const point2& p0, const point2& p1, const point2& p2)
			{
				auto lk = std::unique_lock<std::mutex>{ mtx };
				auto i0 = getIndex(p0);
				auto i1 = getIndex(p1);
				auto i2 = getIndex(p2);
				tArray.push_back(i0);
				tArray.push_back(i1);
				tArray.push_back(i2);
			};

			STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

			auto work = [&](const polygon_with_holes2& polygon)
			{
				std::vector<p2t::Point*> polygon_;
				if (polygon.get_outer().empty() || !is_polygon_simple(polygon.get_outer(), make_tolerance_policy()))
					throw std::invalid_argument("polygon not simple");

				for (const auto& hole : polygon.get_holes())
				{
					if (!is_polygon_simple(hole, make_tolerance_policy()))
						throw std::invalid_argument("polygon not simple");
				}

				for (const auto& p : polygon.get_outer())
				{
					auto lk = std::unique_lock<std::mutex>{ memmutx };
					memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
					polygon_.push_back(memory.back());
				}

				//! Add the holes
				std::vector<std::vector<p2t::Point*>> holes_;
				for (const auto& hole : polygon.get_holes()) 
				{
					holes_.emplace_back();
					for (const auto& p : hole) 
					{
						auto lk = std::unique_lock<std::mutex>{ memmutx };
						memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
						holes_.back().push_back(memory.back());
					}
				}

				std::vector<point2> steinerPoints = generate_fine_steiner_points(polygon, granularity, bsp);
				std::vector<p2t::Point*> steinerPoints_;
				for (const auto& p : steinerPoints)
				{
					auto lk = std::unique_lock<std::mutex>{ memmutx };
					memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
					steinerPoints_.push_back(memory.back());
				}

				do
				{
					try 
					{
						p2t::CDT cdt(polygon_);

						for (const auto& hole_ : holes_)
							cdt.AddHole(hole_);

						for (const auto& p : steinerPoints_)
							cdt.AddPoint(p);

						cdt.Triangulate();

						std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();
						for (auto* triangle : triangles) 
						{
							auto* p0_ = triangle->GetPoint(0);
							auto* p1_ = triangle->GetPoint(1);
							auto* p2_ = triangle->GetPoint(2);
							auto p0 = point2{p0_->x * units::meters, p0_->y * units::meters};
							auto p1 = point2{p1_->x * units::meters, p1_->y * units::meters};
							auto p2 = point2{p2_->x * units::meters, p2_->y * units::meters};
							addTriangle(p0, p1, p2);
						}

						return true;
					} 
					catch (p2t::collinear_points_exception const & e) 
					{
						auto is_bad_point = [&e, &cmp](const p2t::Point* p)
						{
							return (cmp.equals(p->x, e.a.x) && cmp.equals(p->y, e.a.y))
								|| (cmp.equals(p->x, e.b.x) && cmp.equals(p->y, e.b.y))
								|| (cmp.equals(p->x, e.c.x) && cmp.equals(p->y, e.c.y));
						};
						auto oldSize = steinerPoints_.size();
						boost::remove_erase_if(steinerPoints_, is_bad_point);
						if (steinerPoints_.size() == oldSize) 
							return false;//Silently skip the polygon...
					}
				}
				while (true);
			};

			exec.for_each(polygons, work);

            return boost::make_unique<mesh_type>(pArray, tArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder(), weightPolicy);
		}
        std::unique_ptr<mesh_type> m_mesh;
    };

	namespace detail {
		inline void add_segments(const polygon2& pgon, std::vector<segment2>& segs)
		{
			auto size = pgon.size();
			for (std::size_t i = 0, j = 1; i < size; ++i, j = (j + 1) % size)
				segs.emplace_back(pgon[i], pgon[j]);
		}

		inline void add_segments(const polygon_with_holes2& pgon, std::vector<segment2>& segs)
		{
			add_segments(pgon.get_outer(), segs);
			for (const auto& h : pgon.get_holes())
				add_segments(h, segs);
		}

		template <typename Polygons>
		inline std::vector<segment2> polygon_collection_as_segment_range(const Polygons& pgons)
		{
			using namespace stk::detail;
			using namespace geometrix;
			std::vector<segment2> segments;
			for (const auto& p : pgons)
				add_segments(p, segments);

			return segments;
		}
	}//! namespace detail;

	class biased_position_grid
	{
	public:

		struct weight_policy
		{
			weight_policy(const stk::units::length& d, double attractionStrength)
				: distanceSaturation(d*d)
				, attractionStrength(attractionStrength)
			{}

			double get_weight(stk::units::area const& distanceSqrd) const
			{
				using namespace geometrix;
				using namespace stk;

				auto d2 = std::max(distanceSqrd.value(), distanceSaturation.value());
				double f = stk::exp(-attractionStrength * d2);
				return f;
			}

			stk::units::area distanceSaturation{ 1.0 * stk::units::si::square_meters };
			double attractionStrength{ 1.0 };
		};

		//! Construct a generator which uses a reference to an external BSP containing attractive geometry. 
		//! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
		//! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
		//! Attraction factor is a quantity specifying the strength of the attraction.
		template <typename NumberComparisonPolicy, typename Executor>
		biased_position_grid(const std::vector<polygon_with_holes2>& boundary, const solid_bsp2& attractiveBSP, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor, const stk::units::length& minDistance, const NumberComparisonPolicy& cmp, Executor&& exec)
			: m_halfcell(0.5 * granularity)
			, m_tree(detail::polygon_collection_as_segment_range(boundary), geometrix::partition_policies::autopartition_policy{}, cmp) 
		{
			using namespace stk;
			using namespace geometrix;
			auto wp = weight_policy{distanceSaturation, attractionFactor};
			exec.for_each(boundary, [&, this](const polygon_with_holes2& p)
			{
				generate_points(p, granularity, minDistance, attractiveBSP, wp);
			});

			if (m_integral.empty() || m_positions.empty()) 
				throw std::invalid_argument("biased_position_generator specified with invalid geometry.");

			make_integral();
		}

		//! Generates points within the simple polygonal boundary with a bias towards the geometry in attractiveSegments. 
		//! Granularity specifies the spacing of the Steiner points used to generate the underlying mesh.
		//! Distance saturation sets an attraction threshold which limits the attractive potential of a segment once within the specified distance.
		//! Attraction factor is a quantity specifying the strength of the attraction.
		template <typename NumberComparisonPolicy, typename Executor>
		biased_position_grid(const std::vector<polygon_with_holes2>& boundary, const std::vector<segment2>& attractiveSegments, const stk::units::length& granularity, const stk::units::length& distanceSaturation, double attractionFactor, const stk::units::length& minDistance, const NumberComparisonPolicy& cmp, Executor&& exec)
			: biased_position_grid(boundary, solid_bsp2{attractiveSegments, geometrix::partition_policies::autopartition_policy{}, cmp}, granularity, distanceSaturation, attractionFactor, minDistance, cmp, std::forward<Executor>(exec))
		{

		}

		//! Returns a random position in an optional if there was a position found within the specified number of attempts.
		template <typename Gen>
		boost::optional<point2> get_random_position(Gen& gen, std::uint32_t maxAttempts = 100000) const
		{
			GEOMETRIX_ASSERT(!m_positions.empty());

			point2 p;
			do {
                boost::random::uniform_real_distribution<> U;
				auto rT = U(gen);
				auto it(std::lower_bound(m_integral.begin(), m_integral.end(), rT));
				std::size_t i = std::distance(m_integral.begin(), it);
				p = generate_random(i, gen);
			} while (m_tree.point_in_solid_space(p, make_tolerance_policy()) != geometrix::point_in_solid_classification::in_empty_space && --maxAttempts > 0);

			if (maxAttempts > 0)
				return p;
			else
				return boost::none;
		}

	private:

		//! Generate a random point inside a circle at m_position[i];
		template <typename Gen>
		point2 generate_random(std::size_t i, Gen& gen) const
		{
			GEOMETRIX_ASSERT(i < m_positions.size());
			using std::sqrt;

			const auto vx = vector2{ m_halfcell, 0.0 * boost::units::si::meters };
			const auto vy = vector2{ 0.0 * boost::units::si::meters, m_halfcell };

			auto U = boost::random::uniform_real_distribution<>{ -1.0, 1.0 };
			return m_positions[i] + U(gen) * vx + U(gen) * vy;
		}

		template <typename NumberComparisonPolicy>
		typename geometrix::bounds_tuple<point2>::type bounds(const polygon2& pgon, const NumberComparisonPolicy& compare)
		{
			return geometrix::get_bounds(pgon, compare);
		}

		template <typename NumberComparisonPolicy>
		typename geometrix::bounds_tuple<point2>::type bounds(const polygon_with_holes2& pgon, const NumberComparisonPolicy& compare)
		{
			return geometrix::get_bounds(pgon.get_outer(), compare);
		}

		template <typename Polygon>
		void generate_points(const Polygon& pgon, const stk::units::length& cell, const stk::units::length& minDistance, const solid_bsp2& bsp, const weight_policy& wp)
		{
			using namespace geometrix;
			using namespace stk;

			auto cmp = make_tolerance_policy();
			auto obounds = bounds(pgon, cmp);
			auto grid = grid_traits<stk::units::length>(obounds, cell);

			stk::units::length xmin, xmax, ymin, ymax;
			std::tie(xmin, xmax, ymin, ymax) = obounds;

			auto m2 = minDistance * minDistance;
			std::uint32_t imin = grid.get_x_index(xmin), imax = grid.get_x_index(xmax), jmin = grid.get_y_index(ymin), jmax = grid.get_y_index(ymax);
			for (auto j = jmin; j <= jmax; ++j) {
				for (auto i = imin; i <= imax; ++i) {
					auto c = grid.get_cell_centroid(i, j);
					std::size_t idx;
					auto d2 = bsp.get_min_distance_sqrd_to_solid(c, idx, cmp);
					if (d2 > m2 && m_tree.point_in_solid_space(c, cmp) == point_in_solid_classification::in_empty_space) {
						auto w = wp.get_weight(d2);
						{
							auto lk = std::unique_lock<std::mutex>{ m_mtx };
							m_positions.push_back(c);
							m_integral.push_back(w);
						}
					}
				}
			}
		}

		void make_integral()
		{
			auto sum = boost::accumulate(m_integral, 0.0);
			GEOMETRIX_ASSERT(sum > 0.0);
			auto last = double{};
			for (auto& w : m_integral) {
				w = last + w / sum;
				last = w;
			}
		}

		stk::units::length  m_halfcell;
		std::vector<point2> m_positions;
		std::vector<double> m_integral;
		stk::solid_bsp2     m_tree;
		std::mutex          m_mtx;

	};
}//! namespace stk;

