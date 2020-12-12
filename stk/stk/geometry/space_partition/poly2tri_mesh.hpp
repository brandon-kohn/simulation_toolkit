//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stk/geometry/space_partition/mesh.hpp>
#include <stk/utility/scope_exit.hpp>
#include <stk/utility/associative_insert.hpp>
#include <stk/geometry/primitive/polygon_with_holes.hpp>
#include <geometrix/algorithm/point_sequence/is_polygon_simple.hpp>
#include <poly2tri/poly2tri.h>

namespace stk {
    inline mesh2 generate_mesh(const std::vector<stk::polygon_with_holes2>& polygons)
    {
        using namespace geometrix;
        using namespace stk;
        std::vector<p2t::Point*> memory;
        std::map<point2, std::size_t> allIndices;
        std::vector<point2> pArray;
        std::vector<std::size_t> tArray;
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
        auto addTriangle = [&](const point2& p0, const point2& p1, const point2& p2)
        {
            auto i0 = getIndex(p0);
            auto i1 = getIndex(p1);
            auto i2 = getIndex(p2);
            tArray.push_back(i0);
            tArray.push_back(i1);
            tArray.push_back(i2);
        };

        STK_SCOPE_EXIT( for( auto p : memory ) delete p; );
        for (const auto& polygon : polygons)
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
                memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
                polygon_.push_back(memory.back());
            }

            p2t::CDT cdt(polygon_);

            //! Add the holes
            for (const auto& hole : polygon.get_holes())
            {
                std::vector<p2t::Point*> hole_;
                for (const auto& p : hole)
                {
                    memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
                    hole_.push_back(memory.back());
                }

                cdt.AddHole(hole_);
            }

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
        }

        return mesh2( pArray, tArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder() );
    }

    inline mesh2 generate_mesh(const std::vector<stk::polygon_with_holes2>& polygons, const std::vector<point2>& steinerPoints)
    {
        using namespace geometrix;
        using namespace stk;
        std::vector<p2t::Point*> memory;
        std::map<point2, std::size_t> allIndices;
        std::vector<point2> pArray;
        std::vector<std::size_t> tArray;
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
        auto addTriangle = [&](const point2& p0, const point2& p1, const point2& p2)
        {
            auto i0 = getIndex(p0);
            auto i1 = getIndex(p1);
            auto i2 = getIndex(p2);
            tArray.push_back(i0);
            tArray.push_back(i1);
            tArray.push_back(i2);
        };

        STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

        for (const auto& polygon : polygons)
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
                memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
                polygon_.push_back(memory.back());
            }

            p2t::CDT cdt(polygon_);

            //! Add the holes
            for (const auto& hole : polygon.get_holes())
            {
                std::vector<p2t::Point*> hole_;
                for (const auto& p : hole)
                {
                    memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
                    hole_.push_back(memory.back());
                }

                cdt.AddHole(hole_);
            }

            //! Add the points
            for (const auto& p : steinerPoints)
            {
                memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
                cdt.AddPoint(memory.back());
            }

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
        }

        return mesh2( pArray, tArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder() );
    }

    inline mesh2 generate_mesh(const stk::polygon_with_holes2& polygon, const std::vector<stk::point2>& steinerPoints)
    {
        using namespace geometrix;
        if (polygon.get_outer().empty() || !is_polygon_simple(polygon.get_outer(), make_tolerance_policy()))
            throw std::invalid_argument("polygon not simple");

        for (const auto& hole : polygon.get_holes())
        {
            if (!is_polygon_simple(hole, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");
        }

        std::vector<p2t::Point*> polygon_, memory;
        STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

        for( const auto& p : polygon.get_outer() )
        {
            polygon_.push_back( new p2t::Point( get<0>( p ).value(), get<1>( p ).value() ) );
            memory.push_back( polygon_.back() );
        }

        p2t::CDT cdt( polygon_ );

        //! Add the holes
        for( const auto& hole : polygon.get_holes() )
        {
            std::vector<p2t::Point*> hole_;
            for( const auto& p : hole )
            {
                auto p_ = new p2t::Point( get<0>( p ).value(), get<1>( p ).value() );
                hole_.push_back( p_ );
                memory.push_back( p_ );
            }

            cdt.AddHole( hole_ );
        }

        //! Add the points
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

        return mesh2( points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder() );
    }

    inline mesh2 generate_mesh(const stk::polygon_with_holes2& polygon)
    {
        using namespace geometrix;
        if (polygon.get_outer().empty() || !is_polygon_simple(polygon.get_outer(), make_tolerance_policy()))
            throw std::invalid_argument("polygon not simple");

        for (const auto& hole : polygon.get_holes())
        {
            if (!is_polygon_simple(hole, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");
        }

        std::vector<p2t::Point*> polygon_, memory;
        STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

        for( const auto& p : polygon.get_outer() )
        {
            polygon_.push_back( new p2t::Point( get<0>( p ).value(), get<1>( p ).value() ) );
            memory.push_back( polygon_.back() );
        }

        p2t::CDT cdt( polygon_ );

        //! Add the holes
        for( const auto& hole : polygon.get_holes() )
        {
            std::vector<p2t::Point*> hole_;
            for( const auto& p : hole )
            {
                auto p_ = new p2t::Point( get<0>( p ).value(), get<1>( p ).value() );
                hole_.push_back( p_ );
                memory.push_back( p_ );
            }

            cdt.AddHole( hole_ );
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

        return mesh2( points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder() );
    }

    inline mesh2 generate_mesh(const stk::polygon2& polygon)
    {
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

        return mesh2( points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder() );
    }

    inline mesh2 generate_mesh(const stk::polygon2& polygon, const std::vector<stk::point2>& steinerPoints)
    {
        using namespace geometrix;
        if (polygon.empty() || !is_polygon_simple(polygon, make_tolerance_policy()))
            throw std::invalid_argument("polygon not simple");

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
        std::vector<point2> points(indices.size());
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

        return mesh2(points, iArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder());
    }

    inline mesh2 generate_mesh(const std::vector<stk::polygon2>& polygons)
    {
        using namespace geometrix;
        using namespace stk;
        std::vector<p2t::Point*> memory;
        std::map<point2, std::size_t> allIndices;
        std::vector<point2> pArray;
        std::vector<std::size_t> tArray;
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
        auto addTriangle = [&](const point2& p0, const point2& p1, const point2& p2)
        {
            auto i0 = getIndex(p0);
            auto i1 = getIndex(p1);
            auto i2 = getIndex(p2);
            tArray.push_back(i0);
            tArray.push_back(i1);
            tArray.push_back(i2);
        };

        STK_SCOPE_EXIT( for( auto p : memory ) delete p; );

        for (const auto& polygon : polygons)
        {
            std::vector<p2t::Point*> polygon_;
            if (polygon.empty() || !is_polygon_simple(polygon, make_tolerance_policy()))
                throw std::invalid_argument("polygon not simple");

            for (const auto& p : polygon)
            {
                memory.push_back(new p2t::Point(get<0>(p).value(), get<1>(p).value()));
                polygon_.push_back(memory.back());
            }

            p2t::CDT cdt(polygon_);

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
        }

        return mesh2( pArray, tArray, make_tolerance_policy(), stk::rtree_triangle_cache_builder() );
    }
}//! namespace stk;

