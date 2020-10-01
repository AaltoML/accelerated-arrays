// This creates the main() function for the binary that runs the tests.
#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include "cpu_image.hpp"

TEST_CASE( "CpuImage basics", "[accelerated-arrays]" ) {
    using namespace accelerated;
    auto factory = CpuImage::createFactory();

    auto image = factory->create<std::uint16_t, 2>(3, 4).get();

    REQUIRE(image->width == 3);
    REQUIRE(image->height == 4);
    REQUIRE(image->channels == 2);
    REQUIRE(image->storageType == ImageTypeSpec::StorageType::CPU);
    REQUIRE(image->size() == 3*4*2*2);
    REQUIRE(image->numberOfPixels() == 3*4);
    REQUIRE(image->bytesPerPixel() == 2*2);

    std::vector<std::uint16_t> in = {
        1,2,  3,4,  5,6,
        0,0,  9,0,  0,0,
        7,0,  0,0,  0,0,
        0,0,  0,8,  0,9
    };
    image->write(in).get();

    std::vector<std::uint16_t> out;
    image->read(out).get();

    REQUIRE(in.size() == out.size());
    REQUIRE(in.size() == 2*3*4);
    for (std::size_t i = 0; i < in.size(); ++i)
        REQUIRE(in[i] == out[i]);
}
