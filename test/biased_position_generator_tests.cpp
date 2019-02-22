
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <poly2tri/poly2tri.h>

#include <geometrix/algorithm/point_sequence/is_polygon_simple.hpp>
#include <geometrix/algorithm/polygon_with_holes_as_segment_range.hpp>
#include <geometrix/algorithm/hyperplane_partition_policies.hpp>
#include <geometrix/algorithm/solid_leaf_bsp_tree.hpp>
#include <geometrix/algorithm/distance/point_segment_closest_point.hpp>
#include <geometrix/algorithm/point_in_polygon.hpp>
#include <geometrix/utility/random_generator.hpp>
#include <geometrix/algorithm/intersection/polyline_polyline_intersection.hpp>

#include <stk/geometry/geometry_kernel.hpp>
#include <stk/geometry/space_partition/rtree_triangle_cache.ipp>
#include <stk/utility/scope_exit.hpp>
#include <stk/geometry/clipper_boolean_operations.hpp>
#include <stk/geometry/space_partition/biased_position_generator.hpp>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/dynamic_bitset.hpp>
#include <exception>

#include <geometrix/utility/scope_timer.ipp>

template <typename Segment, typename Polygons>
inline std::vector< Segment > polygon_collection_as_segment_range(const Polygons& pgons)
{
	using namespace geometrix;
	std::vector< Segment > segments;
	for (const auto& p : pgons) 
	{
		using access = point_sequence_traits<typename std::decay<decltype(p)>::type>;
		std::size_t size = access::size(p);
		for (std::size_t i = 0, j = 1; i < size; ++i, j = (j + 1) % size) 
		{
			Segment segment = construction_policy< Segment >::construct(access::get_point(p, i), access::get_point(p, j));
			segments.push_back(segment);
		}
	}

	return std::move(segments);
}

template <typename Polygon>
inline stk::polyline2 to_polyline(Polygon&& pgon)
{
	stk::polyline2 r(pgon.begin(), pgon.end());
	r.push_back(pgon.front());
	return std::move(r);
}

template <typename Polygon>
inline bool is_self_intersecting(const Polygon& outer, const std::vector<Polygon>& holes)
{
	using namespace stk;
	using namespace geometrix;
	std::vector<polyline2> subjects;
	subjects.emplace_back(to_polyline(outer));
	for(const auto& hole : holes)
		subjects.emplace_back(to_polyline(hole));

	for (auto i = 0UL; i < subjects.size(); ++i) 
	{
		for (auto j = i + 1; j < subjects.size(); ++j) 
		{
			auto null_visitor = [](intersection_type iType, std::size_t, std::size_t, std::size_t, std::size_t, point2, point2)
			{
				return iType != e_non_crossing;
			};
			if (polyline_polyline_intersect(subjects[i], subjects[j], null_visitor, make_tolerance_policy()))
				return true;
		}
	}
	return false;
}

inline bool is_self_intersecting(const stk::polygon_with_holes2& pgon)
{
	return is_self_intersecting(pgon.get_outer(), pgon.get_holes());
}

#include <boost/range/algorithm_ext/erase.hpp>
TEST(biased_position_generator_test_suite, polygon_with_holes_test)
{
	using namespace stk;

	//! This geometry is from the pedestrian tutorial model. pouter is a pedestrian area.
	auto pouter = polygon2{ {60.932305376161821 * boost::units::si::meters, 2148.1332399388775 * boost::units::si::meters}, {-71.142922936356626 * boost::units::si::meters, 2113.9137489674613 * boost::units::si::meters}, {-49.530612848873716 * boost::units::si::meters, 1976.4354431331158 * boost::units::si::meters}, {-14.710779930115677 * boost::units::si::meters, 1992.0443337513134 * boost::units::si::meters}, {21.910078829270788 * boost::units::si::meters, 1942.2159521607682 * boost::units::si::meters}, {83.745299357397016 * boost::units::si::meters, 1988.4422820704058 * boost::units::si::meters}, {131.77265510737197 * boost::units::si::meters, 2051.4781864918768 * boost::units::si::meters}, {77.741879888635594 * boost::units::si::meters, 2097.1041744546965 * boost::units::si::meters} };
	auto scale = 1000UL;
	auto r0 = clipper_offset(pouter, 0.2*units::si::meters, scale);

	auto outer = r0[0].get_outer();

	//! The holes are the buildings, obstacles, streets, and crosswalks from the pedestrian tutorial model.
	std::vector<polygon2> pholes = 
	{
	    polygon2{ {49.985745427373331 * boost::units::si::meters, 2034.2638289444149 * boost::units::si::meters}, {50.136921647703275 * boost::units::si::meters, 2034.2973980205134 * boost::units::si::meters}, {50.524841590202413 * boost::units::si::meters, 2034.2564682504162 * boost::units::si::meters}, {50.992013334471267 * boost::units::si::meters, 2034.096297416836 * boost::units::si::meters}, {51.749917804030702 * boost::units::si::meters, 2033.6976941125467 * boost::units::si::meters}, {52.450980976514984 * boost::units::si::meters, 2033.1985325450078 * boost::units::si::meters}, {52.817101525783073 * boost::units::si::meters, 2032.8595914645121 * boost::units::si::meters}, {53.053444675693754 * boost::units::si::meters, 2032.550506323576 * boost::units::si::meters}, {53.109885400743224 * boost::units::si::meters, 2032.4164782930166 * boost::units::si::meters}, {53.118204585742205 * boost::units::si::meters, 2032.3010965222493 * boost::units::si::meters}, {53.073176500678528 * boost::units::si::meters, 2032.2080884370953 * boost::units::si::meters}, {52.969575415656436 * boost::units::si::meters, 2032.141181461513 * boost::units::si::meters}, {56.324344530061353 * boost::units::si::meters, 2027.0103307683021 * boost::units::si::meters}, {57.41496527538402 * boost::units::si::meters, 2027.3689513867721 * boost::units::si::meters}, {58.512837049376685 * boost::units::si::meters, 2027.6243591913953 * boost::units::si::meters}, {59.616195663053077 * boost::units::si::meters, 2027.7818286223337 * boost::units::si::meters}, {60.723276927543338 * boost::units::si::meters, 2027.8466341150925 * boost::units::si::meters}, {61.832316653744783 * boost::units::si::meters, 2027.8240501079708 * boost::units::si::meters}, {62.941550652729347 * boost::units::si::meters, 2027.7193510383368 * boost::units::si::meters}, {64.049214735685382 * boost::units::si::meters, 2027.5378113444895 * boost::units::si::meters}, {65.153544713335577 * boost::units::si::meters, 2027.2847054665908 * boost::units::si::meters}, {66.2527763969847 * boost::units::si::meters, 2026.9653078373522 * boost::units::si::meters}, {67.345145597471856 * boost::units::si::meters, 2026.5848928978667 * boost::units::si::meters}, {68.42888812610181 * boost::units::si::meters, 2026.1487350864336 * boost::units::si::meters}, {69.50223979371367 * boost::units::si::meters, 2025.6621088394895 * boost::units::si::meters}, {70.563436411262956 * boost::units::si::meters, 2025.1302885934711 * boost::units::si::meters}, {71.610713789763395 * boost::units::si::meters, 2024.5585487876087 * boost::units::si::meters}, {72.642307740577962 * boost::units::si::meters, 2023.9521638602018 * boost::units::si::meters}, {73.656454074603971 * boost::units::si::meters, 2023.3164082504809 * boost::units::si::meters}, {74.651388602622319 * boost::units::si::meters, 2022.6565563911572 * boost::units::si::meters}, {75.62534713576315 * boost::units::si::meters, 2021.977882723324 * boost::units::si::meters}, {76.576565485389438 * boost::units::si::meters, 2021.2856616852805 * boost::units::si::meters}, {84.172147638513707 * boost::units::si::meters, 2026.8619163222611 * boost::units::si::meters}, {84.066382810997311 * boost::units::si::meters, 2026.9828373575583 * boost::units::si::meters}, {84.035860249830876 * boost::units::si::meters, 2027.1265205265954 * boost::units::si::meters}, {84.073506360407919 * boost::units::si::meters, 2027.2889139046893 * boost::units::si::meters}, {84.172247547772713 * boost::units::si::meters, 2027.4659655634314 * boost::units::si::meters}, {84.524720774323214 * boost::units::si::meters, 2027.8478360204026 * boost::units::si::meters}, {85.036691171349958 * boost::units::si::meters, 2028.2397164842114 * boost::units::si::meters}, {85.651569980895147 * boost::units::si::meters, 2028.6091915396973 * boost::units::si::meters}, {86.312768445175607 * boost::units::si::meters, 2028.9238457782194 * boost::units::si::meters}, {86.963697805593256 * boost::units::si::meters, 2029.1512637790293 * boost::units::si::meters}, {87.547769304830581 * boost::units::si::meters, 2029.2590301372111 * boost::units::si::meters}, {87.797049369371962 * boost::units::si::meters, 2029.2579141296446 * boost::units::si::meters}, {88.008394184231292 * boost::units::si::meters, 2029.2147294310853 * boost::units::si::meters}, {88.174730154918507 * boost::units::si::meters, 2029.1254241140559 * boost::units::si::meters}, {95.240437288943212 * boost::units::si::meters, 2035.0057316524908 * boost::units::si::meters}, {93.894342225044966 * boost::units::si::meters, 2036.1233808947727 * boost::units::si::meters}, {92.575868515647016 * boost::units::si::meters, 2037.2728888932616 * boost::units::si::meters}, {91.284426768659614 * boost::units::si::meters, 2038.4528740141541 * boost::units::si::meters}, {90.019427591643762 * boost::units::si::meters, 2039.6619546180591 * boost::units::si::meters}, {88.780281592276879 * boost::units::si::meters, 2040.898749073036 * boost::units::si::meters}, {87.566399378003553 * boost::units::si::meters, 2042.161875740625 * boost::units::si::meters}, {86.37719155655941 * boost::units::si::meters, 2043.449952987954 * boost::units::si::meters}, {85.212068735680077 * boost::units::si::meters, 2044.761599175632 * boost::units::si::meters}, {84.070441522751935 * boost::units::si::meters, 2046.0954326698557 * boost::units::si::meters}, {82.951720525627024 * boost::units::si::meters, 2047.4500718349591 * boost::units::si::meters}, {81.85531635186635 * boost::units::si::meters, 2048.8241350362077 * boost::units::si::meters}, {80.780639609147329 * boost::units::si::meters, 2050.2162406379357 * boost::units::si::meters}, {79.727100904856343 * boost::units::si::meters, 2051.6250070016831 * boost::units::si::meters}, {78.69411084667081 * boost::units::si::meters, 2053.0490524927154 * boost::units::si::meters}, {77.681080042617396 * boost::units::si::meters, 2054.4869954772294 * boost::units::si::meters}, {76.687419100024272 * boost::units::si::meters, 2055.9374543186277 * boost::units::si::meters}, {75.712538626394235 * boost::units::si::meters, 2057.3990473803133 * boost::units::si::meters}, {74.75584922946291 * boost::units::si::meters, 2058.8703930266201 * boost::units::si::meters}, {73.816761517140549 * boost::units::si::meters, 2060.3501096228138 * boost::units::si::meters}, {66.158148265152704 * boost::units::si::meters, 2055.6229917854071 * boost::units::si::meters}, {66.230897388770245 * boost::units::si::meters, 2055.4179852604866 * boost::units::si::meters}, {66.209476337244269 * boost::units::si::meters, 2055.1981253055856 * boost::units::si::meters}, {66.103525260405149 * boost::units::si::meters, 2054.9676381507888 * boost::units::si::meters}, {65.92268430814147 * boost::units::si::meters, 2054.7307500233874 * boost::units::si::meters}, {65.676593630341813 * boost::units::si::meters, 2054.4916871460155 * boost::units::si::meters}, {65.374893376836553 * boost::units::si::meters, 2054.2546757478267 * boost::units::si::meters}, {64.643224742088933 * boost::units::si::meters, 2053.8037122935057 * boost::units::si::meters}, {63.804799602890853 * boost::units::si::meters, 2053.4116694722325 * boost::units::si::meters}, {62.936739158292767 * boost::units::si::meters, 2053.1123570976779 * boost::units::si::meters}, {62.116164606879465 * boost::units::si::meters, 2052.9395849779248 * boost::units::si::meters}, {61.747784916020464 * boost::units::si::meters, 2052.9112170822918 * boost::units::si::meters}, {61.420197148167063 * boost::units::si::meters, 2052.9271629303694 * boost::units::si::meters}, {61.143041452916805 * boost::units::si::meters, 2052.991648748517 * boost::units::si::meters}, {60.925957980391104 * boost::units::si::meters, 2053.1089007630944 * boost::units::si::meters}, {60.77858688053675 * boost::units::si::meters, 2053.2831452023238 * boost::units::si::meters}, {52.838457798701711 * boost::units::si::meters, 2049.045887815766 * boost::units::si::meters}, {53.107359866728075 * boost::units::si::meters, 2048.4395199753344 * boost::units::si::meters}, {53.288968406617641 * boost::units::si::meters, 2047.8331729816273 * boost::units::si::meters}, {53.387609166384209 * boost::units::si::meters, 2047.2290028976277 * boost::units::si::meters}, {53.407607894332614 * boost::units::si::meters, 2046.6291657760739 * boost::units::si::meters}, {53.353290338651277 * boost::units::si::meters, 2046.0358176771551 * boost::units::si::meters}, {53.228982247412205 * boost::units::si::meters, 2045.4511146591976 * boost::units::si::meters}, {53.039009369094856 * boost::units::si::meters, 2044.8772127786651 * boost::units::si::meters}, {52.787697451654822 * boost::units::si::meters, 2044.316268093884 * boost::units::si::meters}, {52.479372243396938 * boost::units::si::meters, 2043.7704366613179 * boost::units::si::meters}, {52.118359492335003 * boost::units::si::meters, 2043.241874538362 * boost::units::si::meters}, {51.708984947064891 * boost::units::si::meters, 2042.7327377861366 * boost::units::si::meters}, {51.255574355483986 * boost::units::si::meters, 2042.245182460174 * boost::units::si::meters}, {50.762453465664294 * boost::units::si::meters, 2041.7813646169379 * boost::units::si::meters}, {50.233948025968857 * boost::units::si::meters, 2041.3434403147548 * boost::units::si::meters}, {49.674383784818929 * boost::units::si::meters, 2040.9335656138137 * boost::units::si::meters}, {49.088086490228307 * boost::units::si::meters, 2040.5538965705782 * boost::units::si::meters}, {48.479381890094373 * boost::units::si::meters, 2040.2065892387182 * boost::units::si::meters}, {47.852595732954796 * boost::units::si::meters, 2039.8937996821478 * boost::units::si::meters}, {47.212053767056204 * boost::units::si::meters, 2039.6176839545369 * boost::units::si::meters} }
	,   polygon2{ {114.37519531045109 * boost::units::si::meters, 2066.1693747648969 * boost::units::si::meters}, {114.06835949217202 * boost::units::si::meters, 2066.0661975899711 * boost::units::si::meters}, {109.26176228665281 * boost::units::si::meters, 2065.1160425907001 * boost::units::si::meters}, {102.32626512745628 * boost::units::si::meters, 2065.8210978675634 * boost::units::si::meters}, {93.371781873749569 * boost::units::si::meters, 2067.9948091562837 * boost::units::si::meters}, {87.827018571901135 * boost::units::si::meters, 2066.9469834640622 * boost::units::si::meters}, {84.641937824315391 * boost::units::si::meters, 2063.7084856750444 * boost::units::si::meters}, {85.714715134177823 * boost::units::si::meters, 2059.0808878624812 * boost::units::si::meters}, {93.808180637599435 * boost::units::si::meters, 2049.7927328888327 * boost::units::si::meters}, {105.98676263546804 * boost::units::si::meters, 2038.9951209472492 * boost::units::si::meters}, {116.37157779769041 * boost::units::si::meters, 2031.2642725231126 * boost::units::si::meters}, {131.77265510737197 * boost::units::si::meters, 2051.4781864918768 * boost::units::si::meters} }
	,   polygon2{ {90.684332532691769 * boost::units::si::meters, 2010.7262738728896 * boost::units::si::meters}, {97.15864157024771 * boost::units::si::meters, 2006.0472937244922 * boost::units::si::meters}, {104.55473164940486 * boost::units::si::meters, 2015.7546619530767 * boost::units::si::meters}, {97.885072562436108 * boost::units::si::meters, 2020.5748214917257 * boost::units::si::meters}, {86.679418986081146 * boost::units::si::meters, 2028.8632384985685 * boost::units::si::meters}, {76.433051179803442 * boost::units::si::meters, 2021.2674445733428 * boost::units::si::meters} }
	,   polygon2{ {47.406054388848133 * boost::units::si::meters, 2006.0214423397556 * boost::units::si::meters}, {28.81510316481581 * boost::units::si::meters, 2008.4421082111076 * boost::units::si::meters}, {-6.3325108204153366 * boost::units::si::meters, 1992.9333042073995 * boost::units::si::meters}, {27.761365470651072 * boost::units::si::meters, 1946.5902149900794 * boost::units::si::meters}, {38.268075486936141 * boost::units::si::meters, 1954.4447457790375 * boost::units::si::meters}, {24.006394347758032 * boost::units::si::meters, 1956.6066889483482 * boost::units::si::meters}, {25.337794755527284 * boost::units::si::meters, 1971.5821991600096 * boost::units::si::meters}, {9.2162477858364582 * boost::units::si::meters, 1974.2442818395793 * boost::units::si::meters}, {10.293138299894053 * boost::units::si::meters, 1995.4400568399578 * boost::units::si::meters}, {20.247309142898303 * boost::units::si::meters, 1998.1479072198272 * boost::units::si::meters}, {30.038688593485858 * boost::units::si::meters, 1997.1823339676484 * boost::units::si::meters}, {47.070513351471163 * boost::units::si::meters, 1994.4000153196976 * boost::units::si::meters} }
	,   polygon2{ {20.824367295892444 * boost::units::si::meters, 2037.5784559408203 * boost::units::si::meters}, {23.284429916122463 * boost::units::si::meters, 2039.0934458198026 * boost::units::si::meters}, {24.865132656123023 * boost::units::si::meters, 2041.1538779092953 * boost::units::si::meters}, {26.591702522884589 * boost::units::si::meters, 2043.3874097261578 * boost::units::si::meters}, {27.436886786250398 * boost::units::si::meters, 2045.6515751248226 * boost::units::si::meters}, {27.980832709756214 * boost::units::si::meters, 2047.850436209701 * boost::units::si::meters}, {28.362515384622384 * boost::units::si::meters, 2051.9297211011872 * boost::units::si::meters}, {27.891502840968315 * boost::units::si::meters, 2056.749075631611 * boost::units::si::meters}, {27.059137358039152 * boost::units::si::meters, 2060.6350740063936 * boost::units::si::meters}, {10.86143412347883 * boost::units::si::meters, 2135.1603323873132 * boost::units::si::meters}, {5.8497360304463655 * boost::units::si::meters, 2133.8618469722569 * boost::units::si::meters}, {12.257413291197736 * boost::units::si::meters, 2103.2038195906207 * boost::units::si::meters}, {16.417106396809686 * boost::units::si::meters, 2074.0353359831497 * boost::units::si::meters}, {19.598557187244296 * boost::units::si::meters, 2049.2614008579403 * boost::units::si::meters}, {15.43445205379976 * boost::units::si::meters, 2039.5998515598476 * boost::units::si::meters}, {8.7741036305669695 * boost::units::si::meters, 2032.5736824609339 * boost::units::si::meters} }
	,   polygon2{ {-56.292157783464063 * boost::units::si::meters, 2019.4463817449287 * boost::units::si::meters}, {-51.677077550732065 * boost::units::si::meters, 2019.0869584707543 * boost::units::si::meters}, {-31.93846619897522 * boost::units::si::meters, 2020.9734753156081 * boost::units::si::meters}, {-10.177439439343289 * boost::units::si::meters, 2026.0149330757558 * boost::units::si::meters}, {8.6963158243452199 * boost::units::si::meters, 2032.495894654654 * boost::units::si::meters}, {15.43445205379976 * boost::units::si::meters, 2039.6776393661276 * boost::units::si::meters}, {19.598557187244296 * boost::units::si::meters, 2049.4947642767802 * boost::units::si::meters}, {11.94911803904688 * boost::units::si::meters, 2107.3081343872473 * boost::units::si::meters}, {6.1889535349328071 * boost::units::si::meters, 2133.9497351441532 * boost::units::si::meters}, {-71.142922936356626 * boost::units::si::meters, 2113.9137489674613 * boost::units::si::meters} }
	,   polygon2{ {71.229917056334671 * boost::units::si::meters, 2064.7305591413751 * boost::units::si::meters}, {68.703442991012707 * boost::units::si::meters, 2068.9660867480561 * boost::units::si::meters}, {66.35096340382006 * boost::units::si::meters, 2073.0581055404618 * boost::units::si::meters}, {64.154263413336594 * boost::units::si::meters, 2077.0341268694028 * boost::units::si::meters}, {62.095974095980637 * boost::units::si::meters, 2080.9224532209337 * boost::units::si::meters}, {60.181904864846729 * boost::units::si::meters, 2084.7083242442459 * boost::units::si::meters}, {58.395819831697736 * boost::units::si::meters, 2088.4193299282342 * boost::units::si::meters}, {56.737091582326684 * boost::units::si::meters, 2092.0519790798426 * boost::units::si::meters}, {55.196104510279838 * boost::units::si::meters, 2095.6215876042843 * boost::units::si::meters}, {53.768038906739093 * boost::units::si::meters, 2099.1329927379265 * boost::units::si::meters}, {52.4442669787677 * boost::units::si::meters, 2102.60036127083 * boost::units::si::meters}, {51.219446188188158 * boost::units::si::meters, 2106.0299227973446 * boost::units::si::meters}, {50.087628579407465 * boost::units::si::meters, 2109.4293120633811 * boost::units::si::meters}, {49.0434190085507 * boost::units::si::meters, 2112.8042763061821 * boost::units::si::meters}, {48.076211079955101 * boost::units::si::meters, 2116.1781962113455 * boost::units::si::meters}, {47.184096474957187 * boost::units::si::meters, 2119.5454307291657 * boost::units::si::meters}, {46.358125337632373 * boost::units::si::meters, 2122.9255316341296 * boost::units::si::meters}, {45.594805828761309 * boost::units::si::meters, 2126.3167903255671 * boost::units::si::meters}, {44.901745772920549 * boost::units::si::meters, 2129.6612225677818 * boost::units::si::meters}, {42.776482675457373 * boost::units::si::meters, 2143.4292313298211 * boost::units::si::meters}, {33.825504765438382 * boost::units::si::meters, 2141.1101143257692 * boost::units::si::meters}, {35.845358014339581 * boost::units::si::meters, 2128.0249780612066 * boost::units::si::meters}, {36.602126400219277 * boost::units::si::meters, 2124.3731147507206 * boost::units::si::meters}, {37.401134572981391 * boost::units::si::meters, 2120.8232992673293 * boost::units::si::meters}, {38.268158123479225 * boost::units::si::meters, 2117.2752006053925 * boost::units::si::meters}, {39.206830262031872 * boost::units::si::meters, 2113.7322373939678 * boost::units::si::meters}, {40.226098444953095 * boost::units::si::meters, 2110.176715247333 * boost::units::si::meters}, {41.327712079219054 * boost::units::si::meters, 2106.6162167331204 * boost::units::si::meters}, {42.521960206620861 * boost::units::si::meters, 2103.0293187787756 * boost::units::si::meters}, {43.813771465967875 * boost::units::si::meters, 2099.4121804777533 * boost::units::si::meters}, {45.208497828629334 * boost::units::si::meters, 2095.7589603122324 * boost::units::si::meters}, {46.710784210823476 * boost::units::si::meters, 2092.0650571035221 * boost::units::si::meters}, {48.328492209606338 * boost::units::si::meters, 2088.3177289348096 * boost::units::si::meters}, {50.065641742490698 * boost::units::si::meters, 2084.5133355511352 * boost::units::si::meters}, {51.931048879923765 * boost::units::si::meters, 2080.6375198084861 * boost::units::si::meters}, {53.924571628158446 * boost::units::si::meters, 2076.6944962833077 * boost::units::si::meters}, {56.061731123598292 * boost::units::si::meters, 2072.6571758193895 * boost::units::si::meters}, {58.336075937841088 * boost::units::si::meters, 2068.5406175591052 * boost::units::si::meters}, {60.764417070371564 * boost::units::si::meters, 2064.3166414601728 * boost::units::si::meters}, {63.364501328906044 * boost::units::si::meters, 2059.9577094577253 * boost::units::si::meters}, {63.498819327505771 * boost::units::si::meters, 2059.7397893769667 * boost::units::si::meters}, {71.327004247577861 * boost::units::si::meters, 2064.5715717580169 * boost::units::si::meters} }
	,   polygon2{ {-58.900120067643002 * boost::units::si::meters, 2036.035919607617 * boost::units::si::meters}, {4.1643953784951009 * boost::units::si::meters, 2044.8996158568189 * boost::units::si::meters}, {-8.8980958796455525 * boost::units::si::meters, 2130.0408177953213 * boost::units::si::meters}, {-71.142922936356626 * boost::units::si::meters, 2113.9137489674613 * boost::units::si::meters} }
	,   polygon2{ {110.45821070036618 * boost::units::si::meters, 2023.5029782075435 * boost::units::si::meters}, {110.02380849211477 * boost::units::si::meters, 2023.8018999565393 * boost::units::si::meters}, {106.16568962216843 * boost::units::si::meters, 2026.5812900839373 * boost::units::si::meters}, {102.61995341570582 * boost::units::si::meters, 2029.2666604900733 * boost::units::si::meters}, {95.238993924867827 * boost::units::si::meters, 2035.1346307881176 * boost::units::si::meters}, {88.016271076747216 * boost::units::si::meters, 2029.1236497499049 * boost::units::si::meters}, {96.979312419949565 * boost::units::si::meters, 2021.9979013362899 * boost::units::si::meters}, {100.69889908016194 * boost::units::si::meters, 2019.1808649403974 * boost::units::si::meters}, {104.72677316813497 * boost::units::si::meters, 2016.2791830971837 * boost::units::si::meters}, {104.87608505174285 * boost::units::si::meters, 2016.1764382943511 * boost::units::si::meters} }
	,   polygon2{ {-18.061758793657646 * boost::units::si::meters, 1990.5421708123758 * boost::units::si::meters}, {-20.770634066371713 * boost::units::si::meters, 1992.547215571627 * boost::units::si::meters}, {-50.018991128657945 * boost::units::si::meters, 1979.5420716349036 * boost::units::si::meters}, {-49.530612848873716 * boost::units::si::meters, 1976.4354431331158 * boost::units::si::meters} }
	,   polygon2{ {-50.94824690616224 * boost::units::si::meters, 2012.4251115052029 * boost::units::si::meters}, {-47.678686576487962 * boost::units::si::meters, 2004.0408030152321 * boost::units::si::meters}, {-30.54414284741506 * boost::units::si::meters, 2010.6039813356474 * boost::units::si::meters}, {-33.936395633907523 * boost::units::si::meters, 2018.885938981548 * boost::units::si::meters} }
	,   polygon2{ {60.234277158975601 * boost::units::si::meters, 2059.7500370731577 * boost::units::si::meters}, {61.850952659966424 * boost::units::si::meters, 2060.587027894333 * boost::units::si::meters}, {63.429687363794073 * boost::units::si::meters, 2061.4007652923465 * boost::units::si::meters}, {62.587008505593985 * boost::units::si::meters, 2062.7849339777604 * boost::units::si::meters}, {59.960716544941533 * boost::units::si::meters, 2067.5574048114941 * boost::units::si::meters}, {54.056160550448112 * boost::units::si::meters, 2078.3457092046738 * boost::units::si::meters}, {49.275278887769673 * boost::units::si::meters, 2087.2141866404563 * boost::units::si::meters}, {43.904383104119916 * boost::units::si::meters, 2101.2286764290184 * boost::units::si::meters}, {41.076919109153096 * boost::units::si::meters, 2109.933896958828 * boost::units::si::meters}, {37.597890595905483 * boost::units::si::meters, 2123.7189256409183 * boost::units::si::meters}, {34.801003644533921 * boost::units::si::meters, 2140.4144436297938 * boost::units::si::meters}, {34.656916939886287 * boost::units::si::meters, 2141.3255256619304 * boost::units::si::meters}, {29.025312255194876 * boost::units::si::meters, 2139.8664280846715 * boost::units::si::meters}, {29.343940848018974 * boost::units::si::meters, 2137.6840392211452 * boost::units::si::meters}, {32.315366688242648 * boost::units::si::meters, 2122.0849221227691 * boost::units::si::meters}, {35.907462136412505 * boost::units::si::meters, 2108.2023783950135 * boost::units::si::meters}, {38.397395269654226 * boost::units::si::meters, 2098.6930259708315 * boost::units::si::meters}, {44.05604608124122 * boost::units::si::meters, 2085.6785464836285 * boost::units::si::meters}, {49.206872912938707 * boost::units::si::meters, 2074.5974559597671 * boost::units::si::meters}, {55.502115392417181 * boost::units::si::meters, 2062.2393005518243 * boost::units::si::meters}, {57.615248237678315 * boost::units::si::meters, 2058.3588521527126 * boost::units::si::meters}, {58.716268902702723 * boost::units::si::meters, 2058.9929615128785 * boost::units::si::meters} }
	,   polygon2{ {-50.878204123058822 * boost::units::si::meters, 1985.007620960474 * boost::units::si::meters}, {56.475419476861134 * boost::units::si::meters, 2026.9620064720511 * boost::units::si::meters}, {53.008824725286104 * boost::units::si::meters, 2032.2638855213299 * boost::units::si::meters}, {-51.864093662879895 * boost::units::si::meters, 1991.2789738662541 * boost::units::si::meters} }
	,   polygon2{ {20.496436579851434 * boost::units::si::meters, 2137.656673932448 * boost::units::si::meters}, {21.422715179622173 * boost::units::si::meters, 2132.4367652460933 * boost::units::si::meters}, {22.011930690612644 * boost::units::si::meters, 2129.3538772370666 * boost::units::si::meters}, {22.604525375645608 * boost::units::si::meters, 2126.4220930049196 * boost::units::si::meters}, {23.214941204874776 * boost::units::si::meters, 2123.5701133944094 * boost::units::si::meters}, {24.532586044457275 * boost::units::si::meters, 2117.8869991777465 * boost::units::si::meters}, {26.070330081449356 * boost::units::si::meters, 2111.7795714670792 * boost::units::si::meters}, {30.26292373938486 * boost::units::si::meters, 2096.073230439797 * boost::units::si::meters}, {34.304796504904516 * boost::units::si::meters, 2085.937103856355 * boost::units::si::meters}, {35.943276813079137 * boost::units::si::meters, 2082.0105860047042 * boost::units::si::meters}, {37.562859142140951 * boost::units::si::meters, 2078.3439666507766 * boost::units::si::meters}, {39.307569587836042 * boost::units::si::meters, 2074.6399859320372 * boost::units::si::meters}, {41.33169653697405 * boost::units::si::meters, 2070.5749470107257 * boost::units::si::meters}, {45.196967197989579 * boost::units::si::meters, 2063.1527551589534 * boost::units::si::meters}, {50.44328349805437 * boost::units::si::meters, 2053.3217666633427 * boost::units::si::meters}, {58.559859892644454 * boost::units::si::meters, 2057.6531853275374 * boost::units::si::meters}, {53.335369086242281 * boost::units::si::meters, 2067.443275376223 * boost::units::si::meters}, {49.530037477903534 * boost::units::si::meters, 2074.7503707222641 * boost::units::si::meters}, {47.587758875335567 * boost::units::si::meters, 2078.6510342257097 * boost::units::si::meters}, {45.933331172971521 * boost::units::si::meters, 2082.1633467273787 * boost::units::si::meters}, {44.39718789269682 * boost::units::si::meters, 2085.6410660017282 * boost::units::si::meters}, {42.823372394370381 * boost::units::si::meters, 2089.4126185923815 * boost::units::si::meters}, {39.010841159964912 * boost::units::si::meters, 2098.9736071322113 * boost::units::si::meters}, {34.975939282798208 * boost::units::si::meters, 2114.0892014540732 * boost::units::si::meters}, {33.475290258938912 * boost::units::si::meters, 2120.0492996312678 * boost::units::si::meters}, {32.194846105820034 * boost::units::si::meters, 2125.5719641260803 * boost::units::si::meters}, {31.611759874038398 * boost::units::si::meters, 2128.2962546264753 * boost::units::si::meters}, {31.039218865451403 * boost::units::si::meters, 2131.1288259327412 * boost::units::si::meters}, {30.470571111305617 * boost::units::si::meters, 2134.1040995102376 * boost::units::si::meters}, {29.429458484402858 * boost::units::si::meters, 2139.9711386989802 * boost::units::si::meters} }
	,   polygon2{ {78.969179322652053 * boost::units::si::meters, 2071.9684624345973 * boost::units::si::meters}, {75.647501201310661 * boost::units::si::meters, 2077.5648095887154 * boost::units::si::meters}, {72.147657879511826 * boost::units::si::meters, 2075.5019007716328 * boost::units::si::meters}, {75.460972486995161 * boost::units::si::meters, 2069.9056369587779 * boost::units::si::meters} }
	,   polygon2{ {-52.211355003295466 * boost::units::si::meters, 1993.4879418378696 * boost::units::si::meters}, {50.116093482414726 * boost::units::si::meters, 2034.2080727582797 * boost::units::si::meters}, {47.268453865894116 * boost::units::si::meters, 2039.7477549007162 * boost::units::si::meters}, {-53.198604649514891 * boost::units::si::meters, 1999.7679465310648 * boost::units::si::meters} }
	,   polygon2{ {48.017425745201763 * boost::units::si::meters, 1961.733095000498 * boost::units::si::meters}, {41.713242659228854 * boost::units::si::meters, 1962.6332763833925 * boost::units::si::meters}, {43.885201533150394 * boost::units::si::meters, 1978.033262996003 * boost::units::si::meters}, {28.58506342000328 * boost::units::si::meters, 1980.0540837459266 * boost::units::si::meters}, {30.038688593485858 * boost::units::si::meters, 1997.1823339676484 * boost::units::si::meters}, {20.247309142898303 * boost::units::si::meters, 1998.1479072198272 * boost::units::si::meters}, {11.542079725477379 * boost::units::si::meters, 1999.0136258145794 * boost::units::si::meters}, {9.2162477858364582 * boost::units::si::meters, 1974.2442818395793 * boost::units::si::meters}, {25.337794755527284 * boost::units::si::meters, 1971.5821991600096 * boost::units::si::meters}, {24.006394347758032 * boost::units::si::meters, 1956.6066889483482 * boost::units::si::meters}, {38.268075486936141 * boost::units::si::meters, 1954.4447457790375 * boost::units::si::meters} }
	,   polygon2{ {54.749469637463335 * boost::units::si::meters, 2127.9021364478394 * boost::units::si::meters}, {53.529123799700756 * boost::units::si::meters, 2133.7214063126594 * boost::units::si::meters}, {50.202053181303199 * boost::units::si::meters, 2133.0224207052961 * boost::units::si::meters}, {51.414034716959577 * boost::units::si::meters, 2127.2032352313399 * boost::units::si::meters} }
	,   polygon2{ {139.59735513507621 * boost::units::si::meters, 1977.7680113734677 * boost::units::si::meters}, {143.77709310222417 * boost::units::si::meters, 1975.0239649955183 * boost::units::si::meters}, {150.36280441022245 * boost::units::si::meters, 1985.0553361177444 * boost::units::si::meters}, {146.18306644307449 * boost::units::si::meters, 1987.7993824956939 * boost::units::si::meters} }
	,   polygon2{ {73.816761517140549 * boost::units::si::meters, 2060.3501096237451 * boost::units::si::meters}, {71.190584940311965 * boost::units::si::meters, 2064.6048947637901 * boost::units::si::meters}, {63.53197168832412 * boost::units::si::meters, 2059.8777769254521 * boost::units::si::meters}, {66.158148265152704 * boost::units::si::meters, 2055.6229917854071 * boost::units::si::meters} }
	,   polygon2{ {50.484425916685723 * boost::units::si::meters, 2053.4570706384256 * boost::units::si::meters}, {52.838457798701711 * boost::units::si::meters, 2049.045887815766 * boost::units::si::meters}, {60.77858688053675 * boost::units::si::meters, 2053.2831452032551 * boost::units::si::meters}, {58.424554998520762 * boost::units::si::meters, 2057.6943280259147 * boost::units::si::meters} }
	};

	//! The original geometry is self intersecting.. so process these. This is important. The input geometry to the biased_position_generator must not contain ANY self intersections.
	GEOMETRIX_ASSERT(is_self_intersecting(pouter, pholes));

	//! Union the holes together to find the actual disjoint set of holes.
	std::vector<polygon_with_holes2> newHoles;
	auto unn = clipper_union(pholes, scale);
	for (auto const& h : unn)
	{
		//! Offset these slightly to emphasize near intersections.
		auto r = clipper_offset(h, 0.1 * units::si::meters, scale);
		GEOMETRIX_ASSERT(r.size() == 1);//! If the offsetting results in more than one polygon.. the input polygon needs to be investigated.
		newHoles.insert(newHoles.end(), r.begin(), r.end());
	}

	//! Difference the disjoint set of holes from the outer area. The result is a collection of polygons with holes, AKA accessible spaces.
	auto asAreas = clipper_difference(outer, newHoles, scale);

	//! The result is a collection of areas.. each of which is a disjoint accessible space; separate from the others. Each should be processed as a separate region.

	//! Here we look at the first region which is the most interesting one.
	GEOMETRIX_ASSERT(!is_self_intersecting(asAreas[0]));

	stk::units::length granularity = 4.0 * units::si::meters; //! This term specifies the resolution of the steiner points in the resulting mesh. 
	stk::units::length distSaturation = 1.0 * units::si::meters;//! This term specifies the distance at which the attractive force saturates... i.e. the mesh elements within this distance have uniform weight.
	double attractionStrength = 0.1;//! Quantity specifying how strongly the attractive geometry weighs on mesh elements.
	auto segs = polygon_collection_as_segment_range<segment2>(pholes);//! Specify the holes to be the attractive geometry (which are stored as segments.)

	//! This is the generator. I construct from polygon with holes here.. also has interface for simple polygonal boundary only.
	auto bpg = biased_position_generator{ asAreas[0].get_outer(), asAreas[0].get_holes(), segs, granularity, distSaturation, attractionStrength };

	//! Here I generate a bunch of random positions.
		
	//! These are for drawing via GraphicalDebugging plugin on visual studio... ignore.
	std::vector<circle2> rs;

	geometrix::random_real_generator<> rnd;//! This is just a uniform distribution. Use whatever you have handy.

	for (auto i = 0; i < 500; ++i)
	{
		//! This is where you get the random position in the region.
		auto p = bpg.get_random_position<point2>(rnd(), rnd(), rnd());

		//! These are for drawing via GraphicalDebugging plugin on visual studio... ignore.
		rs.emplace_back(p, 0.1 * units::si::meters);
	}

	boost::range::remove_erase_if(asAreas, [](const polygon_with_holes2& pgon)
	{
		if (!is_polygon_simple(pgon.get_outer(), make_tolerance_policy()))
			return true;
		for(const auto& hole : pgon.get_holes())
			if (!is_polygon_simple(hole, make_tolerance_policy()))
				return true;
		return false;
	});

	auto bpg2 = biased_position_generator{ asAreas, segs, granularity, distSaturation, attractionStrength };

	//! Here I generate a bunch of random positions.
		
	//! These are for drawing via GraphicalDebugging plugin on visual studio... ignore.
	rs.clear();

	for (auto i = 0; i < 500; ++i)
	{
		//! This is where you get the random position in the region.
		auto p = bpg2.get_random_position<point2>(rnd(), rnd(), rnd());

		//! These are for drawing via GraphicalDebugging plugin on visual studio... ignore.
		rs.emplace_back(p, 0.1 * units::si::meters);
	}
	EXPECT_TRUE(true);
}

