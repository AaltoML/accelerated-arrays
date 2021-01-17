// This creates the main() function for the binary that runs the tests.
#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include <cmath>

#include "cpu/image.hpp"
#include "cpu/operations.hpp"
#ifdef TEST_WITH_OPENGL
#include "opengl/image.hpp"
#include "opengl/operations.hpp"
#endif

TEST_CASE( "CpuImage basics", "[accelerated-arrays]" ) {
    using namespace accelerated;

    auto factory = cpu::Image::createFactory();
    auto image = factory->create<std::int16_t, 2>(3, 4);

    {
        auto testSpec = factory->getSpec<std::uint8_t, 2>();
        REQUIRE(testSpec.channels == 2);
        REQUIRE(testSpec.dataType == ImageTypeSpec::DataType::UINT8);
        REQUIRE(testSpec.storageType == ImageTypeSpec::StorageType::CPU);
    }

    REQUIRE(image->width == 3);
    REQUIRE(image->height == 4);
    REQUIRE(image->channels == 2);
    REQUIRE(image->storageType == ImageTypeSpec::StorageType::CPU);
    REQUIRE(image->size() == 3*4*2*2);
    REQUIRE(image->numberOfPixels() == 3*4);
    REQUIRE(image->bytesPerPixel() == 2*2);

    std::vector<std::int16_t> in = {
        1,2,  3,4,  5,0,
        0,0,  9,0,  0,6,
        7,0,  0,0,  0,0,
        0,0,  0,8,  0,9
    };

    image->write(in).wait();

    {
        const auto &cpuImg = cpu::Image::castFrom(*image);
        REQUIRE(cpuImg.get<std::int16_t>(2, 0, 0) == 5);
        REQUIRE(cpuImg.get<std::int16_t>(2, 1, 1) == 6);
        REQUIRE(std::fabs(cpuImg.get<float>(2, 1, 1) - 6) < 1e-10);
        REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::CLAMP) == 5);
        REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::MIRROR) == 3);
        REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::ZERO) == 0);
        REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::REPEAT) == 1);
        REQUIRE(std::fabs(cpuImg.get<float>(3, 0, 0, Image::Border::CLAMP) - 5) < 1e-10);
        auto pix = cpuImg.get<std::int16_t, 2>(1, 0);
        REQUIRE(pix.size() == 2);
        REQUIRE(pix.at(0) ==  3);
        REQUIRE(pix.at(1) ==  4);
    }

    {
        // Test ROI
        auto roi = image->createROI(1, 1, 2, 1);
        REQUIRE(roi->width == 2);
        REQUIRE(roi->height == 1);
        REQUIRE(roi->channels == 2);
        REQUIRE(roi->dataType == ImageTypeSpec::DataType::SINT16);
        REQUIRE(roi->storageType == ImageTypeSpec::StorageType::CPU);
        const auto &cpuRoi = cpu::Image::castFrom(*roi);
        REQUIRE(cpuRoi.get<std::int16_t>(0, 0, 0) == 9);
        REQUIRE(cpuRoi.get<std::int16_t>(1, 0, 1) == 6);
        REQUIRE(cpuRoi.get<std::int16_t>(-1, 0, 1, Image::Border::MIRROR) == 6);
    }

    {
        std::vector<std::int16_t> out;
        image->read(out).wait();

        REQUIRE(in.size() == out.size());
        REQUIRE(in.size() == 2*3*4);
        for (std::size_t i = 0; i < in.size(); ++i)
            REQUIRE(in[i] == out[i]);
    }

    {
        auto &cpuImg = cpu::Image::castFrom(*image);
        auto imgRef = cpu::Image::createReference<std::int16_t, 2>(image->width, image->height, cpuImg.getData<std::int16_t>());

        cpuImg.set<std::int16_t>(2, 1, 1, 12);
        REQUIRE(cpuImg.get<std::int16_t>(2, 1, 1) == 12);
        REQUIRE(imgRef->get<std::int16_t>(2, 1, 1) == 12);
        cpuImg.set<float>(2, 1, 1, 13.001);
        REQUIRE(std::fabs(cpuImg.get<float>(2, 1, 1) - 13.001) < 0.001);
        REQUIRE(imgRef->get<std::int16_t>(2, 1, 1) != 12);
        REQUIRE(imgRef->get<std::int16_t>(0, 0, 1) == 2);
    }

    // test copyTo
    {
        auto &cpuImg = cpu::Image::castFrom(*image);
        auto cpy = factory->createLike(*image);
        cpuImg.copyTo(*cpy);
        const auto &cpuImgCpy = cpu::Image::castFrom(*image);
        REQUIRE(cpuImg.get<std::int16_t>(2, 1, 1) == cpuImgCpy.get<std::int16_t>(2, 1, 1));
    }
}

TEST_CASE( "Fixed point images", "[accelerated-arrays]" ) {
    using namespace accelerated;
    auto factory = cpu::Image::createFactory();

    typedef FixedPoint<std::int16_t> Type;

    auto image = factory->create<Type, 2>(3, 4);

    REQUIRE(image->width == 3);
    REQUIRE(image->height == 4);
    REQUIRE(image->channels == 2);
    REQUIRE(image->storageType == ImageTypeSpec::StorageType::CPU);
    REQUIRE(image->size() == 3*4*2*2);
    REQUIRE(image->numberOfPixels() == 3*4);
    REQUIRE(image->bytesPerPixel() == 2*2);

    std::vector<std::int16_t> in = {
        1,2,  3,4,  5,0,
        0,0,  9,0,  0,6,
        7,0,  0,0,  0,0,
        0,0,  0,8,  0,9
    };

    image->write(reinterpret_cast<std::vector<Type>&>(in)).wait();
    auto &cpuImg = cpu::Image::castFrom(*image);
    REQUIRE(cpuImg.get<Type>(2, 0, 0).value == 5);
    REQUIRE(std::fabs(cpuImg.get<float>(2, 0, 0) - 5.0 / 0x7fff) < 0.0001);
    cpuImg.set<float>(2, 0, 0, 23.001 / 0x7fff);
    REQUIRE(cpuImg.get<Type>(2, 0, 0).value == 23);
    REQUIRE(std::fabs(cpuImg.get<float>(2, 0, 0) - 23.001 / 0x7fff) < 0.0001);
}
