#include <catch2/catch.hpp>
#include <iostream>

#include "cpu/image.hpp"
#include "cpu/operations.hpp"

#ifdef TEST_WITH_OPENGL
#include "opengl/image.hpp"
#include "opengl/operations.hpp"
#endif

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
        auto image = it.img->create<Type, 2>(3, 4);
        image->writeRawFixedPoint(inData).wait();
        //for (auto &el : inData) std::cout << "in-data:" << int(el) << std::endl;

        auto convolution = it.ops->create(
                operations::fixedConvolution2D::Spec{}
                    .setKernel({
                        { -1, 0, 1 },
                        { -3, 0, 3 },
                        { -1, 0, 1 }}, 1/3.0)
                    .setBias(1e-5),
                *image
            );

        auto outImage = it.img->createLike(*image);
        operations::callUnary(convolution, *image, *outImage).wait();

        auto outData = inData;
        outData.clear();
        outData.resize(inData.size(), 123);
        outImage->readRawFixedPoint(outData).wait();
        // for (auto &el : outData) std::cout << "out-data:" << int(el) << std::endl;

        // just for checking the output
        auto checkerProc = Processor::createInstant();
        auto factory = cpu::Image::createFactory(*checkerProc);
        auto checkImage = factory->create<Type, 2>(3, 4);
        checkImage->writeRawFixedPoint(outData).wait();

        const auto &outCpu = cpu::Image::castFrom(*checkImage);
        REQUIRE(outCpu.get<Type>(1, 1, 1).value == int((-2 + 3*6) / 3.0 + 0.5));
    }
}
