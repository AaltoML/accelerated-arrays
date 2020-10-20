#include "image.hpp"

namespace accelerated {
namespace cpu {
namespace {
class ImageBase : public Image {
private:
    inline int index(int x, int y) const {
        return (y * width + x) * channels * bytesPerChannel();
    }

    bool isCopyCompatible(::accelerated::Image &other) const {
        // TODO: fixed point vs integer types would work OK in direct copy too
        return other.channels == channels &&
            other.dataType == dataType &&
            other.width == width &&
            other.height == height;
    }

protected:
    std::uint8_t *data = nullptr;

    void get(int x, int y, std::uint8_t *targetArray) const final {
        const auto bpc = bytesPerChannel();
        for (int i = 0; i < channels; ++i) get(x, y, i, targetArray + i * bpc);
    }

    void set(int x, int y, const std::uint8_t *srcArray) final {
        const auto bpc = bytesPerChannel();
        for (int i = 0; i < channels; ++i) set(x, y, i, srcArray + i * bpc);
    }

    void get(int x, int y, int channel, std::uint8_t *targetArray) const final {
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channel >= 0 && channel < channels);
        const auto bpc = bytesPerChannel();
        const auto offset = index(x, y) + channel * bpc;
        for (std::size_t i = 0; i < bpc; ++i) targetArray[i] = data[i + offset];
    }

    void set(int x, int y, int channel, const std::uint8_t *srcArray) final {
        ACCELERATED_ARRAYS_PIXEL_ASSERT(channel >= 0 && channel < channels);
        const auto bpc = bytesPerChannel();
        const auto offset = index(x, y) + channel * bpc;
        for (std::size_t i = 0; i < bpc; ++i) data[i + offset] = srcArray[i];
    }

public:
    Future readRaw(std::uint8_t *outputData) final {
        std::memcpy(outputData, data, size());
        return Future::instantlyResolved();
    }

    Future writeRaw(const std::uint8_t *inputData) final {
        std::memcpy(data, inputData, size());
        return Future::instantlyResolved();
    }

    Future copyFrom(::accelerated::Image &other) final {
        aa_assert(isCopyCompatible(other));
        return other.readRaw(data);
    }

    Future copyTo(::accelerated::Image &other) const final {
        aa_assert(isCopyCompatible(other));
        return other.writeRaw(data);
    }

    std::uint8_t *getDataRaw() final {
        return data;
    }

    ImageBase(int w, int h, int channels, DataType dtype) :
        Image(w, h, channels, dtype)
    {}
};

class ImageWithData final : public ImageBase {
private:
    std::vector<std::uint8_t> dataVec;

public:
    ImageWithData(int w, int h, int channels, DataType dtype) :
        ImageBase(w, h, channels, dtype)
    {
        dataVec.resize(size());
        data = dataVec.data();
    }
};

class ImageReference final : public ImageBase {
public:
    ImageReference(int w, int h, int channels, DataType dtype, std::uint8_t *extData) :
        ImageBase(w, h, channels, dtype)
    {
        data = extData;
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

#define X(dtype) \
template<> dtype Image::get<dtype>(int x, int y, int channel) const { \
    checkType<dtype>(); \
    dtype result; \
    ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height); \
    get(x, y, channel, reinterpret_cast<std::uint8_t*>(&result)); \
    return result; \
} \
template<> void Image::set<dtype>(int x, int y, int channel, dtype value) { \
    checkType<dtype>(); \
    ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height); \
    set(x, y, channel, reinterpret_cast<const std::uint8_t*>(&value)); \
}
ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_TYPE(X)
#undef X

template<> float Image::get<float>(int x, int y, int channel) const {
    switch (dataType) {
    case DataType::FLOAT32: {
        float result;
        get(x, y, channel, reinterpret_cast<std::uint8_t*>(&result));
        return result;
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
        set(x, y, channel, reinterpret_cast<const std::uint8_t*>(&value));
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

Image::Image(int w, int h, int ch, DataType dtype) : ::accelerated::Image(w, h, getSpec(ch, dtype)) {}

}
}
