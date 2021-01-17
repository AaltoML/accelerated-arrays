#include <catch2/catch.hpp>
#include <cmath>
#include <iostream>

#include "opengl/operations.hpp"
#include "opengl/image.hpp"
#include "opengl/adapters.hpp"

#ifdef TEST_OPENGL_WITH_VISIBLE_WINDOW
#include <chrono>
#include <thread>
#include <GLFW/glfw3.h>
#endif

TEST_CASE( "manual OpenGL", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    processor->enqueue([]() {
        GLuint texId, fbId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexImage2D(GL_TEXTURE_2D, 0,
            GL_RGBA,
            640, 400, 0,
            GL_RGBA,
            GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        glGenFramebuffers(1, &fbId);
        glBindFramebuffer(GL_FRAMEBUFFER, fbId);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
        REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

        glDeleteFramebuffers(1, &fbId);
        glDeleteTextures(1, &texId);
    });
    REQUIRE(true);
}

TEST_CASE( "adapters", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    for (bool leak : { true, false }) {
        processor->enqueue([leak]() {
            auto fb = opengl::FrameBuffer::create(640, 400, opengl::Image::getSpec(4, ImageTypeSpec::DataType::UINT8));
            if (!leak) fb->destroy();
        });
    }
    REQUIRE(true);
}

TEST_CASE( "fixed-point image", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    auto factory = opengl::Image::createFactory(*processor);

    REQUIRE(factory->getSpec<std::uint8_t, 2>().storageType == ImageTypeSpec::StorageType::GPU_OPENGL);

    typedef std::uint8_t IntType;
    typedef FixedPoint<IntType> Type;
    REQUIRE(sizeof(Type) == 1);

    // note: also testing read adapters: 2-channel image cannot be read directly
    auto image = factory->create<Type, 2>(19, 17); // also weird dimensions
    REQUIRE(image->storageType == ImageTypeSpec::StorageType::GPU_OPENGL);

    std::vector<IntType> inBuf, outBuf;
    for (std::size_t i = 0; i < image->numberOfScalars(); ++i)
        inBuf.push_back(i);

    REQUIRE(image->numberOfScalars() == 19*17*2);
    outBuf.resize(image->numberOfScalars(), 222);

    image->writeRawFixedPoint(inBuf); // single-threaded, no need to wait here
    image->readRawFixedPoint(outBuf).wait();

    for (std::size_t i = 0; i < inBuf.size(); ++i) {
        const int inEl = int(inBuf.at(i)), outEl = int(outBuf.at(i));
        // std::cout << "in: " << inEl << ", " << "out: " << outEl << std::endl;
        REQUIRE(inEl == outEl);
    }

    REQUIRE(int(outBuf[5]) == 5);

    double s = 1.0 / FixedPoint<IntType>::max();

    auto ops = opengl::operations::createFactory(*processor);
    auto fill = ops->fill({ 203 * s, 204 * s }).build(*image);

    operations::callNullary(fill, *image).wait();
    image->readRawFixedPoint(outBuf).wait();
    REQUIRE(int(outBuf.back()) == 204);

    std::vector<IntType> roiBuf;
    roiBuf.resize(4*5*2, 205);
    image->createROI(2, 3, 4, 5)->writeRawFixedPoint(roiBuf).wait();

    image->readRawFixedPoint(outBuf).wait();
    REQUIRE(int(outBuf.back()) == 204);
    REQUIRE(int(outBuf.at((3 * 19 + 2) * 2)) == 205);
}

#ifndef ACCELERATED_ARRAYS_USE_OPENGL_ES
TEST_CASE( "signed fixed-point image", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    auto factory = opengl::Image::createFactory(*processor);

    typedef std::int8_t IntType;
    typedef FixedPoint<IntType> Type;

    auto image = factory->create<Type, 4>(20, 30);

    std::vector<IntType> inBuf, outBuf;
    inBuf.resize(image->numberOfScalars(), -111);
    image->writeRawFixedPoint(inBuf);
    image->readRawFixedPoint(outBuf).wait();
    REQUIRE(int(outBuf[0]) == -111);

    double s = 1.0 / FixedPoint<IntType>::max();

    auto ops = opengl::operations::createFactory(*processor);
    auto fill = ops->fill({ 3 * s, 4 * s, 5 * s, -6 * s }).build(*image);

    operations::callNullary(fill, *image).wait();
    image->readRawFixedPoint(outBuf).wait();
    REQUIRE(int(outBuf.at(0)) == 3);
    REQUIRE(int(outBuf.at(1)) == 4);
    REQUIRE(int(outBuf.at(2)) == 5);
    REQUIRE(int(outBuf.at(3)) == -6);
    REQUIRE(int(outBuf.back()) == -6);
}
TEST_CASE( "16-bit integer image", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    auto factory = opengl::Image::createFactory(*processor);

    typedef std::int16_t Type;
    auto image = factory->create<Type, 1>(20, 30);

    std::vector<Type> inBuf, outBuf;
    inBuf.resize(image->numberOfScalars(), -111);
    image->write(inBuf);
    image->read(outBuf).wait();
    REQUIRE(int(outBuf[0]) == -111);

    auto ops = opengl::operations::createFactory(*processor);
    auto fill = ops->fill(-204).build(*image);

    operations::callNullary(fill, *image).wait();
    image->read(outBuf).wait();
    REQUIRE(int(outBuf.back()) == -204);
}
#endif

TEST_CASE( "32-bit integer image", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    auto factory = opengl::Image::createFactory(*processor);

    typedef std::int32_t Type;
    auto image = factory->create<Type, 1>(20, 30);

    std::vector<Type> inBuf, outBuf;
    inBuf.resize(image->numberOfScalars(), -111);
    image->write(inBuf);
    image->read(outBuf).wait();
    REQUIRE(int(outBuf[0]) == -111);

    auto ops = opengl::operations::createFactory(*processor);
    auto fill = ops->fill(-204).build(*image);

    operations::callNullary(fill, *image).wait();
    image->read(outBuf).wait();
    REQUIRE(int(outBuf.back()) == -204);
}

TEST_CASE( "float image", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    auto factory = opengl::Image::createFactory(*processor);

    typedef float Type;
    auto image = factory->create<Type, 3>(20, 30);

    std::vector<Type> inBuf, outBuf;
    inBuf.resize(image->numberOfScalars(), 3.14159);
    image->write(inBuf);
    image->read(outBuf).wait();
    REQUIRE(std::fabs(outBuf[0] - 3.14159) < 1e-5);

    auto ops = opengl::operations::createFactory(*processor);
    auto fill = ops->fill({ 201, 202, -3.14159 }).build(*image);

    operations::callNullary(fill, *image).wait();
    image->read(outBuf).wait();
    REQUIRE(std::fabs(outBuf.back() - (-3.14159)) < 1e-5);
}

#ifdef TEST_OPENGL_WITH_VISIBLE_WINDOW
TEST_CASE( "GLFW draw to window", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    GLFWwindow *wnd = nullptr;

    const int width = 320, height = 200;
    // TODO: won't work on Mac... refactor to use SYNC
    auto processor = opengl::createGLFWWindow(320, 200, "pink window",
        opengl::GLFWProcessorMode::ASYNC, (void**)&wnd);
    auto factory = opengl::Image::createFactory(*processor);
    auto ops = opengl::operations::createFactory(*processor);

    auto screen = factory->wrapScreen(width, height);
    auto region = screen->createROI(int(width*0.15), int(height*0.25), int(width*0.6), int(height*0.4));
    auto fill = ops->fill({ 1, 0, 0.5, 1 }).build(*screen);
    auto fill2 = ops->fill({ 0.6, 0.1, 0.4, 1 }).build(*region);

    REQUIRE(wnd == nullptr);
    int itr = 0;
    do {
        operations::callNullary(fill, *screen);
        operations::callNullary(fill2, *region).wait();
        processor->enqueue([&wnd]() {
            REQUIRE(wnd != nullptr);
            glfwSwapBuffers(wnd);
        }).wait();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (itr++ > 50) break;
    } while (!glfwWindowShouldClose(wnd));
}
#endif
