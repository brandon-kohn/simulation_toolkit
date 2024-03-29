//
//! Copyright � 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/geometry/transformer.hpp>
#include <geometrix/arithmetic/matrix/trace.hpp>

TEST(TransformerTestSuite, testBasicUsage)
{
    using namespace ::testing;
    using namespace stk;
    using namespace geometrix;

    point2 op{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters };
    point2 dp{ -1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters };
    point2 dr{ 0.0 * boost::units::si::meters, 0.0 * boost::units::si::meters };
    units::angle theta = geometrix::constants::half_pi<units::angle>();

    transformer2 sut;
    sut.translate(dp).rotate(theta).translate(-as_vector(op));

    auto r = sut(op);   
    EXPECT_TRUE(numeric_sequence_equals_2d(r, dp, make_tolerance_policy()));

    auto r2 = sut(point2{ 0.5 * boost::units::si::meters, 0.5 * boost::units::si::meters });
    EXPECT_TRUE(numeric_sequence_equals_2d(r2, point2{ -0.5 * boost::units::si::meters, 0.5 * boost::units::si::meters }, make_tolerance_policy()));
}

TEST(TransformerTestSuite, testOffsetRotatedAxes)
{
    using namespace ::testing;
    using namespace stk;
    using namespace geometrix;

    point2 oA{ 0.0 * boost::units::si::meters, 0.0 * boost::units::si::meters };
    point2 oB{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters };
    units::angle theta = 0.25 * geometrix::constants::pi<units::angle>();

    transformer2 sut;
    sut.translate(oB).rotate(theta).translate(-as_vector(oA));

    auto segxA = segment2{ oA, point2{ 1.0 * boost::units::si::meters, 0.0 * boost::units::si::meters } };
    auto segyA = segment2{ oA, point2{ 0.0 * boost::units::si::meters, 1.0 * boost::units::si::meters } };

    auto segxB = sut(segxA);
    auto segyB = sut(segyA);
    
    EXPECT_TRUE(numeric_sequence_equals_2d(segxB.get_start(), point2{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters }, make_tolerance_policy()));
    EXPECT_TRUE(numeric_sequence_equals_2d(segxB.get_end(), point2{ 1.7071067811865475 * boost::units::si::meters, 1.7071067811865475 * boost::units::si::meters }, make_tolerance_policy()));

    EXPECT_TRUE(numeric_sequence_equals_2d(segyB.get_start(), point2{ 1.0 * boost::units::si::meters, 1.0 * boost::units::si::meters }, make_tolerance_policy()));
    EXPECT_TRUE(numeric_sequence_equals_2d(segyB.get_end(), point2{ 0.29289321881345243 * boost::units::si::meters, 1.7071067811865475 * boost::units::si::meters }, make_tolerance_policy()));
}

TEST(TransformerTestSuite, VectorTranslationTest)
{
    using namespace ::testing;
    using namespace geometrix;
    using namespace stk;
    
    point2 start(5.0* units::si::meters, 0.0* units::si::meters), end(10.0* units::si::meters, 0.0* units::si::meters);
    point2 start2(-5.0* units::si::meters, 0.0* units::si::meters), end2(-5.0* units::si::meters, 5.0* units::si::meters);

    //vector2 translation = start2 - start;
    auto orientationA = normalize(end - start);
    auto orientationB = normalize(end2 - start2);

    transformer2 sut;
    sut.translate(start2).rotate(orientationA, orientationB).translate(-as_vector(start));

    vector2 v(1.0 * units::si::meters, 0.0 * units::si::meters);
    auto nv = sut(v);

    EXPECT_TRUE(numeric_sequence_equals_2d(nv, vector2(0.0* units::si::meters, 1.0 * units::si::meters), make_tolerance_policy()));
}

TEST(TransformerTestSuite, VelocityTranslationTest)
{
    using namespace ::testing;
    using namespace geometrix;
    using namespace stk;
    
    point2 start(5.0* units::si::meters, 0.0* units::si::meters), end(10.0* units::si::meters, 0.0* units::si::meters);
    point2 start2(-5.0* units::si::meters, 0.0* units::si::meters), end2(-5.0* units::si::meters, 5.0* units::si::meters);

    //vector2 translation = start2 - start;
    auto orientationA = normalize(end - start);
    auto orientationB = normalize(end2 - start2);

    transformer2 sut;
    sut.translate(start2).rotate(orientationA, orientationB).translate(-as_vector(start));

    velocity2 v(1.0 * units::si::meters_per_second, 0.0 * units::si::meters_per_second);
    auto nv = sut(v);

    EXPECT_TRUE(numeric_sequence_equals_2d(nv, velocity2(0.0* units::si::meters_per_second, 1.0 * units::si::meters_per_second), make_tolerance_policy()));
}

TEST(TransformerTestSuite, testCase1)
{
    using namespace ::testing;
    using namespace stk;
    using namespace geometrix;
    using std::cos;
    using std::sin;

    point2 op{ -118.04574333498022 * boost::units::si::meters, 9.9930356699042022 * boost::units::si::meters };
    point2 dp{ -118.17108733498026 * boost::units::si::meters, 10.032866669818759 * boost::units::si::meters };
    point2 dr{ -52.078490334970411 * boost::units::si::meters, 18.071499669924378 * boost::units::si::meters };
    //units::angle theta = 0.088582999999999995 * units::si::radians;
    units::angle theta = 0.088582999999999995 * (geometrix::constants::pi<double>() / 180.0) * units::si::radians;

    //auto cost = cos(theta);
    //auto sint = sin(theta);
    //vector2 t = op - op;

    polyline2 from{ { -98.812037174440036 * boost::units::si::meters, 3.704616406039666 * boost::units::si::meters },{ -98.730050000012852 * boost::units::si::meters, 8.6038500005379319 * boost::units::si::meters },{ -98.730485487279253 * boost::units::si::meters, 8.6038531885851981 * boost::units::si::meters } };
    polyline2 to{ { -98.723170052621242 * boost::units::si::meters, 8.4114582485210363 * boost::units::si::meters },{ -98.81315000000177 * boost::units::si::meters, 3.5125500001013279 * boost::units::si::meters },{ -98.812037060730916 * boost::units::si::meters, 3.5125469878725553 * boost::units::si::meters } };

    transformer2 sut;
    sut.translate(dp).rotate(theta).translate(-as_vector(op));
    
    auto r = sut(op);
    //auto r2 = sut(from[0]);
    
    EXPECT_TRUE(numeric_sequence_equals_2d(r, dp, make_tolerance_policy()));
}

namespace {

    template <typename Point>
    inline stk::point3 make_point3(const Point& p, const stk::units::length& z = 0.0 * stk::units::si::meters)
    {
        using namespace geometrix;
        using namespace stk;
        return point3{ get<0>(p), get<1>(p), z };
    }

    template <typename Polygon>
    inline stk::polygon3 make_polygon3(const Polygon& polygon, const stk::units::length& z = 0.0 * stk::units::si::meters)
    {
        using namespace geometrix;
        using namespace boost::adaptors;
        using namespace stk;

        using point_t = typename point_sequence_traits<Polygon>::point_type;

        polygon3 result;
        boost::copy(polygon | transformed([z](const point_t& p) { return make_point3(p, z); }), std::back_inserter(result));

        return result;
    }

    template <typename Point>
    inline stk::point2 make_point2(const Point& p)
    {
        using namespace geometrix;
        using namespace stk;
        return point2{ get<0>(p), get<1>(p) };
    }

    template <typename Polygon>
    inline stk::polygon2 make_polygon2(const Polygon& polygon)
    {
        using namespace geometrix;
        using namespace boost::adaptors;
        using namespace stk;
        using point_t = typename point_sequence_traits<Polygon>::point_type;

        polygon2 result;
        boost::copy(polygon | transformed([](const point_t& p) { return make_point2(p); }), std::back_inserter(result));

        return result;
    }

    template<int N, int M, int P>
    inline geometrix::matrix<double, N, P> mm_mult(const geometrix::matrix<double, N, M>& A, const geometrix::matrix<double, M, P>& B)
    {
        geometrix::matrix<double, N, P> C;

        for (auto i = 0; i < N; ++i)
            for (auto j = 0; j < P; ++j)
            {
                double sum = 0.0;
                for (auto k = 0; k < M; ++k)
                    sum += A[i][k] * B[k][j];
                C[i][j] = sum;
            }
        
        return C;
    }

    template <typename MatrixA, typename MatrixB, typename NumberComparisonPolicy>
    inline bool compare_matrices(const MatrixA& A, const MatrixB& B, const NumberComparisonPolicy& cmp)
    {
        using namespace geometrix;
        if (row_dimension_of<MatrixA>::value != row_dimension_of<MatrixB>::value || column_dimension_of<MatrixA>::value != column_dimension_of<MatrixB>::value)
            return false;

        for (auto j = 0; j < column_dimension_of<MatrixA>::value; ++j)
            for (auto i = 0; i < row_dimension_of<MatrixA>::value; ++i)
                if (!cmp.equals(A[i][j], B[i][j]))
                    return false;
        return true;
    }

	template <std::size_t Rows, std::size_t Columns, typename T>
	inline geometrix::matrix<double, Rows + 1, Columns> concatenate_row(const geometrix::matrix<double, Rows, Columns>& m, geometrix::vector<T, Columns>& v)
	{
		using namespace geometrix;

		matrix<double, Rows + 1, Columns> r;
		for (int i = 0; i < Rows; ++i)
			for (int j = 0; j < Columns; ++j)
				r[i][j] = m[i][j];
		for (int j = 0; j < Columns; ++j)
			r[Rows][j] = construct<double>(v[j]);

		return r;
	}

	template <std::size_t Rows, std::size_t Columns, typename T>
	inline geometrix::matrix<double, Rows, Columns+1> concatenate_column(const geometrix::matrix<double, Rows, Columns>& m, geometrix::vector<T, Columns>& v)
	{
		using namespace geometrix;

		matrix<double, Rows, Columns+1> r;
		for (int i = 0; i < Rows; ++i)
		{
			for (int j = 0; j < Columns; ++j)
				r[i][j] = m[i][j];
			r[i][Columns] = construct<double>(v[i]);
		}

		return r;
	}

	template <std::size_t Rows, std::size_t Columns, typename T>
	inline geometrix::matrix<double, Rows, Columns> assign_translation(const geometrix::matrix<double, Rows, Columns>& m, geometrix::vector<T, Rows-1>& v)
	{
		using namespace geometrix;

		matrix<double, Rows, Columns> r = m;
		for (auto i = std::size_t{}; i < Rows-1; ++i)
			r[i][Columns-1] = construct<double>(v[i]);
		
		return r;
	}
	
	inline stk::transformer2 estimate_2d_transformer(const geometrix::segment<stk::point3>& geometryA, const stk::vector3& utmToA, const stk::vector3& bToUTM, const stk::vector3& v, const stk::units::angle& roll, const stk::units::angle& pitch, const stk::units::angle& yaw)
	{
		using namespace stk;
		using namespace geometrix;

		using xform_t = transformer<3, post_multiplication_matrix_concatenation_policy>;
		
		auto rot_transpose = xform_t{ trans(rotate3_x(roll) * rotate3_y(pitch) * rotate3_z(yaw)) };
		auto rv = vector3{ -rot_transpose(v) };
		auto xform = xform_t{ translate3(bToUTM) * assign_translation(rot_transpose.matrix(), rv) * translate3(utmToA) };

		//! Estimate a 1 meter segment from originA along the x/y diagonal slope 1.
		auto geometryAwrtB = xform(geometryA);

		auto ra = segment2{ geometryA.get_start(), geometryA.get_end() };
		auto rb = segment2{ geometryAwrtB.get_start(), geometryAwrtB.get_end() };

		auto orientationA = normalize(geometryA.get_end() - geometryA.get_start());
		auto orientationB = normalize(geometryAwrtB.get_end() - geometryAwrtB.get_start());

		return transformer2().translate(vector2{ bToUTM }).translate(rb.get_start() - bToUTM).rotate(dimensionless2{ orientationA[0], orientationA[1] }, dimensionless2{ orientationB[0], orientationB[1] }).translate(-as_vector(ra.get_start() + utmToA)).translate(vector2{ utmToA });
	}
}
TEST(TransformerTestSuite, test3DTransform)
{
	using namespace geometrix;
	using namespace stk;

	auto A = polygon2{ { 414372.45361500001 * boost::units::si::meters, 3705830.9450559998 * boost::units::si::meters },{ 414372.46756899997 * boost::units::si::meters, 3705831.9419880002 * boost::units::si::meters },{ 414372.48152299999 * boost::units::si::meters, 3705832.9389200001 * boost::units::si::meters },{ 414372.49547700002 * boost::units::si::meters, 3705833.935852 * boost::units::si::meters },{ 414372.50943099998 * boost::units::si::meters, 3705834.9327839999 * boost::units::si::meters },{ 414372.52338500001 * boost::units::si::meters, 3705835.9297159999 * boost::units::si::meters },{ 414372.53733899997 * boost::units::si::meters, 3705836.9266479998 * boost::units::si::meters },{ 414372.551293 * boost::units::si::meters, 3705837.9235800002 * boost::units::si::meters },{ 414372.56541500002 * boost::units::si::meters, 3705839.3267470002 * boost::units::si::meters },{ 414372.593154 * boost::units::si::meters, 3705840.9143759999 * boost::units::si::meters },{ 414372.59961700003 * boost::units::si::meters, 3705841.8323169998 * boost::units::si::meters },{ 414372.60608 * boost::units::si::meters, 3705842.7502580001 * boost::units::si::meters },{ 414372.61254300002 * boost::units::si::meters, 3705843.6681989999 * boost::units::si::meters },{ 414372.61900599999 * boost::units::si::meters, 3705844.5861399998 * boost::units::si::meters },{ 414372.62258099997 * boost::units::si::meters, 3705845.5054680002 * boost::units::si::meters },{ 414369.60605100001 * boost::units::si::meters, 3705845.5492750001 * boost::units::si::meters },{ 414369.59149000002 * boost::units::si::meters, 3705844.5789180002 * boost::units::si::meters },{ 414369.57407899998 * boost::units::si::meters, 3705843.60984 * boost::units::si::meters },{ 414369.556668 * boost::units::si::meters, 3705842.6407619999 * boost::units::si::meters },{ 414369.53925700003 * boost::units::si::meters, 3705841.6716840002 * boost::units::si::meters },{ 414369.52184599999 * boost::units::si::meters, 3705840.702606 * boost::units::si::meters },{ 414369.50443500001 * boost::units::si::meters, 3705839.7335279998 * boost::units::si::meters },{ 414369.48702399997 * boost::units::si::meters, 3705838.7644500001 * boost::units::si::meters },{ 414369.46961299999 * boost::units::si::meters, 3705837.795372 * boost::units::si::meters },{ 414369.45220200001 * boost::units::si::meters, 3705836.8262939998 * boost::units::si::meters },{ 414369.43479099998 * boost::units::si::meters, 3705835.8572160001 * boost::units::si::meters },{ 414369.41738 * boost::units::si::meters, 3705834.8881379999 * boost::units::si::meters },{ 414369.39996900002 * boost::units::si::meters, 3705833.9190600002 * boost::units::si::meters },{ 414369.38255799998 * boost::units::si::meters, 3705832.949982 * boost::units::si::meters },{ 414369.365147 * boost::units::si::meters, 3705831.9809039999 * boost::units::si::meters },{ 414369.34773600003 * boost::units::si::meters, 3705831.0118260002 * boost::units::si::meters } };
	auto B = polygon2{ { 414375.17473700002 * boost::units::si::meters, 3705842.5063820002 * boost::units::si::meters },{ 414375.17236899998 * boost::units::si::meters, 3705843.4983870001 * boost::units::si::meters },{ 414375.17577500001 * boost::units::si::meters, 3705844.4875909998 * boost::units::si::meters },{ 414375.17918199999 * boost::units::si::meters, 3705845.4767959998 * boost::units::si::meters },{ 414375.18258800003 * boost::units::si::meters, 3705846.466 * boost::units::si::meters },{ 414375.185994 * boost::units::si::meters, 3705847.4552040002 * boost::units::si::meters },{ 414375.18939999997 * boost::units::si::meters, 3705848.4444090002 * boost::units::si::meters },{ 414375.19280700001 * boost::units::si::meters, 3705849.433613 * boost::units::si::meters },{ 414375.19621299999 * boost::units::si::meters, 3705850.4228170002 * boost::units::si::meters },{ 414375.19961900002 * boost::units::si::meters, 3705851.4120220002 * boost::units::si::meters },{ 414375.203025 * boost::units::si::meters, 3705852.4012259999 * boost::units::si::meters },{ 414375.20643199998 * boost::units::si::meters, 3705853.3904300001 * boost::units::si::meters },{ 414375.20983800001 * boost::units::si::meters, 3705854.3796350001 * boost::units::si::meters },{ 414375.21324399998 * boost::units::si::meters, 3705855.3688389999 * boost::units::si::meters },{ 414375.21665000002 * boost::units::si::meters, 3705856.3580439999 * boost::units::si::meters },{ 414375.22005599999 * boost::units::si::meters, 3705857.3472480001 * boost::units::si::meters },{ 414375.22346299997 * boost::units::si::meters, 3705858.3364519998 * boost::units::si::meters },{ 414372.22438999999 * boost::units::si::meters, 3705858.3764769998 * boost::units::si::meters },{ 414372.21688199998 * boost::units::si::meters, 3705857.4250099999 * boost::units::si::meters },{ 414372.20937400003 * boost::units::si::meters, 3705856.4735440002 * boost::units::si::meters },{ 414372.20186600002 * boost::units::si::meters, 3705855.5220769998 * boost::units::si::meters },{ 414372.19435800001 * boost::units::si::meters, 3705854.5706099998 * boost::units::si::meters },{ 414372.18685100001 * boost::units::si::meters, 3705853.6191440001 * boost::units::si::meters },{ 414372.179343 * boost::units::si::meters, 3705852.6676770002 * boost::units::si::meters },{ 414372.17183499999 * boost::units::si::meters, 3705851.716211 * boost::units::si::meters },{ 414372.16996000003 * boost::units::si::meters, 3705850.8003230002 * boost::units::si::meters },{ 414372.16808600002 * boost::units::si::meters, 3705849.8844349999 * boost::units::si::meters },{ 414372.166211 * boost::units::si::meters, 3705848.9685479999 * boost::units::si::meters },{ 414372.16433599999 * boost::units::si::meters, 3705848.05266 * boost::units::si::meters },{ 414372.16246199998 * boost::units::si::meters, 3705847.1367720002 * boost::units::si::meters },{ 414372.16058700002 * boost::units::si::meters, 3705846.220884 * boost::units::si::meters },{ 414372.158712 * boost::units::si::meters, 3705845.3049960001 * boost::units::si::meters },{ 414372.156838 * boost::units::si::meters, 3705844.3891090001 * boost::units::si::meters },{ 414372.15496299998 * boost::units::si::meters, 3705843.4732209998 * boost::units::si::meters },{ 414372.158811 * boost::units::si::meters, 3705842.5547440001 * boost::units::si::meters } };

	//auto originA = point2{ 414348.862273 * units::si::meters, 3705824.230245 * units::si::meters };
	//auto originB = point2{ 414368.746286 * units::si::meters, 3705860.557236 * units::si::meters };
	//auto originA3 = point3{ 414348.862273 * units::si::meters, 3705824.230245 * units::si::meters, 350.0 * units::si::meters };
	//auto originB3 = point3{ 414368.746286 * units::si::meters, 3705860.557236 * units::si::meters, 350.0 * units::si::meters };

	auto UTMtoA = vector3{ -414348.862273 * units::si::meters, -3705824.230245 * units::si::meters, 0.0 * units::si::meters };
	auto UTMtoB = vector3{ -414368.746286 * units::si::meters, -3705860.557236 * units::si::meters, 0.0 * units::si::meters };

	auto roll = 0.091437 * (constants::pi<units::angle>() / 180.0);
	auto pitch = 0.312962 * (constants::pi<units::angle>() / 180.0);
	auto yaw = -0.089251 * (constants::pi<units::angle>() / 180.0);
	auto v = vector3{ -19.286012 * units::si::meters, -38.724455 * units::si::meters, -3.590890 * units::si::meters };
	//auto ba = vector3{ originA3 - originB3 };

	using xform_t = transformer<3, post_multiplication_matrix_concatenation_policy>;

	auto rZ = rotate3_z(yaw);
	auto rY = rotate3_y(pitch);
	auto rX = rotate3_x(roll);

	auto rot_transpose = xform_t{ transpose(rX * rY * rZ) };
	vector3 neg_rot_transpose_trans = -rot_transpose(v);
	auto RT = xform_t{ assign_translation(rot_transpose.matrix(), neg_rot_transpose_trans) };
	auto xform = xform_t{ translate3(-UTMtoA) * RT.matrix() * translate3(UTMtoB) };

	// correct pos_wrt_a = [8.2, -17.9, 348.4]
	// correct pos_wrt_b = [25.531, 21.429, 352.102]
	auto pos_wrt_a = point3{ 8.2 * units::si::meters, -17.9 * units::si::meters, 348.4 * units::si::meters };

	auto pos_wrt_b = RT(pos_wrt_a);
	EXPECT_TRUE(numeric_sequence_equals(pos_wrt_b, point3{ 25.52980003* units::si::meters, 21.42595666* units::si::meters, 352.10209189* units::si::meters }, make_tolerance_policy(1e-5)));

	auto bl_UTM = point3{ 414372.158811* units::si::meters, 3705842.554744* units::si::meters, 348.303791 * units::si::meters };
	auto tl_UTM = point3{ 414369.606051* units::si::meters, 3705845.549275* units::si::meters, 351.979645 * units::si::meters };
	//auto bl_UTM_proj = point3{ 414372.158811* units::si::meters, 3705842.554744* units::si::meters, /*348.303791*/0.0 * units::si::meters };
	//auto tl_UTM_proj = point3{ 414369.606051* units::si::meters, 3705845.549275* units::si::meters, /*351.979645*/0.0 * units::si::meters };
	//auto bl_wrt_b = point3{ bl_UTM - originB3 };
	//auto tl_wrt_a = point3{ tl_UTM - originA3 };

	auto tl_UTM_calc = xform(bl_UTM);
	EXPECT_TRUE(numeric_sequence_equals(tl_UTM_calc, tl_UTM, make_tolerance_policy(1e-2)));

	auto result1 = make_polygon2(xform(make_polygon3(B, 350.0 * units::si::meters)));

	auto B3 = make_polygon3(B, 350.0 * units::si::meters);
/*
	auto xformer = [](const geometrix::segment<stk::point3>& geometryA, const stk::vector3& utmToA, const stk::vector3& bToUTM, const stk::vector3& v, const stk::units::angle& roll, const stk::units::angle& pitch, const stk::units::angle& yaw) ->stk::transformer2
	{
		using xform_t = transformer<3, post_multiplication_matrix_concatenation_policy>;

		auto rot_transpose = xform_t{ trans(rotate3_x(roll) * rotate3_y(pitch) * rotate3_z(yaw)) };
		auto rv = vector3{ -rot_transpose(v) };
		auto xform = xform_t{ translate3(bToUTM) * assign_translation(rot_transpose.matrix(), rv) * translate3(utmToA) };

		//! Estimate a 1 meter segment from originA along the x/y diagonal slope 1.
		auto geometryAwrtB = xform(geometryA);

		auto ra = segment2{ geometryA.get_start(), geometryA.get_end() };
		auto rb = segment2{ geometryAwrtB.get_start(), geometryAwrtB.get_end() };

		auto orientationA = normalize(geometryA.get_end() - geometryA.get_start());
		auto orientationB = normalize(geometryAwrtB.get_end() - geometryAwrtB.get_start());
		
		return transformer2().translate(vector2{ bToUTM }).translate(rb.get_start() - bToUTM).rotate(dimensionless2{ orientationA[0], orientationA[1] }, dimensionless2{ orientationB[0], orientationB[1] }).translate(-as_vector(ra.get_start() + utmToA)).translate(vector2{ utmToA });
	};
*/
	auto xform2 = estimate_2d_transformer(segment<point3>{ B3[0], B3[1] }, UTMtoB, -UTMtoA, v, roll, pitch, yaw);
	auto result3 = xform2(B);

	EXPECT_TRUE(true);
}

TEST(TransformerTestSuite, test3DTransformPrePost)
{
	using namespace geometrix;
	using namespace stk;

	using postxform_t = transformer<2, post_multiplication_matrix_concatenation_policy>;
	using prexform_t = transformer<2, pre_multiplication_matrix_concatenation_policy>;

	auto theta = constants::half_pi<units::angle>() * 0.5;

	auto prexform = prexform_t{ prexform_t().rotate(theta) };
	auto postxform = postxform_t{ postxform_t().rotate(theta) };

	auto seg = segment2{ point2{ 1.0 * units::si::meters, 0.0 * units::si::meters }, point2{ 2.0 * units::si::meters, 0.0 * units::si::meters } };
	auto spre = prexform(seg);
	auto spost = postxform(seg);

}

TEST(TransformerTestSuite, testFindRotationAxisThetaEqualsPi_4)
{
	using namespace geometrix;
	using namespace stk;

	using xform_t = transformer<3, post_multiplication_matrix_concatenation_policy>;

	auto theta = constants::half_pi<units::angle>() * 0.5;

	auto m = rotate3_y(theta);

	auto v = rotation_axis_of(m);
	EXPECT_TRUE(numeric_sequence_equals(v, vector<double, 3>(0.0, 2.0 * sin(theta), 0.0), make_tolerance_policy()));
	auto a = rotation_angle_of(m);
	EXPECT_TRUE(make_tolerance_policy().equals(theta, a));
}

TEST(TransformerTestSuite, testFindRotationAxisThetaEqualsPi)
{
	using namespace geometrix;
	using namespace stk;

	using xform_t = transformer<3, post_multiplication_matrix_concatenation_policy>;

	auto theta = constants::pi<units::angle>();

	auto m = rotate3_y(theta);

	auto v = rotation_axis_of(m);
	EXPECT_TRUE(numeric_sequence_equals(v, vector<double, 3>(0.0, 2.0 * sin(theta), 0.0), make_tolerance_policy()));
	auto a = rotation_angle_of(m);
	EXPECT_TRUE(make_tolerance_policy().equals(theta, a));
}

TEST(TransformerTestSuite, polygon_with_holesTest)
{
	using namespace geometrix;
	using namespace stk;
	
	using xform_t = transformer<2, post_multiplication_matrix_concatenation_policy>;

	polygon2 pgon = polygon2{ { 10 * units::si::meters, 5.25 * units::si::meters },{ 10 * units::si::meters, 10 * units::si::meters },{ 0 * units::si::meters, 10 * units::si::meters },{ 0 * units::si::meters, 0 * units::si::meters },{ 10 * units::si::meters, 0 * units::si::meters },{ 10 * units::si::meters, 4.75 * units::si::meters },{ 1 * units::si::meters, 4.75 * units::si::meters },{ 1 * units::si::meters, 5.25 * units::si::meters } };
	polygon2 hole = { { 0.5 * units::si::meters, 0.5 * units::si::meters },{ 0.5 * units::si::meters, 1 * units::si::meters },{ 1 * units::si::meters, 1 * units::si::meters },{ 1 * units::si::meters, 0.5 * units::si::meters } };
	polygon_with_holes2 geometry{ pgon, std::vector<polygon2>{hole} };

	auto xform = xform_t{};
	xform.translate(vector2{ 1.0 * units::si::meters, 1.0 * units::meters });

	auto r = xform(geometry);

	EXPECT_TRUE(true);

	auto xform2 = xform;
	xform2.inverse();

	auto og = xform2(r);

	EXPECT_TRUE(true);

}

#include <geometrix/utility/scope_timer.ipp>
std::size_t nRuns = 10000000;
TEST( TransformerTestSuite, timer_point_transformer_test )
{
	using namespace geometrix;
	using namespace stk;

	using xform_t = transformer<2, post_multiplication_matrix_concatenation_policy>;


	auto xform = xform_t{};
	auto v = vector2{ 1.0 * units::si::meters, 1.0 * units::meters };
	xform.translate( v );
	auto xform2 = xform;
	xform2.inverse();

	std::vector<point2> results( nRuns );
	auto p = point2 { 10 * units::si::meters, 5.25 * units::si::meters };
	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "transformer2::xform" );
		for( auto i = 0ULL; i < nRuns; ++i )
			results[i] = xform( p );
	}
	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "transformer2::xform2" );
		for( auto i = 0ULL; i < nRuns; ++i )
			results[i] = xform2( p );
	}
	
	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "transformer2::raw" );
		for( auto i = 0ULL; i < nRuns; ++i )
			results[i] = p - v;
	}
	{
		GEOMETRIX_MEASURE_SCOPE_TIME( "transformer2::raw2" );
		for( auto i = 0ULL; i < nRuns; ++i )
			results[i] = p + v;
	}
	EXPECT_TRUE(true);
}

