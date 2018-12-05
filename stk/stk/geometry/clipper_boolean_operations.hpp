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
            to_clipper(clip, hole, (ClipperLib::PolyType)!type, scale);
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

    template <typename Geometry1, typename Geometry2>
    inline std::vector<polygon_with_holes2> clipper_union(Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctUnion, ptree);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1>
    inline std::vector<polygon_with_holes2> clipper_union(Geometry1&& a, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        to_clipper(clip, a, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctUnion, ptree);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1, typename Geometry2>
    inline std::vector<polygon_with_holes2> clipper_difference(Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptClip, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctDifference, ptree);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1, typename Geometry2>
    inline std::vector<polygon_with_holes2> clipper_intersection(Geometry1&& a, Geometry2&& b, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        to_clipper(clip, a, ClipperLib::ptSubject, scale);
        to_clipper(clip, b, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctIntersection, ptree);
        return to_polygons_with_holes(ptree, scale);
    }

    template <typename Geometry1>
    inline std::vector<polygon_with_holes2> clipper_intersection(Geometry1&& a, unsigned int scale)
    {
        ClipperLib::Clipper clip;
        to_clipper(clip, a, ClipperLib::ptSubject, scale);

        ClipperLib::PolyTree ptree;
        clip.Execute(ClipperLib::ctIntersection, ptree);
        return to_polygons_with_holes(ptree, scale);
    }
}//! namespace stk;

