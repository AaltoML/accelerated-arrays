#pragma once

#include "../image.hpp"

namespace accelerated {
namespace opengl {
struct FrameBuffer;
class Image : public ::accelerated::Image {
public:
    virtual int getTextureId() const = 0;
    virtual bool supportsDirectRead() const = 0;
    virtual bool supportsDirectWrite() const = 0;

    virtual FrameBuffer &getFrameBuffer() = 0;

    class Factory : public ::accelerated::Image::Factory {
    public:
        /**
         * Create a read-only reference to an existing OpenGL texture.
         * (attempting a write operation on it will produce an error),
         *
         * For textures of type GL_TEXTURE_EXTERNAL_OES, like GPU camera images,
         * change the stype argument to StorageType::GPU_OPENGL_EXTERNAL
         */
        template <class T, int Channels> std::unique_ptr<Image> wrapTexture(int textureId, int w, int h, StorageType stype = StorageType::GPU_OPENGL) {
            return wrapTexture(textureId, w, h, Image::getSpec(Channels, ImageTypeSpec::getType<T>(), stype));
        }

        /**
         * Create a write reference to an existing OpenGL frame buffer.
         * The relevant data is assumed to be in GL_COLOR_ATTACHMENT0.
         * It may be possible to read pixel data from the exiting FBO to CPU
         * memory using read(Raw) but this Image cannot be used as an input to
         * other accelerated::Functions.
         *
         * If you want read-write, the frame buffer usually has a texture for
         * which you can create a separate read-only reference using wrapTexture.
         */
        template <class T, int Channels> std::unique_ptr<Image> wrapFrameBuffer(int fboId, int w, int h) {
            return wrapFrameBuffer(fboId, w, h, Image::getSpec(Channels, ImageTypeSpec::getType<T>()));
        }

        /**
         * Will produce an output target that is directly drawn on the screen,
         * GL_BACK buffer. Depending on the system, you may still have to swap
         * buffers manually, e.g., glfwSwapBuffers.
         *
         * The screen is assumed to have 4 channels (RGBA) and is always of
         * data type FixedPoint<std::uint8_t>.
         */
        virtual std::unique_ptr<Image> wrapScreen(int w, int h) = 0;

        virtual std::unique_ptr<Image> wrapTexture(int textureId, int w, int h, const ImageTypeSpec &spec) = 0;
        virtual std::unique_ptr<Image> wrapFrameBuffer(int frameBufferId, int w, int h, const ImageTypeSpec &spec) = 0;
    };

    static std::unique_ptr<Factory> createFactory(Processor &processor);
    static Image &castFrom(::accelerated::Image &image);
    static bool isCompatible(ImageTypeSpec::StorageType stype);

    static ImageTypeSpec getSpec(int channels, DataType dtype, StorageType stype = StorageType::GPU_OPENGL);

protected:
    Image(int w, int h, const ImageTypeSpec &spec);
};

}
}
