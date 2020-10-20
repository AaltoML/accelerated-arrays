#include <catch2/catch.hpp>
#include <iostream>

#include "cpu/image.hpp"
#include "cpu/operations.hpp"

#ifdef TEST_WITH_OPENGL
#include "opengl/image.hpp"
#include "opengl/operations.hpp"
#endif

namespace {
using namespace accelerated;

struct ProcessorItem {
    std::unique_ptr<Processor> processor;
    std::unique_ptr<Image::Factory> img;
    std::unique_ptr<operations::StandardFactory> ops;

    ProcessorItem() {}
    ProcessorItem(std::unique_ptr<Processor> p) : processor(std::move(p)) {
        img = cpu::Image::createFactory();
        ops = cpu::operations::createFactory(*processor);
    }
};
}

TEST_CASE( "Convolution 2D", "[accelerated-arrays]" ) {

    std::vector< ProcessorItem > items;
    items.emplace_back(Processor::createInstant());
    items.emplace_back(Processor::createThreadPool(1));
    items.emplace_back(Processor::createThreadPool(5));

    #ifdef TEST_WITH_OPENGL

    items.emplace_back();
    items.back().processor = opengl::createGLFWProcessor();
    items.back().img = opengl::Image::createFactory(*items.back().processor);
    {
        auto gpuOps = opengl::operations::createFactory(*items.back().processor);
        gpuOps->debugLogShaders(true);
        items.back().ops = std::move(gpuOps);
    }

    #endif

    for (auto &it : items) {
        typedef std::uint8_t IntType;
        typedef FixedPoint<IntType> Type;

        std::vector<IntType> inData = {
            1,2,  3,4,  5,0,
            0,0,  9,0,  0,6,
            7,0,  0,0,  0,0,
            0,0,  0,8,  0,9
        };
        auto image = it.img->create<Type, 2>(3, 4);
        image->writeRawFixedPoint(inData).wait();
        //for (auto &el : inData) std::cout << "in-data:" << int(el) << std::endl;

        auto convolution = it.ops->fixedConvolution2D({
                { -1, 0, 1 },
                { -3, 0, 3 },
                { -1, 0, 1 }
            })
            .scaleKernelValues(1/3.0)
            .setBias(1e-5)
            .build(*image);

        auto outImage = it.img->createLike(*image);
        operations::callUnary(convolution, *image, *outImage).wait();

        auto outData = inData;
        outData.clear();
        outData.resize(inData.size(), 123);
        outImage->readRawFixedPoint(outData).wait();
        // for (auto &el : outData) std::cout << "out-data:" << int(el) << std::endl;

        // just for checking the output
        auto factory = cpu::Image::createFactory();
        auto checkImage = factory->create<Type, 2>(3, 4);
        checkImage->writeRawFixedPoint(outData).wait();

        const auto &outCpu = cpu::Image::castFrom(*checkImage);
        REQUIRE(outCpu.get<Type>(1, 1, 1).value == int((-2 + 3*6) / 3.0 + 0.5));
    }
}

TEST_CASE( "Affine pixel ops & copyFrom", "[accelerated-arrays]" ) {

    std::vector< ProcessorItem > items;
    items.emplace_back(Processor::createInstant());
    items.emplace_back(Processor::createThreadPool(1));
    items.emplace_back(Processor::createThreadPool(5));

    #ifdef TEST_WITH_OPENGL

    items.emplace_back();
    items.back().processor = opengl::createGLFWProcessor();
    items.back().img = opengl::Image::createFactory(*items.back().processor);
    items.back().ops = opengl::operations::createFactory(*items.back().processor);

    #endif

    for (auto &it : items) {
        typedef std::uint8_t IntType;
        typedef FixedPoint<IntType> Type;

        std::vector<IntType> inData = {
            1,2,  3,4,  5,0,
            0,0,  9,0,  0,6,
            7,0,  0,0,  0,0,
            0,0,  0,8,  0,9
        };
        auto inImage = it.img->create<Type, 2>(3, 4);
        inImage->writeRawFixedPoint(inData).wait();
        //for (auto &el : inData) std::cout << "in-data:" << int(el) << std::endl;

        auto intermediaryImage = it.img->create<std::int16_t, 3>(3, 4);

        auto pixAffine = it.ops->pixelwiseAffine({
                { -1, 1 },
                { 0, 2 },
                { 1, 1 }
            })
            .scaleLinearValues(1000)
            .setBias({5, 0, 0})
            .build(*inImage, *intermediaryImage);

        operations::callUnary(pixAffine, *inImage, *intermediaryImage).wait();

        auto outImage = it.img->create<Type, 3>(3, 4);
        auto chanAffine = it.ops->channelwiseAffine(0.1, -0.02).build(*intermediaryImage, *outImage);

        operations::callUnary(chanAffine, *intermediaryImage, *outImage).wait();

        // Testing copyFrom
        auto factory = cpu::Image::createFactory();
        auto checkImage = factory->createLike(*outImage);
        auto &outCpu = cpu::Image::castFrom(*checkImage);
        outCpu.copyFrom(*outImage).wait();

        int outVal = int(outCpu.get<Type>(1, 0, 0).value);

        const float af = (4 - 3) / 255.0 * 1000 + 5;
        const int ai = int(af);
        const float bf = ai * 0.1 - 0.02;
        const int bi = int(bf * 255 + 0.5);
        // std::cout << af << "," << ai << "," << bf << "," << bi << "\t" << outVal << std::endl;

        REQUIRE(outVal == bi);
    }
}

TEST_CASE( "Swizzle", "[accelerated-arrays]" ) {
    std::vector< ProcessorItem > items;
    items.emplace_back(Processor::createInstant());
    items.emplace_back(Processor::createThreadPool(1));
    items.emplace_back(Processor::createThreadPool(5));

    #ifdef TEST_WITH_OPENGL

    items.emplace_back();
    items.back().processor = opengl::createGLFWProcessor();
    items.back().img = opengl::Image::createFactory(*items.back().processor);
    items.back().ops = opengl::operations::createFactory(*items.back().processor);

    #endif

    for (auto &it : items) {
        typedef std::uint32_t Type;
        std::vector<Type> inData = {
            1,2,  3,4,  5,0,
            0,0,  9,0,  0,6
        };
        auto inImage = it.img->create<Type, 2>(3, 2);
        inImage->write(inData).wait();

        auto outImage = it.img->create<Type, 4>(3, 2);
        auto swiz = it.ops->swizzle("0gr1").build(*inImage, *outImage);

        operations::callUnary(swiz, *inImage, *outImage).wait();

        auto factory = cpu::Image::createFactory();
        auto checkImage = factory->createLike(*outImage);
        auto &outCpu = cpu::Image::castFrom(*checkImage);
        outCpu.copyFrom(*outImage).wait();

        REQUIRE(outCpu.get<Type>(1, 0, 0) == 0);
        REQUIRE(outCpu.get<Type>(1, 0, 1) == 4);
        REQUIRE(outCpu.get<Type>(1, 0, 2) == 3);
        REQUIRE(outCpu.get<Type>(1, 0, 3) == 1);
    }
}
