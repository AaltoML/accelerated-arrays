// This creates the main() function for the binary that runs the tests.
#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>
#include "cpu_image.hpp"
#include "cpu_ops.hpp"

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
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::REPEAT) == 5);
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::MIRROR) == 3);
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::ZERO) == 0);
            REQUIRE(cpuImg.get<std::int16_t>(3, 0, 0, Image::Border::WRAP) == 1);
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

        auto cpuOps = cpu::operations::createFactory(*processor);
        auto convolution = cpuOps->create(
                operations::fixedConvolution2D::Spec{}
                    .setKernel({
                        { -1, 0, 1 },
                        { -3, 0, 3 },
                        { -1, 0, 1 }}, 1/3.0)
                    .setBias(0.5),
                *image
            );

        auto outImage = factory->createLike(*image);
        waitFor(operations::callUnary(convolution, *image, *outImage));

        auto &outCpu = cpu::Image::castFrom(*outImage);
        REQUIRE(outCpu.get<std::int16_t>(1, 1, 1) == int((-2 + 3*6) / 3.0 + 0.5));
    }
}
