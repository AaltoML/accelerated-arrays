#include "image.hpp"

namespace accelerated {
namespace cpu {
namespace {
bool isCopyCompatible(const ::accelerated::Image &a, const ::accelerated::Image &b) {
    // TODO: fixed point vs integer types would work OK in direct copy too
    return a.channels == b.channels &&
        a.dataType == b.dataType &&
        a.width == b.width &&
        a.height == b.height;
}

class ImageWithData final : public Image {
private:
    std::vector<std::uint8_t> dataVec;

public:
    ImageWithData(int w, int h, int channels, DataType dtype) :
        Image(w, h, channels, dtype)
    {
        dataVec.resize(size());
        data = dataVec.data();
    }
};

class ImageReference final : public Image {
public:
    ImageReference(int w, int h, int channels, DataType dtype, std::uint8_t *extData) :
        Image(w, h, channels, dtype)
    {
        data = extData;
        aa_assert(reinterpret_cast<std::uintptr_t>(data) % bytesPerChannel() == 0);
    }
};

class ImageFactory final : public Image::Factory {
public:
    std::unique_ptr<::accelerated::Image> create(int w, int h, int channels, ImageTypeSpec::DataType dtype) final {
        return std::unique_ptr<::accelerated::Image>(new ImageWithData(w, h, channels, dtype));
    }

    ImageTypeSpec getSpec(int channels, ImageTypeSpec::DataType dtype) final {
        return Image::getSpec(channels, dtype);
    }
};

inline bool applyBorder1D(int &i, int size, Image::Border border) {
    if (i >= 0 && i < size) {
        return true;
    }
    switch (border) {
    case Image::Border::ZERO:
        return false;
    case Image::Border::MIRROR:
        if (i < 0) i = -i;
        else if (i >= size) i = size - 1 - (i - (size - 1));
        ACCELERATED_ARRAYS_PIXEL_ASSERT(i >= 0 && i < size); // multiple mirroring undefined
        return true;
    case Image::Border::REPEAT:
        if (i < 0) i = size - (-i % size);
        else i = i % size;
        return true;
    case Image::Border::CLAMP:
        if (i < 0) i = 0;
        else if (i >= size) i = size - 1;
        return true;
    case Image::Border::UNDEFINED:
    default:
        ACCELERATED_ARRAYS_PIXEL_ASSERT(false);
        return false;
    }
}
}

bool Image::applyBorder(int &x, int &y, Border border) const {
    return applyBorder1D(x, width, border) && applyBorder1D(y, height, border);
}

Future Image::readRaw(std::uint8_t *outputData) {
    std::memcpy(outputData, data, size());
    return Future::instantlyResolved();
}

Future Image::writeRaw(const std::uint8_t *inputData) {
    std::memcpy(data, inputData, size());
    return Future::instantlyResolved();
}

Future Image::copyFrom(::accelerated::Image &other) {
    aa_assert(isCopyCompatible(*this, other));
    return other.readRaw(data);
}

Future Image::copyTo(::accelerated::Image &other) const {
    aa_assert(isCopyCompatible(*this, other));
    return other.writeRaw(data);
}

std::uint8_t *Image::getDataRaw() {
    return data;
}

#define X(dtype) \
template<> dtype Image::get<dtype>(int x, int y, int channel) const { \
    checkType<dtype>(); \
    ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height); \
    const dtype* src = reinterpret_cast<const dtype*>(__builtin_assume_aligned(data, sizeof(dtype))); \
    return src[(y * width + x) * channels + channel]; \
} \
template<> void Image::set<dtype>(int x, int y, int channel, dtype value) { \
    checkType<dtype>(); \
    ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height); \
    dtype* target = reinterpret_cast<dtype*>(__builtin_assume_aligned(data, sizeof(dtype))); \
    target[(y * width + x) * channels + channel] = value; \
}
ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_TYPE(X)
#undef X

template<> float Image::get<float>(int x, int y, int channel) const {
    switch (dataType) {
    case DataType::FLOAT32: {
        ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
        const float* src = reinterpret_cast<const float*>(__builtin_assume_aligned(data, sizeof(float)));
        return src[(y * width + x) * channels + channel];
    }
    #define X(type, name) case name: return static_cast<double>(get<type>(x, y, channel));
    ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_NAMED_TYPE(X)
    #undef X
    }
    ACCELERATED_ARRAYS_PIXEL_ASSERT(false);
    return 0;
}

template<> void Image::set<float>(int x, int y, int channel, float value) {
    switch (dataType) {
    case DataType::FLOAT32: {
        ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
        float* target = reinterpret_cast<float*>(__builtin_assume_aligned(data, sizeof(float)));
        target[(y * width + x) * channels + channel] = value;
        return;
    }
    #define X(type, name) case name: set<type>(x, y, channel, type(value)); return;
    ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_NAMED_TYPE(X)
    #undef X
    }
    ACCELERATED_ARRAYS_PIXEL_ASSERT(false);
}

ImageTypeSpec Image::getSpec(int channels, DataType dtype) {
    return ImageTypeSpec {
        channels,
        dtype,
        ImageTypeSpec::StorageType::CPU
    };
}

std::unique_ptr<Image::Factory> Image::createFactory() {
    return std::unique_ptr<Image::Factory>(new ImageFactory());
}

std::unique_ptr<Image> Image::createReference(int w, int h, int channels, DataType dtype, std::uint8_t *data) {
    return std::unique_ptr<Image>(new ImageReference(w, h, channels, dtype, data));
}

Image::Image(int w, int h, int ch, DataType dtype) :
    ::accelerated::Image(w, h, getSpec(ch, dtype)),
    data(nullptr)
{}

}
}
