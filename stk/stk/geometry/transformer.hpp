//
//! Copyright © 2017
//! Brandon Kohn
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef STK_GEOMETRY_TRANSFORMER_HPP
#define STK_GEOMETRY_TRANSFORMER_HPP

#include <geometrix/tensor/matrix.hpp>
#include <geometrix/tags.hpp>
#include <geometrix/tensor/homogeneous_adaptor.hpp>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/concept/assert.hpp>
#include <type_traits>

namespace stk {
    
    namespace detail {
        template <unsigned int N, unsigned int D>
        struct identity_setter
        {
            BOOST_STATIC_CONSTANT(unsigned int, R = N / D);
            BOOST_STATIC_CONSTANT(unsigned int, C = N % D);
            
            template <typename Matrix>
            static void apply(Matrix& m)
            {
                m[R][C] = get<typename geometrix::type_at<Matrix, R, C>::type, R, C>();
                identity_setter<N-1, D>::apply(m);
            }
            
        private:

            template <typename T, unsigned int I, unsigned int J>
            static T get(typename std::enable_if<I != J>::type * = nullptr)
            {
                return geometrix::constants::zero<T>();
            }

            template <typename T, unsigned int I, unsigned int J>
            static T get(typename std::enable_if<I == J>::type * = nullptr)
            {
                return geometrix::constants::one<T>();
            }

        };
        
        template <unsigned int D>
        struct identity_setter<0, D>
        {
            template <typename Matrix>
            static void apply(Matrix& m)
            {
                m[0][0] = geometrix::constants::one<typename geometrix::type_at<Matrix,0,0>::type>();
            }
        };
    }//! namespace detail;
    
	inline geometrix::matrix<double, 3, 3> translate2(const vector2& v)
	{
		using namespace geometrix;
		using m_t = geometrix::matrix<double, 3, 3>;

		return m_t
		{
			  1.0, 0.0, construct<double>(get<0>(v))
			, 0.0, 1.0, construct<double>(get<1>(v))
			, 0.0, 0.0, 1.0
		};
	}
	
	inline geometrix::matrix<double, 3, 3> rotate2(const units::angle& yaw)
	{
		using namespace geometrix;
		using std::cos;
		using std::sin;
		using m_t = geometrix::matrix<double, 3, 3>;

		auto sinw = sin(yaw);
		auto cosw = cos(yaw);
		return m_t
		{
			    cosw,   -sinw,  0
			,   sinw,   cosw,   0
			,   0,  0,  1
		};
	}

	inline geometrix::matrix<double,4,4> translate3(const vector3& v)
	{
		using namespace geometrix;
		using m_t = geometrix::matrix<double, 4, 4>;

		return m_t
			{
				  1.0, 0.0, 0.0, construct<double>(get<0>(v))
				, 0.0, 1.0, 0.0, construct<double>(get<1>(v))
				, 0.0, 0.0, 1.0, construct<double>(get<2>(v))
				, 0.0, 0.0, 0.0, 1.0
			};
	}

	inline geometrix::matrix<double,4,4> rotate3_x(const units::angle& roll)
	{
		using namespace geometrix;
		using std::cos;
		using std::sin;
		using namespace geometrix;
		using m_t = geometrix::matrix<double, 4, 4>;

		auto sinr = sin(roll);
		auto cosr = cos(roll);

		return m_t
		{
			    1,  0,  0,  0
			,   0,  cosr,   -sinr,  0
			,   0,  sinr,   cosr, 0
			,   0,  0,  0,  1
		};
	}

	inline geometrix::matrix<double, 4, 4> rotate3_y(const units::angle& pitch)
	{
		using namespace geometrix;
		using std::cos;
		using std::sin;
		using m_t = matrix<double, 4, 4>;

		auto sinp = sin(pitch);
		auto cosp = cos(pitch);
		return m_t
			{
				    cosp,   0,  sinp,   0
				,   0,  1,  0,  0
				,   -sinp,  0,  cosp, 0
				,   0,  0,  0,  1
			};
	}

	inline geometrix::matrix<double, 4, 4> rotate3_z(const units::angle& yaw)
	{
		using namespace geometrix;
		using std::cos;
		using std::sin;
		using m_t = geometrix::matrix<double, 4, 4>;

		auto sinw = sin(yaw);
		auto cosw = cos(yaw);
		return m_t
			{
				    cosw,   -sinw,  0,  0
				,   sinw,   cosw,   0,  0
				,   0,  0,  1,  0
				,   0,  0,  0,  1
			};
	}

	struct pre_multiplication_matrix_concatenation_policy
	{
		template <typename A, typename B>
		A operator()(const A& a, const B& b) const
		{
			return geometrix::construct<A>(b * a);
		}
	};

	struct post_multiplication_matrix_concatenation_policy
	{
		template <typename A, typename B>
		A operator()(const A& a, const B& b) const
		{
			return geometrix::construct<A>(a * b);
		}
	};
	
	struct column_vector_multiplication_transformation_policy
	{
		template <typename Result, typename A, typename B>
		Result apply(const A& a, const B& b) const
		{
			return geometrix::construct<Result>(a * b);
		}
	};

	template <unsigned int D>
	class transformer_base
	{
	public:
		BOOST_STATIC_CONSTANT(unsigned int, dimensionality = D + 1);
		using element_type = double;
		using transform_matrix = geometrix::matrix<element_type, dimensionality, dimensionality>;
		
		transformer_base()
		{
			reset();
		}

		template <typename Matrix>
		transformer_base(const Matrix& m)
			: m_transform(geometrix::construct<transform_matrix>(m))
		{}

		void reset()
		{
			detail::identity_setter<dimensionality * dimensionality - 1, dimensionality>::apply(m_transform);
		}
		
		transform_matrix& matrix() { return m_transform; }
		transform_matrix const& matrix() const { return m_transform; }
		
		transform_matrix m_transform;
	};

	template <unsigned int D, typename MatrixConcatenationPolicy>
	struct transformer_operation_layer;

	template <typename MatrixConcatenationPolicy>
	struct transformer_operation_layer<2, MatrixConcatenationPolicy> : transformer_base<2>
	{
		transformer_operation_layer() = default;

		template <typename Matrix>
		transformer_operation_layer(const Matrix& m)
			: transformer_base(m)
		{}

		transformer_operation_layer& translate(const vector2& v)
		{
			using namespace geometrix;

			transform_matrix m =
			{
			      1.0, 0.0, construct<double>(get<0>(v))
				, 0.0, 1.0, construct<double>(get<1>(v))
				, 0.0, 0.0, 1.0
			};

			m_transform = MatrixConcatenationPolicy()(m_transform, m);

			return *this;
		}
		
		transformer_operation_layer& rotate(const point2& origin, const units::angle& theta)
		{
			using namespace geometrix;
			using std::cos;
			using std::sin;

			transform_matrix t =
			{
				1.0, 0.0, construct<double>(get<0>(origin))
				, 0.0, 1.0, construct<double>(get<1>(origin))
				, 0.0, 0.0, 1.0
			};

			auto sint = sin(theta);
			auto cost = cos(theta);
			transform_matrix r =
			{
				cost, -sint, 0.0
				, sint, cost, 0.0
				, 0.0, 0.0, 1.0
			};

			transform_matrix tp =
			{
				1.0, 0.0, -construct<double>(get<0>(origin))
				, 0.0, 1.0, -construct<double>(get<1>(origin))
				, 0.0, 0.0, 1.0
			};

			m_transform = construct<transform_matrix>(m_transform * t * r * tp);

			return *this;
		}
		
		transformer_operation_layer& rotate(const units::angle& theta)
		{
			using namespace geometrix;
			using std::cos;
			using std::sin;

			auto sint = sin(theta);
			auto cost = cos(theta);
			transform_matrix r =
			{
				  cost, -sint, 0.0
				, sint, cost, 0.0
				, 0.0, 0.0, 1.0
			};

			m_transform = MatrixConcatenationPolicy()(m_transform, r);

			return *this;
		}

		transformer_operation_layer& rotate(const dimensionless2& a, const dimensionless2& b)
		{
			using namespace geometrix;

			auto sint = exterior_product_area(a, b);
			auto cost = dot_product(a, b);
			transform_matrix r =
			{
				  cost, -sint, 0.0
				, sint, cost, 0.0
				, 0.0, 0.0, 1.0
			};

			m_transform = MatrixConcatenationPolicy()(m_transform, r);

			return *this;
		}
	};

	template <typename MatrixConcatenationPolicy>
	struct transformer_operation_layer<3, MatrixConcatenationPolicy> : transformer_base<3>
	{
		transformer_operation_layer() = default;

		template <typename Matrix>
		transformer_operation_layer(const Matrix& m)
			: transformer_base(m)
		{}

		transformer_operation_layer& translate(const vector3& v)
		{
			using namespace geometrix;

			transform_matrix m =
			{
				  1.0, 0.0, 0.0, get<0>(v).value()
				, 0.0, 1.0, 0.0, get<1>(v).value()
				, 0.0, 0.0, 1.0, get<2>(v).value()
				, 0.0, 0.0, 0.0, 1.0
			};

			m_transform = MatrixConcatenationPolicy()(m_transform, m);

			return *this;
		}

		transformer_operation_layer& rotate_x(const units::angle& roll)
		{
			using namespace geometrix;
			using std::cos;
			using std::sin;

			auto sinr = sin(roll);
			auto cosr = cos(roll);

			transform_matrix r =
			{
				    1,  0,  0,  0
				,   0,  cosr,   -sinr,  0
				,   0,  sinr,   cosr, 0
				,   0,  0,  0,  1
			};

			m_transform = MatrixConcatenationPolicy()(m_transform, r);

			return *this;
		}

		transformer_operation_layer& rotate_y(const units::angle& pitch)
		{
			using namespace geometrix;
			using std::cos;
			using std::sin;
			auto sinp = sin(pitch);
			auto cosp = cos(pitch);
			transform_matrix r =
			{
				    cosp,   0,  sinp,   0
				,   0,  1,  0,  0
				,   -sinp,  0,  cosp, 0
				,   0,  0,  0,  1
			};

			m_transform = MatrixConcatenationPolicy()(m_transform, r);

			return *this;
		}

		transformer_operation_layer& rotate_z(const units::angle& yaw)
		{
			using namespace geometrix;
			using std::cos;
			using std::sin;

			auto sinw = sin(yaw);
			auto cosw = cos(yaw);
			transform_matrix r =
			{
				    cosw,   -sinw,  0,  0
				,   sinw,   cosw,   0,  0
				,   0,  0,  1,  0
				,   0,  0,  0,  1
			};

			m_transform = MatrixConcatenationPolicy()(m_transform, r);

			return *this;
		}
	};
	
	template <std::size_t I>
	inline geometrix::vector<double, 3> rotation_axis_of(const geometrix::matrix<double, I, I>& m)
	{
		GEOMETRIX_STATIC_ASSERT(I == 3 || I == 4);//! This is either a 3x3 matrix for 3D rotation or a 4x4 homogeneous matrix with 3D rotation.
		return geometrix::vector<double, 3>{ m[2][1] - m[1][2], m[0][2] - m[2][0], m[1][0] - m[0][1] };
	}

	namespace detail {
		template <std::size_t I>
		inline double trace_to_cos_theta(const geometrix::matrix<double, I, I>& m)
		{
			GEOMETRIX_STATIC_ASSERT(I == 2 || I == 3);
			using namespace geometrix;
			return trace(m) - 1.0;
		}

		template <>
		inline double trace_to_cos_theta<4>(const geometrix::matrix<double, 4, 4>& m)
		{
			using namespace geometrix;
			return trace(m) - 1.0 - get<3,3>(m);
		}
	}
	template <std::size_t I>
	inline units::angle rotation_angle_of(const geometrix::matrix<double, I, I>& m)
	{
		using namespace geometrix;
		GEOMETRIX_STATIC_ASSERT(I == 2 || I == 3 || I == 4);//! This is either a 2x2 matrix for 2D rotation, 3x3 matrix for 3D rotation or a 4x4 homogeneous matrix with 3D rotation.
		auto cosTheta = detail::trace_to_cos_theta(m);
		auto sinTheta = /*0.5 **/ magnitude(rotation_axis_of(m));
		using std::atan2;
		return atan2(sinTheta, cosTheta) * units::si::radians;
	}

	template <unsigned int D, typename MatrixConcatenationPolicy = post_multiplication_matrix_concatenation_policy, typename TransformApplicationPolicy = column_vector_multiplication_transformation_policy>
	class transformer : public transformer_operation_layer<D, MatrixConcatenationPolicy>
	{
		using base_t = transformer_operation_layer<D, MatrixConcatenationPolicy>;
		using transform_matrix = typename base_t::transform_matrix;

	public:

		transformer() = default;

		template <typename Matrix>
		transformer(const Matrix& m)
			: base_t(m)
		{
			using namespace geometrix;
			BOOST_CONCEPT_ASSERT((SquareMatrixConcept<Matrix>()));
		}

		transformer(const base_t& ops)
			: base_t(ops)
		{}

		template <typename Geometry>
		Geometry operator()(const Geometry& p) const
		{
			using namespace geometrix;
			static_assert(dimension_of<Geometry>::value == D, "transform called with incompatible geometry dimensions");
			return transform(p, typename geometrix::geometry_tag_of<Geometry>::type());
		}

		transformer& transpose()
		{
			using namespace geometrix;
			base_t::m_transform = construct<transform_matrix>(trans(base_t::m_transform));
			return *this;
		}

		transformer& negate()
		{
			using namespace geometrix;
			base_t::m_transform = construct<transform_matrix>(-base_t::m_transform);
			return *this;
		}

	private:

		template <typename Point>
		Point transform(const Point& p, geometrix::geometry_tags::point_tag) const
		{
			using namespace geometrix;
			return TransformApplicationPolicy().template apply<Point>(base_t::m_transform, as_positional_homogeneous<typename arithmetic_type_of<Point>::type>(p));
		}

		template <typename Vector>
		Vector transform(const Vector& v, geometrix::geometry_tags::vector_tag) const
		{
			using namespace geometrix;
			return TransformApplicationPolicy().template apply<Vector>(base_t::m_transform, as_vectoral_homogeneous<typename arithmetic_type_of<Vector>::type>(v));
		}

		template <typename Segment>
		Segment transform(const Segment& s, geometrix::geometry_tags::segment_tag) const
		{
			using namespace geometrix;
			return construct<Segment>(transform(get_start(s), geometry_tags::point_tag()), transform(get_end(s), geometry_tags::point_tag()));
		}

		template <typename Polyline>
		Polyline transform(const Polyline& p, geometrix::geometry_tags::polyline_tag) const
		{
			using namespace geometrix;
			using namespace boost::adaptors;
			using point_t = typename point_sequence_traits<Polyline>::point_type;

			Polyline result;
			boost::copy(p | transformed([this](const point_t& p) { return transform(p, geometry_tags::point_tag()); }), std::back_inserter(result));

			return result;
		}

		template <typename Polygon>
		Polygon transform(const Polygon& p, geometrix::geometry_tags::polygon_tag) const
		{
			using namespace geometrix;
			using namespace boost::adaptors;
			using point_t = typename point_sequence_traits<Polygon>::point_type;

			Polygon result;
			boost::copy(p | transformed([this](const point_t& p) { return transform(p, geometry_tags::point_tag()); }), std::back_inserter(result));

			return result;
		}
	};

	using transformer2 = transformer<2, post_multiplication_matrix_concatenation_policy>;
	using transformer3 = transformer<3, post_multiplication_matrix_concatenation_policy>;

}//! namespace stk;

#endif//! STK_GEOMETRY_TRANSFORMER_HPP
