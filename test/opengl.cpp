#include <catch2/catch.hpp>
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

TEST_CASE( "image", "[accelerated-arrays-opengl]" ) {
    using namespace accelerated;
    auto processor = opengl::createGLFWProcessor();
    auto factory = opengl::Image::createFactory(*processor);
    auto image = factory->create<std::uint8_t, 4>(20, 30);

    std::vector<std::uint8_t> inBuf, outBuf;
    inBuf.resize(image->size(), 111);
    outBuf.resize(image->size(), 222);

    image->write(inBuf); // single-threaded, no need to wait here
    image->read(outBuf).wait();
    REQUIRE(outBuf[0] == 111);

    auto ops = opengl::operations::createFactory(*processor);
    auto fill = ops->create(
            operations::fill::Spec{}.setValue({ 201, 202, 203, 204 }),
            *image);

    operations::callNullary(fill, *image).wait();
    image->read(outBuf).wait();
    REQUIRE(int(outBuf.back()) == 204);
}
