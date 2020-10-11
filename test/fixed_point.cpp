#include <catch2/catch.hpp>
#include <cmath>

#include "fixed_point.hpp"

TEST_CASE( "Fixed point", "[accelerated-arrays]" ) {
    using namespace accelerated;

    FixedPoint<std::int8_t> a(0.5), b(0.5);

    REQUIRE(std::fabs(a.toFloat() - 0.5) < 1.0/100);

    auto c = a*b;

    REQUIRE(c == FixedPoint<std::int8_t>(0.25));

    // TODO
    //auto d = FixedPoint<std::int8_t>(-0.6) + a;
    //REQUIRE(d == FixedPoint<std::int8_t>(-0.1));
}
