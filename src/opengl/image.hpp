#pragma once

#include "../image.hpp"

namespace accelerated {
namespace opengl {
struct FrameBuffer;
class Image : public ::accelerated::Image {
public:
    // OpenGL-specifics
    virtual int getTextureId() const = 0;
    virtual bool supportsDirectRead() const = 0;
    virtual bool supportsDirectWrite() const = 0;

    virtual FrameBuffer &getFrameBuffer() = 0;

    class Factory : public ::accelerated::Image::Factory {
    public:
        template <class T, int Channels> std::unique_ptr<Image> wrapTexture(int textureId, int w, int h, StorageType stype = StorageType::GPU_OPENGL) {
            return wrapTexture(textureId, w, h, getSpec(Channels, ImageTypeSpec::getType<T>(), stype));
        }

        virtual std::unique_ptr<Image> wrapTexture(int textureId, int w, int h, const ImageTypeSpec &spec) = 0;
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
