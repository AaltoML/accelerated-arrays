#include "image.hpp"
#include "function.hpp"

#include <limits>

namespace accelerated {

Image::~Image() = default;
Image::Factory::~Factory() = default;

std::unique_ptr<Image> Image::Factory::createLike(const Image &image) {
    return create(image.width, image.height, image.channels, image.dataType);
}

#define Y(dtype, name, n) \
    template <> std::unique_ptr<Image> Image::Factory::create<dtype, n>(int w, int h) \
    { return create(w, h, n, name); } \
    template <> ImageTypeSpec Image::Factory::getSpec<dtype, n>() \
    { return getSpec(n, name); }
#define X(dtype, name) \
    template <> bool ImageTypeSpec::isType<dtype>() const { return dataType == name; } \
    template <> void ImageTypeSpec::checkType<dtype>() const { aa_assert(isType<dtype>()); } \
    template <> ImageTypeSpec::DataType ImageTypeSpec::getType<dtype>() { return name; } \
    Y(dtype, name, 1) \
    Y(dtype, name, 2) \
    Y(dtype, name, 3) \
    Y(dtype, name, 4)
ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(X)
#undef X
#undef Y

std::size_t ImageTypeSpec::bytesPerChannel() const {
    switch (dataType) {
        #define X(dtype, name) case name: return sizeof(dtype);
        ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(X)
        #undef X
    }
    aa_assert(false && "invalid data type");
    return 0;
}

bool ImageTypeSpec::isIntegerType(DataType dtype) {
    switch (dtype) {

        case DataType::UINT8: return true;
        case DataType::SINT8: return true;
        case DataType::UINT16: return true;
        case DataType::SINT16: return true;
        case DataType::UINT32: return true;
        case DataType::SINT32: return true;

        case DataType::FLOAT32: return false;
        case DataType::UFIXED8: return false;
        case DataType::SFIXED8: return false;
        case DataType::UFIXED16: return false;
        case DataType::SFIXED16: return false;
        case DataType::UFIXED32: return false;
        case DataType::SFIXED32: return false;

        default: aa_assert(false);
    }
    return false;
}

bool ImageTypeSpec::isSigned(DataType dtype) {
    switch (dtype) {
        case DataType::FLOAT32: return true;

        case DataType::UINT8: return false;
        case DataType::SINT8: return true;
        case DataType::UINT16: return false;
        case DataType::SINT16: return true;
        case DataType::UINT32: return false;
        case DataType::SINT32: return true;
        case DataType::UFIXED8: return false;
        case DataType::SFIXED8: return true;
        case DataType::UFIXED16: return false;
        case DataType::SFIXED16: return true;
        case DataType::UFIXED32: return false;
        case DataType::SFIXED32: return true;
        default: aa_assert(false);
    }
    return false;
}

bool ImageTypeSpec::isFixedPoint(DataType dtype) {
    switch (dtype) {
        case DataType::UINT8: return false;
        case DataType::SINT8: return false;
        case DataType::UINT16: return false;
        case DataType::SINT16: return false;
        case DataType::UINT32: return false;
        case DataType::SINT32: return false;
        case DataType::FLOAT32: return false;

        case DataType::UFIXED8: return true;
        case DataType::SFIXED8: return true;
        case DataType::UFIXED16: return true;
        case DataType::SFIXED16: return true;
        case DataType::UFIXED32: return true;
        case DataType::SFIXED32: return true;
        default: aa_assert(false);
    }
    return false;
}

bool ImageTypeSpec::isFloat(DataType dtype) {
    return dtype == DataType::FLOAT32;
}

}
