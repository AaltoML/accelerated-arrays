#include "image.hpp"
#include "function.hpp"

namespace accelerated {

Image::~Image() = default;
Image::Factory::~Factory() = default;

std::future<std::unique_ptr<Image>> Image::Factory::createLike(const Image &image) {
    return create(image.width, image.height, image.channels, image.dataType);
}

#define Y(dtype, name, n) \
    template <> std::future<std::unique_ptr<Image>> Image::Factory::create<dtype, n>(int w, int h) \
    { return create(w, h, n, name); }
#define X(dtype, name) \
    template <> bool ImageTypeSpec::isType<dtype>() const { return dataType == name; } \
    template <> void ImageTypeSpec::checkType<dtype>() const { assert(isType<dtype>()); } \
    template <> ImageTypeSpec::DataType ImageTypeSpec::getType<dtype>() { return name; } \
    template <> std::future<void> Image::read(dtype *out) \
        { assert(isType<dtype>()); return readRaw(reinterpret_cast<std::uint8_t*>(out)); } \
    template <> std::future<void> Image::write(const dtype *in) \
        { assert(isType<dtype>()); return writeRaw(reinterpret_cast<const std::uint8_t*>(in)); } \
    Y(dtype, name, 1) \
    Y(dtype, name, 2) \
    Y(dtype, name, 3) \
    Y(dtype, name, 4)
ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(X)
#undef X
#undef Y

}
