//
//! Copyright © 2018-2019
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
#include <geometrix/primitive/point_sequence_utilities.hpp>
#include <stk/geometry/transformer.hpp>
#include <geometrix/algorithm/point_sequence/is_polygon_simple.hpp>

namespace stk {

    inline void to_clipper(ClipperLib::Clipper& clip, const polygon2& a, ClipperLib::PolyType type, unsigned int scale)
    {
        ClipperLib::Path path;
		path.reserve(a.size());

        for (const auto& p : a)
            path << ClipperLib::IntPoint(p[0].value() * scale, p[1].value() * scale);

        clip.AddPath(path, type, true);
    }

    inline void to_clipper(ClipperLib::Clipper& clip, const polygon_with_holes2& a, ClipperLib::PolyType type, unsigned int scale)
    {
        to_clipper(clip, a.get_outer(), type, scale);
        for (const auto& hole : a.get_holes())
            to_clipper(clip, hole, type, scale);
    }

	//! Add a polyline to clipper. NOTE: Must be a subject type.
    inline void to_clipper(ClipperLib::Clipper& clip, const polyline2& a, ClipperLib::PolyType type, unsigned int scale)
    {
		GEOMETRIX_ASSERT(type == ClipperLib::ptSubject);
        ClipperLib::Path path;
		path.reserve(a.size());
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
		outerStack.reserve(ptree.ChildCount());

        for (int i = 0; i < ptree.ChildCount(); ++i)
            outerStack.push_back(ptree.Childs[i]);

        while (!outerStack.empty())
        {
            auto pOuter = outerStack.back();
            outerStack.pop_back();
            GEOMETRIX_ASSERT(!pOuter->IsHole());

            polygon_with_holes2 contour;
			contour.get_outer().reserve(pOuter->Contour.size());
            for (const auto& p : pOuter->Contour)
                contour.get_outer().emplace_back((p.X / static_cast<double>(scale)) * units::si::meters, (p.Y / static_cast<double>(scale)) * units::si::meters);

            for (int i = 0; i < pOuter->ChildCount(); ++i)
            {
                auto pChild = pOuter->Childs[i];
                GEOMETRIX_ASSERT(pChild->IsHole());
                if (pChild->IsHole())
                {
                    polygon2 hole;
					hole.reserve(pChild->Contour.size());
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

	inline void clipper_clean(polygon2& a, unsigned int scale)
	{
		ClipperLib::Path path;
		path.reserve(a.size());

		for (const auto& p : a)
			path.emplace_back(p[0].value() * scale, p[1].value() * scale);

		a.clear();
		for(const auto& p : path)
			a.emplace_back((p.X / static_cast<double>(scale)) * units::si::meters, (p.Y / static_cast<double>(scale)) * units::si::meters);
	}

	inline void clipper_clean(polygon_with_holes2& a, unsigned int scale)
	{
		clipper_clean(a.get_outer(), scale);
		for (auto& hole : a.get_holes())
			clipper_clean(hole, scale);
	}

	template <typename T>
	inline void clipper_clean(std::vector<T>& a, unsigned int scale)
	{
		for (const auto& i : a)
			clipper_clean(i, scale);
	}

    template <typename Geometry1, typename Geometry2>
    inline std::vector<polygon_with_holes2> clipper_union_impl(ClipperLib::Clipper& clip, Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctUnion, ptree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1>
    inline std::vector<polygon_with_holes2> clipper_union_impl(ClipperLib::Clipper& clip, Geometry1&& a, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctUnion, ptree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
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
        clip.Execute(ClipperLib::ctDifference, ptree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
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
        clip.Execute(ClipperLib::ctIntersection, ptree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1, typename Geometry2, typename std::enable_if<std::is_same<polyline2, typename std::decay<Geometry1>::type>::value, int>::type = 0>
    inline std::vector<polyline2> clipper_intersection_impl(ClipperLib::Clipper& clip, Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptClip, scale);

        ClipperLib::PolyTree ptree;
		clip.Execute(ClipperLib::ctIntersection, ptree, ClipperLib::pftNonZero, ClipperLib::pftNonZero);
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

	inline std::vector<polygon_with_holes2> clipper_simplify(const polygon_with_holes2& pgon, unsigned int scale)
	{
		auto result = clipper_union_simple(pgon.get_outer(), scale);
		auto holes = std::vector<polygon_with_holes2>{};

		for (const auto h : pgon.get_holes())
			holes = clipper_union_simple(holes, geometrix::reverse(h), scale);

		for (const auto h : holes)
			result = clipper_difference_simple(result, h, scale);
		return result;
	}

	inline std::vector<polygon_with_holes2> clipper_simplify(const polygon2& pgon, unsigned int scale)
	{
		return clipper_union_simple(pgon, scale);
	}

	inline std::vector<stk::polygon_with_holes2> heal_non_simple_polygon(const stk::polygon_with_holes2& pgon, stk::units::length const& healOffset, unsigned int scale)
	{
		using namespace stk;
		using namespace geometrix;

		std::vector<stk::polygon_with_holes2> outer = { pgon };
		if (!is_polygon_simple(pgon.get_outer(), make_tolerance_policy()))
			outer = clipper_offset(pgon.get_outer(), healOffset, scale);

		for (const auto& h : pgon.get_holes())
		{
			auto nh = clipper_offset(h, healOffset, scale);
			outer = clipper_difference_simple(outer, nh, scale);
		}

		return outer;
	}

	inline void heal_non_simple_polygons(std::vector<stk::polygon_with_holes2>& pgons, stk::units::length const& offset = 0.001 * boost::units::si::meters, unsigned int scale = 10000)
	{
		for (std::size_t i = 0; i < pgons.size();) 
		{
			if (!is_polygon_with_holes_simple(pgons[i], make_tolerance_policy())) 
			{
				auto newGeometry = heal_non_simple_polygon(pgons[i], offset, scale);
				std::iter_swap(pgons.begin() + i, pgons.end() - 1);
				pgons.pop_back();
				pgons.insert(pgons.end(), newGeometry.begin(), newGeometry.end());
				continue;
			}

			++i;
		}
	}

}//! namespace stk;

