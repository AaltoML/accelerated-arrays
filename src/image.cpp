#include "image.hpp"

namespace accelerated {

Image::~Image() = default;
Image::Factory::~Factory() = default;

#define Y(dtype, name, n) \
    template <> std::future<std::unique_ptr<Image>> Image::Factory::create<dtype, n>(int w, int h) \
    { return create(w, h, n, DataType::name); }
#define X(dtype, name) \
    template <> bool ImageTypeSpec::isType<dtype>() const { return dataType == DataType::name; } \
    template <> void ImageTypeSpec::checkType<dtype>() const { assert(isType<dtype>()); } \
    template <> std::future<void> Image::read(dtype *out) \
        { assert(isType<dtype>()); return readRaw(reinterpret_cast<std::uint8_t*>(out)); } \
    template <> std::future<void> Image::write(const dtype *in) \
        { assert(isType<dtype>()); return writeRaw(reinterpret_cast<const std::uint8_t*>(in)); } \
    Y(dtype, name, 1) \
    Y(dtype, name, 2) \
    Y(dtype, name, 3) \
    Y(dtype, name, 4)

X(std::uint8_t, UINT8)
X(std::uint16_t, UINT16)
X(std::int16_t, SINT16)
X(std::uint32_t, UINT32)
X(std::int32_t, SINT32)
X(float, FLOAT32)

#undef X
#undef Y
}
