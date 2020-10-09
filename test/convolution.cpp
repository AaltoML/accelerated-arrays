#include <catch2/catch.hpp>

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
