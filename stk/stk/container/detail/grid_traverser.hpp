#include <boost/mpl/if.hpp>
#include <boost/concept_check.hpp>
#include <cstdint>

namespace stk {
	namespace detail {
		template <unsigned int IndexI, unsigned int IndexJ>
		struct cell_iterator
		{
			static const int I = IndexI;
			static const int J = IndexJ;
		};

		template <typename Iter, unsigned int Side>
		struct next
			: boost::mpl::if_c
			  <
				  ((Iter::I + 1) > Side)
			    , cell_iterator< 0, Iter::J + 1 >
			    , cell_iterator< Iter::I + 1, Iter::J >
			  >
		{};

		template <typename Iter, unsigned int Side>
		struct traverse
		{
			template <typename Visitor>
			static void apply( int i, int j, Visitor&& v )
			{
				int inext = i + Iter::I;
				int jnext = j + Iter::J;
				v( inext, jnext );
				traverse<typename next<Iter, Side>::type, Side>::template apply( i, j, std::forward<Visitor>( v ) );
			}
		};

		template <unsigned int Side>
		struct traverse < cell_iterator<Side,Side>, Side >
		{
			template <typename Visitor>
			static void apply( int i, int j, Visitor&& v )
			{
				int inext = i + Side;
				int jnext = j + Side;
				v( inext, jnext );
			}
		};
	}//! namespace detail;

	template <unsigned int Rank>
	struct grid_traverser
	{
	private:

		static const int Side = Rank * 2;

	public:

		template <typename Visitor>
		static void apply( int i, int j, Visitor&& v )
		{
			using namespace detail;
			traverse< cell_iterator<0, 0>, Side >::template apply( i - Rank, j - Rank, v );
		}

	};

	template <unsigned int Rank, typename Grid, typename Point, typename Visitor>
	inline void visit_cells( Grid& grid, const Point& p, Visitor&& v )
	{
		BOOST_CONCEPT_ASSERT(( boost::BinaryFunction< Visitor, void, int, int > ));
		using namespace geometrix;
		grid_traverser<Rank>::template apply( grid.get_traits().get_x_index( get<0>( p ) ), grid.get_traits().get_y_index( get<1>( p ) ), v );
	}

    //! TODO: Need to make this better... cell-polygon intersection is heavy.
	template <typename Grid, typename Polygon, typename Visitor>
	inline void visit_overlapped_cells(Grid& grid, const Polygon& pgon, Visitor&& v)
	{
		using namespace geometrix;
		auto bounds = get_bounds(pgon, make_tolerance_policy());
		auto traits = grid.get_traits();
		auto imin = traits.get_x_index((std::max)(std::get<e_xmin>(bounds), traits.get_min_x()));
		auto imax = traits.get_x_index((std::min)(std::get<e_xmax>(bounds), traits.get_max_x()));
		auto jmin = traits.get_y_index((std::max)(std::get<e_ymin>(bounds), traits.get_min_y()));
		auto jmax = traits.get_y_index((std::min)(std::get<e_ymax>(bounds), traits.get_max_y()));

		for (auto i = imin; i <= imax; ++i)
			for (auto j = jmin; j <= jmax; ++j)
				v(i, j);
	}

	struct Index
    {
        Index()
            : v{}
        {}

        std::int64_t v;
    };
}//! namespace stk;

