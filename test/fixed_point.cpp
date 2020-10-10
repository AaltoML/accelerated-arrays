#include <catch2/catch.hpp>

#include "fixed_point.hpp"

TEST_CASE( "Fixed point", "[accelerated-arrays]" ) {
    using namespace accelerated;

    Fixed<std::int8_t> a(0.5), b(0.5);
    auto c = a*b;

    REQUIRE(c == Fixed<std::int8_t>(0.25));
}
