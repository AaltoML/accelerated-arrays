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

struct ImplementationBase : public Image {
    std::uint8_t *data;
    std::size_t rowWidth;

    ImplementationBase(int w, int h, int ch, DataType dtype) :
        Image(w, h, ch, dtype),
        data(nullptr),
        rowWidth(std::size_t(w))
    {}

    bool isContiguous() const {
        return int(rowWidth) == width;
    }

    std::unique_ptr<::accelerated::Image> createROI(int x0, int y0, int roiWidth, int roiHeight) final {
        auto *roiData = data + ((y0 * rowWidth + x0) * channels) * bytesPerChannel();
        return createReference(roiWidth, roiHeight, channels, dataType, roiData, rowWidth);
    }

    template<class dtype> dtype getNative(int x, int y, int channel) const {
        checkType<dtype>();
        ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
        const dtype* src = reinterpret_cast<const dtype*>(__builtin_assume_aligned(data, sizeof(dtype)));
        return src[(y * rowWidth + x) * channels + channel];
    }

    template<class dtype> void setNative(int x, int y, int channel, dtype value) {
        checkType<dtype>();
        ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
        dtype* target = reinterpret_cast<dtype*>(__builtin_assume_aligned(data, sizeof(dtype)));
        target[(y * rowWidth + x) * channels + channel] = value;
    }

    float getFloat(int x, int y, int channel) const {
        switch (dataType) {
        case DataType::FLOAT32: {
            ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
            const float* src = reinterpret_cast<const float*>(__builtin_assume_aligned(data, sizeof(float)));
            return src[(y * rowWidth + x) * channels + channel];
        }
        #define X(type, name) case name: return static_cast<double>(getNative<type>(x, y, channel));
        ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_NAMED_TYPE(X)
        #undef X
        }
        ACCELERATED_ARRAYS_PIXEL_ASSERT(false);
        return 0;
    }

    void setFloat(int x, int y, int channel, float value) {
        switch (dataType) {
        case DataType::FLOAT32: {
            ACCELERATED_ARRAYS_PIXEL_ASSERT(x >= 0 && y >= 0 && x < width && y < height);
            float* target = reinterpret_cast<float*>(__builtin_assume_aligned(data, sizeof(float)));
            target[(y * rowWidth + x) * channels + channel] = value;
            return;
        }
        #define X(type, name) case name: setNative<type>(x, y, channel, type(value)); return;
        ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_NAMED_TYPE(X)
        #undef X
        }
        ACCELERATED_ARRAYS_PIXEL_ASSERT(false);
    }
};

class ImageWithData final : public ImplementationBase {
private:
    std::vector<std::uint8_t> dataVec;

public:
    ImageWithData(int w, int h, int channels, DataType dtype) :
        ImplementationBase(w, h, channels, dtype)
    {
        dataVec.resize(size());
        data = dataVec.data();
    }
};

class ImageReference final : public ImplementationBase {
public:
    ImageReference(int w, int h, int channels, DataType dtype, std::uint8_t *extData, std::size_t rowWidthPixels) :
        ImplementationBase(w, h, channels, dtype)
    {
        if (rowWidthPixels != 0) {
            aa_assert(rowWidthPixels >= rowWidth);
            rowWidth = rowWidthPixels;
        }
        data = extData;
        // check alignment
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
    const auto &impl = reinterpret_cast<const ImplementationBase&>(*this);
    aa_assert(impl.isContiguous());
    std::memcpy(outputData, impl.data, size());
    return Future::instantlyResolved();
}

Future Image::writeRaw(const std::uint8_t *inputData) {
    const auto &impl = reinterpret_cast<ImplementationBase&>(*this);
    aa_assert(impl.isContiguous());
    std::memcpy(impl.data, inputData, size());
    return Future::instantlyResolved();
}

Future Image::copyFrom(::accelerated::Image &other) {
    const auto &impl = reinterpret_cast<ImplementationBase&>(*this);
    aa_assert(impl.isContiguous());
    aa_assert(isCopyCompatible(*this, other));
    return other.readRaw(impl.data);
}

Future Image::copyTo(::accelerated::Image &other) const {
    const auto &impl = reinterpret_cast<const ImplementationBase&>(*this);
    aa_assert(impl.isContiguous());
    aa_assert(isCopyCompatible(*this, other));
    return other.writeRaw(impl.data);
}

std::uint8_t *Image::getDataRaw() {
    return reinterpret_cast<ImplementationBase&>(*this).data;
}

std::size_t Image::bytesPerRow() const {
    return reinterpret_cast<const ImplementationBase&>(*this).rowWidth * bytesPerPixel();
}

#define X(dtype) \
template<> dtype Image::get<dtype>(int x, int y, int channel) const { \
    return reinterpret_cast<const ImplementationBase&>(*this).getNative<dtype>(x, y, channel); \
} \
template<> void Image::set<dtype>(int x, int y, int channel, dtype value) { \
    reinterpret_cast<ImplementationBase&>(*this).setNative<dtype>(x, y, channel, value); \
}
ACCELERATED_IMAGE_FOR_EACH_NON_FLOAT_TYPE(X)
#undef X

template<> float Image::get<float>(int x, int y, int channel) const {
    return reinterpret_cast<const ImplementationBase&>(*this).getFloat(x, y, channel);
}

template<> void Image::set<float>(int x, int y, int channel, float value) {
    reinterpret_cast<ImplementationBase&>(*this).setFloat(x, y, channel, value);
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
    return std::unique_ptr<Image>(new ImageReference(w, h, channels, dtype, data, 0));
}

std::unique_ptr<Image> Image::createReference(int w, int h, int channels, DataType dtype, std::uint8_t *data, std::size_t rowWidthPixels) {
    aa_assert(rowWidthPixels > 0);
    return std::unique_ptr<Image>(new ImageReference(w, h, channels, dtype, data, rowWidthPixels));
}

Image::Image(int w, int h, int ch, DataType dtype) :
    ::accelerated::Image(w, h, getSpec(ch, dtype))
{}

}
}
