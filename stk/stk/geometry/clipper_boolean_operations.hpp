//
//! Copyright © 2018
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <clipper/clipper.hpp>
#include <stk/geometry/primitive/point.hpp>
#include <stk/geometry/primitive/polygon.hpp>
#include <stk/geometry/primitive/polyline.hpp>
#include <stk/geometry/primitive/polygon_with_holes.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>

namespace stk {

    inline void to_clipper(ClipperLib::Clipper& clip, const polygon2& a, ClipperLib::PolyType type, unsigned int scale)
    {
        ClipperLib::Path path;

        for (const auto& p : a)
            path << ClipperLib::IntPoint(p[0].value() * scale, p[1].value() * scale);

        clip.AddPath(path, type, true);
    }

    inline void to_clipper(ClipperLib::Clipper& clip, const polygon_with_holes2& a, ClipperLib::PolyType type, unsigned int scale)
    {
        to_clipper(clip, a.get_outer(), type, scale);
        for (const auto& hole : a.get_holes())
            //to_clipper(clip, hole, (ClipperLib::PolyType)!type, scale);
            to_clipper(clip, hole, type, scale);
    }

	//! Add a polyline to clipper. NOTE: Must be a subject type.
    inline void to_clipper(ClipperLib::Clipper& clip, const polyline2& a, ClipperLib::PolyType type, unsigned int scale)
    {
        ClipperLib::Path path;
		GEOMETRIX_ASSERT(type == ClipperLib::ptSubject);

        for (const auto& p : a)
            path << ClipperLib::IntPoint(p[0].value() * scale, p[1].value() * scale);

        clip.AddPath(path, ClipperLib::ptSubject, false);
    }

    template <typename T>
    inline void to_clipper(ClipperLib::Clipper& clip, const std::vector<T>& a, ClipperLib::PolyType type, unsigned int scale)
    {
        for (const auto& i : a)
            to_clipper(clip, i, type, scale);
    }

    inline std::vector<polygon_with_holes2> to_polygons_with_holes(ClipperLib::PolyTree &ptree, unsigned int scale)
    {
        std::vector<polygon_with_holes2> results;
        std::vector<ClipperLib::PolyNode*> outerStack;

        for (int i = 0; i < ptree.ChildCount(); ++i)
            outerStack.push_back(ptree.Childs[i]);

        while (!outerStack.empty())
        {
            auto pOuter = outerStack.back();
            outerStack.pop_back();
            GEOMETRIX_ASSERT(!pOuter->IsHole());

            polygon_with_holes2 contour;
            for (const auto& p : pOuter->Contour)
                contour.get_outer().emplace_back((p.X / static_cast<double>(scale)) * units::si::meters, (p.Y / static_cast<double>(scale)) * units::si::meters);

            for (int i = 0; i < pOuter->ChildCount(); ++i)
            {
                auto pChild = pOuter->Childs[i];
                GEOMETRIX_ASSERT(pChild->IsHole());
                if (pChild->IsHole())
                {
                    polygon2 hole;
                    for (const auto& p : pChild->Contour)
                        hole.emplace_back((p.X / static_cast<double>(scale))* units::si::meters, (p.Y / static_cast<double>(scale))* units::si::meters);
                    contour.add_hole(std::move(hole));

                    for (int q = 0; q < pChild->ChildCount(); ++q)
                    {
                        GEOMETRIX_ASSERT(!pChild->Childs[q]->IsHole());
                        outerStack.push_back(pChild->Childs[q]);
                    }
                }
            }

            results.emplace_back(std::move(contour));
        }

        return results;
    }

    inline std::vector<polyline2> to_polylines(ClipperLib::PolyTree &ptree, unsigned int scale)
    {
        std::vector<polyline2> results;
		results.reserve(ptree.Total());
		for (auto i = 0; i < ptree.ChildCount(); ++i) 
		{
			if (ptree.Childs[i]->IsOpen()) 
			{
				polyline2 pline;
				for (const auto& p : ptree.Childs[i]->Contour)
					pline.emplace_back((p.X / static_cast<double>(scale)) * units::si::meters, (p.Y / static_cast<double>(scale)) * units::si::meters);
				results.emplace_back(std::move(pline));
			}
		}

        return results;
    }

    template <typename Geometry1, typename Geometry2>
    inline std::vector<polygon_with_holes2> clipper_union_impl(ClipperLib::Clipper& clip, Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctUnion, ptree);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1>
    inline std::vector<polygon_with_holes2> clipper_union_impl(ClipperLib::Clipper& clip, Geometry1&& a, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctUnion, ptree);
        return to_polygons_with_holes(ptree, scale);
    }
    
    template <typename... Args>
    inline std::vector<polygon_with_holes2> clipper_union(Args&&...a)
    {
        ClipperLib::Clipper clip;
        return clipper_union_impl(clip, std::forward<Args>(a)...);
    }
    
    template <typename... Args>
    inline std::vector<polygon_with_holes2> clipper_union_simple(Args&&...a)
    {
        ClipperLib::Clipper clip;
        clip.StrictlySimple(true);
        return clipper_union_impl(clip, std::forward<Args>(a)...);
    }

    template <typename Geometry1, typename Geometry2>
    inline std::vector<polygon_with_holes2> clipper_difference_impl(ClipperLib::Clipper& clip, Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptClip, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctDifference, ptree);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename... Args>
    inline std::vector<polygon_with_holes2> clipper_difference(Args&&...a)
    {
        ClipperLib::Clipper clip;
        return clipper_difference_impl(clip, std::forward<Args>(a)...);
    }
    
    template <typename... Args>
    inline std::vector<polygon_with_holes2> clipper_difference_simple(Args&&...a)
    {
        ClipperLib::Clipper clip;
        clip.StrictlySimple(true);
        return clipper_difference_impl(clip, std::forward<Args>(a)...);
    }

    template <typename Geometry1, typename Geometry2, typename std::enable_if<!std::is_same<polyline2, typename std::decay<Geometry1>::type>::value, int>::type = 0>
    inline std::vector<polygon_with_holes2> clipper_intersection_impl(ClipperLib::Clipper& clip, Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptClip, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctIntersection, ptree);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1, typename Geometry2, typename std::enable_if<std::is_same<polyline2, typename std::decay<Geometry1>::type>::value, int>::type = 0>
    inline std::vector<polyline2> clipper_intersection_impl(ClipperLib::Clipper& clip, Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptClip, scale);

        ClipperLib::PolyTree ptree;
		clip.Execute(ClipperLib::ctIntersection, ptree);
        return to_polylines(ptree, scale);
    }
    
    template <typename Geometry1, typename Geometry2, typename std::enable_if<std::is_same<polyline2, typename std::decay<Geometry1>::type>::value, int>::type = 0>
    inline std::vector<polyline2> clipper_intersection(Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        return clipper_intersection_impl(clip, std::forward<Geometry1>(a), std::forward<Geometry2>(b), scale);
    }
    
	template <typename Geometry1, typename Geometry2, typename std::enable_if<!std::is_same<polyline2, typename std::decay<Geometry1>::type>::value, int>::type = 0>
    inline std::vector<polygon_with_holes2> clipper_intersection(Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        return clipper_intersection_impl(clip, std::forward<Geometry1>(a), std::forward<Geometry2>(b), scale);
    }
    
    template <typename Geometry1, typename Geometry2, typename std::enable_if<std::is_same<polyline2, typename std::decay<Geometry1>::type>::value, int>::type = 0>
    inline std::vector<polyline2> clipper_intersection_simple(Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        clip.StrictlySimple(true);
        return clipper_intersection_impl(clip, std::forward<Geometry1>(a), std::forward<Geometry2>(b), scale);
    }
    
	template <typename Geometry1, typename Geometry2, typename std::enable_if<!std::is_same<polyline2, typename std::decay<Geometry1>::type>::value, int>::type = 0>
    inline std::vector<polygon_with_holes2> clipper_intersection_simple(Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        clip.StrictlySimple(true);
        return clipper_intersection_impl(clip, std::forward<Geometry1>(a), std::forward<Geometry2>(b), scale);
    }

    inline std::vector<polygon_with_holes2> clipper_offset(const polygon2& pgon, const units::length& offset, unsigned int scale)
    {
        ClipperLib::Path boundary;

        for (const auto& p : pgon)
            boundary << ClipperLib::IntPoint(p[0].value() * scale, p[1].value() * scale);

        ClipperLib::ClipperOffset co;
        co.AddPath(boundary, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);
        ClipperLib::PolyTree ptree;
        co.Execute(ptree, offset.value() * scale);

        return to_polygons_with_holes(ptree, scale);
    }

    inline std::vector<polygon_with_holes2> clipper_offset(const polygon_with_holes2& pgon, const units::length& offset, unsigned int scale)
    {
        ClipperLib::ClipperOffset co;
        
		ClipperLib::Path boundary;
        for (const auto& p : pgon.get_outer())
            boundary << ClipperLib::IntPoint(p[0].value() * scale, p[1].value() * scale);
        co.AddPath(boundary, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);

		for (auto const& hole : pgon.get_holes())
		{
			boundary.clear();
			for (const auto& p : hole)
				boundary << ClipperLib::IntPoint(p[0].value() * scale, p[1].value() * scale);
			co.AddPath(boundary, ClipperLib::jtSquare, ClipperLib::etClosedPolygon);
		}

        ClipperLib::PolyTree ptree;
        co.Execute(ptree, offset.value() * scale);

        return to_polygons_with_holes(ptree, scale);
    }
}//! namespace stk;

