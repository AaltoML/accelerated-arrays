// This creates the main() function for the binary that runs the tests.
#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include "cpu/image.hpp"
#include "cpu/operations.hpp"

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
    }
}

TEST_CASE( "Convolution 2D", "[accelerated-arrays]" ) {
    using namespace accelerated;

    struct Item {
        std::unique_ptr<Processor> processor;
        std::unique_ptr<Image::Factory> img;
        std::unique_ptr<operations::StandardFactory> ops;

        Item() {}
        Item(std::unique_ptr<Processor> p) : processor(std::move(p)) {
            img = cpu::Image::createFactory(*processor);
            ops = cpu::operations::createFactory(*processor);
        }
    };

    std::vector< Item > items;
    items.emplace_back(Processor::createInstant());
    items.emplace_back(Processor::createThreadPool(1));
    items.emplace_back(Processor::createThreadPool(5));

    for (auto &it : items) {
        std::vector<std::int16_t> in = {
            1,2,  3,4,  5,0,
            0,0,  9,0,  0,6,
            7,0,  0,0,  0,0,
            0,0,  0,8,  0,9
        };
        auto image = it.img->create<std::int16_t, 2>(3, 4);
        image->write(in).wait();

        auto convolution = it.ops->create(
                operations::fixedConvolution2D::Spec{}
                    .setKernel({
                        { -1, 0, 1 },
                        { -3, 0, 3 },
                        { -1, 0, 1 }}, 1/3.0)
                    .setBias(0.5),
                *image
            );

        auto outImage = it.img->createLike(*image);
        operations::callUnary(convolution, *image, *outImage).wait();
        auto outData = in;

        outImage->read(outData).wait();

        // just for checking the output
        auto checkerProc = Processor::createInstant();
        auto factory = cpu::Image::createFactory(*checkerProc);
        auto checkImage = factory->create<std::int16_t, 2>(3, 4);
        checkImage->write(outData).wait();
        const auto &outCpu = cpu::Image::castFrom(*checkImage);
        REQUIRE(outCpu.get<std::int16_t>(1, 1, 1) == int((-2 + 3*6) / 3.0 + 0.5));
    }
}

TEST_CASE( "types", "[accelerated-arrays]" ) {
    using namespace accelerated;
    REQUIRE(int(ImageTypeSpec::minValueOf(ImageTypeSpec::DataType::UINT8)) == 0);
    REQUIRE(int(ImageTypeSpec::maxValueOf(ImageTypeSpec::DataType::UINT8)) == 0xff);

    REQUIRE(int(ImageTypeSpec::minValueOf(ImageTypeSpec::DataType::SINT8)) == -128);
    REQUIRE(int(ImageTypeSpec::maxValueOf(ImageTypeSpec::DataType::SINT8)) == 127);

    REQUIRE(int(ImageTypeSpec::minValueOf(ImageTypeSpec::DataType::UINT16)) == 0);
    REQUIRE(int(ImageTypeSpec::maxValueOf(ImageTypeSpec::DataType::UINT16)) == 0xffff);

    REQUIRE(int(ImageTypeSpec::minValueOf(ImageTypeSpec::DataType::SINT16)) == -0x8000);
    REQUIRE(int(ImageTypeSpec::maxValueOf(ImageTypeSpec::DataType::SINT16)) == 0x7fff);

    REQUIRE(long(ImageTypeSpec::minValueOf(ImageTypeSpec::DataType::UINT32)) == 0);
    REQUIRE(long(ImageTypeSpec::maxValueOf(ImageTypeSpec::DataType::UINT32)) == 0xffffffffl);

    REQUIRE(long(ImageTypeSpec::minValueOf(ImageTypeSpec::DataType::SINT32)) == -0x80000000l);
    REQUIRE(long(ImageTypeSpec::maxValueOf(ImageTypeSpec::DataType::SINT32)) == 0x7fffffffl);
}
