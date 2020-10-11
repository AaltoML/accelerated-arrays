#include <catch2/catch.hpp>
#include <cmath>

#include "opengl/operations.hpp"
#include "opengl/image.hpp"

#include "opengl/adapters.hpp"

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

    typedef std::uint8_t IntType;
    typedef FixedPoint<IntType> Type;
    REQUIRE(sizeof(Type) == 1);

    auto image = factory->create<Type, 4>(20, 30);

    std::vector<IntType> inBuf, outBuf;
    inBuf.resize(image->numberOfScalars(), 111);
    outBuf.resize(image->numberOfScalars(), 222);

    image->writeRawFixedPoint(inBuf); // single-threaded, no need to wait here
    image->readRawFixedPoint(outBuf).wait();
    REQUIRE(int(outBuf[0]) == 111);

    double s = 1.0 / FixedPoint<IntType>::max();

    auto ops = opengl::operations::createFactory(*processor);
    auto fill = ops->create(
            operations::fill::Spec{}.setValue({ 201 * s, 202 * s, 203 * s, 204 * s }),
            *image);

    operations::callNullary(fill, *image).wait();
    image->readRawFixedPoint(outBuf).wait();
    REQUIRE(int(outBuf.back()) == 204);
}

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
    auto fill = ops->create(
            operations::fill::Spec{}.setValue({ 3 * s, 4 * s, 5 * s, -6 * s }),
            *image);

    operations::callNullary(fill, *image).wait();
    image->readRawFixedPoint(outBuf).wait();
    REQUIRE(int(outBuf.at(0)) == 3);
    REQUIRE(int(outBuf.at(1)) == 4);
    REQUIRE(int(outBuf.at(2)) == 5);
    REQUIRE(int(outBuf.at(3)) == -6);
    REQUIRE(int(outBuf.back()) == -6);
}

TEST_CASE( "integer image", "[accelerated-arrays-opengl]" ) {
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
    auto fill = ops->create(
            operations::fill::Spec{}.setValue({ -204 }),
            *image);

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
    auto fill = ops->create(
            operations::fill::Spec{}.setValue({ 201, 202, -3.14159 }),
            *image);

    operations::callNullary(fill, *image).wait();
    image->read(outBuf).wait();
    REQUIRE(std::fabs(outBuf.back() - (-3.14159)) < 1e-5);
}
