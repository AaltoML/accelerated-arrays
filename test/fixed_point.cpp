#include <catch2/catch.hpp>
#include <cmath>

#include "fixed_point.hpp"

TEST_CASE( "Unsigned fixed point", "[accelerated-arrays]" ) {
    using namespace accelerated;

    typedef FixedPoint<std::uint16_t> F;
    REQUIRE(sizeof(F) == 2);
    F a(0.5), b(0.5);

    REQUIRE(!F::isSigned());
    REQUIRE(std::fabs(F::max() - 0xffff) < 1e-8);
    REQUIRE(std::fabs(F::min()) < 1e-8);
    REQUIRE(std::fabs(F::floatMax() - 1.0) < 1e-5);
    REQUIRE(std::fabs(F::floatMin()) < 1e-5);

    REQUIRE(a.value == int(0xffff*0.5 + 0.5));
    REQUIRE(std::fabs(a.toFloat() - 0.5) < 1.0/10000);

    auto c = a*b;
    REQUIRE(c == F(0.25));
}

TEST_CASE( "Signed fixed point", "[accelerated-arrays]" ) {
    using namespace accelerated;

    typedef FixedPoint<std::int8_t> F;
    REQUIRE(sizeof(F) == 1);
    F a(0.5), b(0.5);

    REQUIRE(F::isSigned());
    REQUIRE(std::fabs(F::max() - 127) < 1e-8);
    REQUIRE(std::fabs(F::min() - (-128)) < 1e-8);
    REQUIRE(std::fabs(F::floatMax() - 1.0) < 1e-5);
    REQUIRE(std::fabs(F::floatMin() - (-1.0)) < 1e-5);

    REQUIRE(a.value == int((255*0.5 - 1)/2));
    REQUIRE(std::fabs(a.toFloat() - 0.5) < 1.0/100);

    auto c = a*b;
    REQUIRE(c == F(0.25));
    auto d = F(-0.6) + a;
    REQUIRE(d == F(-0.1));
}
