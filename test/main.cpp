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

    std::vector< std::unique_ptr<Processor> > processors;
    processors.push_back(Processor::createQueue());
    processors.push_back(Processor::createInstant());
    processors.push_back(Processor::createThreadPool(1));
    processors.push_back(Processor::createThreadPool(5));

    for (std::size_t i = 0; i < processors.size(); ++i) {
        auto processor = std::move(processors.at(i));
        Queue *queue = nullptr;
        if (i == 0) {
            queue = reinterpret_cast<Queue*>(processor.get());
        }

        auto factory = cpu::Image::createFactory(*processor);
        auto image = factory->create<std::int16_t, 2>(3, 4);

        auto waitFor = [queue](Future f) {
            if (queue) queue->processAll();
            f.wait();
        };

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

        waitFor(image->write(in));

        {
            const auto &cpuImg = cpu::Image::castFrom(*image);
            REQUIRE(cpuImg.get<std::int16_t>(2, 0, 0) == 5);
            REQUIRE(cpuImg.get<std::int16_t>(2, 1, 1) == 6);
            REQUIRE(std::fabs(cpuImg.getFloat(2, 1, 1) - 6) < 1e-10);
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::CLAMP) == 5);
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::MIRROR) == 3);
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::ZERO) == 0);
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::REPEAT) == 1);
            REQUIRE(std::fabs(cpuImg.getFloat(3, 0, 0, Image::Border::CLAMP) - 5) < 1e-10);
            auto pix = cpuImg.get<std::int16_t, 2>(1, 0);
            REQUIRE(pix.size() == 2);
            REQUIRE(pix.at(0) ==  3);
            REQUIRE(pix.at(1) ==  4);
        }

        {
            std::vector<std::int16_t> out;
            waitFor(image->read(out));

            REQUIRE(in.size() == out.size());
            REQUIRE(in.size() == 2*3*4);
            for (std::size_t i = 0; i < in.size(); ++i)
                REQUIRE(in[i] == out[i]);
        }

        {
            auto &cpuImg = cpu::Image::castFrom(*image);
            cpuImg.set<std::int16_t>(2, 1, 1, 12);
            REQUIRE(cpuImg.get<std::int16_t>(2, 1, 1) == 12);
            cpuImg.setFloat(2, 1, 1, 13.001);
            REQUIRE(std::fabs(cpuImg.getFloat(2, 1, 1) - 13.001) < 0.001);
        }
    }
}

TEST_CASE( "Fixed point images", "[accelerated-arrays]" ) {
    using namespace accelerated;

    auto processor = Processor::createInstant();
    auto factory = cpu::Image::createFactory(*processor);

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
    REQUIRE(std::fabs(cpuImg.getFloat(2, 0, 0) - 5.0 / 0x7fff) < 0.0001);
    cpuImg.setFloat(2, 0, 0, 23.001 / 0x7fff);
    REQUIRE(cpuImg.get<Type>(2, 0, 0).value == 23);
    REQUIRE(std::fabs(cpuImg.getFloat(2, 0, 0) - 23.001 / 0x7fff) < 0.0001);
}
