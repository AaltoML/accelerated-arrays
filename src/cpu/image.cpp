#include "image.hpp"

namespace accelerated {
namespace cpu {
namespace {
class ImageImplementation final : public Image {
private:
    std::vector<std::uint8_t> data;
    Processor &processor;

    inline int index(int x, int y) const {
        return (y * width + x) * channels * bytesPerChannel();
    }

protected:
    void get(int x, int y, std::uint8_t *targetArray) const final {
        const auto bpc = bytesPerChannel();
        for (int i = 0; i < channels; ++i) get(x, y, i, targetArray + i * bpc);
    }

    void set(int x, int y, const std::uint8_t *srcArray) final {
        const auto bpc = bytesPerChannel();
        for (int i = 0; i < channels; ++i) set(x, y, i, srcArray + i * bpc);
    }

    void get(int x, int y, int channel, std::uint8_t *targetArray) const final {
        aa_assert(channel >= 0 && channel < channels);
        const auto bpc = bytesPerChannel();
        const auto offset = index(x, y) + channel * bpc;
        for (std::size_t i = 0; i < bpc; ++i) targetArray[i] = data[i + offset];
    }

    void set(int x, int y, int channel, const std::uint8_t *srcArray) final {
        aa_assert(channel >= 0 && channel < channels);
        const auto bpc = bytesPerChannel();
        const auto offset = index(x, y) + channel * bpc;
        for (std::size_t i = 0; i < bpc; ++i) data[i + offset] = srcArray[i];
    }

public:
    Future readRaw(std::uint8_t *outputData) final {
        return processor.enqueue([this, outputData]() {
            std::memcpy(outputData, data.data(), size());
        });
    }

    Future writeRaw(const std::uint8_t *inputData) final {
        return processor.enqueue([this, inputData]() {
            std::memcpy(data.data(), inputData, size());
        });
    }

    ImageImplementation(int w, int h, int channels, DataType dtype, Processor &p) :
        Image(w, h, channels, dtype), processor(p)
    {
        data.resize(size());
    }
};

class ImageFactory final : public Image::Factory {
private:
    Processor &processor;
public:
    ImageFactory(Processor &p) : processor(p) {}

    std::unique_ptr<::accelerated::Image> create(int w, int h, int channels, ImageTypeSpec::DataType dtype) final {
        return std::unique_ptr<::accelerated::Image>(new ImageImplementation(w, h, channels, dtype, processor));
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
        aa_assert(i >= 0 && i < size); // multiple mirroring undefined
        return true;
    case Image::Border::REPEAT:
        if (i < 0) i = 0;
        else if (i >= size) i = size - 1;
        return true;
    case Image::Border::WRAP:
        if (i < 0) i = size - (-i % size);
        else i = i % size;
        return true;
    case Image::Border::UNDEFINED:
    default:
        aa_assert(false);
        return false;
    }
}
}

bool Image::applyBorder(int &x, int &y, Border border) const {
    return applyBorder1D(x, width, border) && applyBorder1D(y, height, border);
}

void Image::setFloat(int x, int y, int channel, double value) {
    switch (dataType) {
    #define X(type, name) case name: set<type>(x, y, channel, type(value)); return;
    ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(X)
    #undef X
    }
    aa_assert(false);
}

double Image::getFloat(int x, int y, int channel) const {
    switch (dataType) {
    #define X(type, name) case name: return static_cast<double>(get<type>(x, y, channel));
    ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(X)
    #undef X
    }
    aa_assert(false);
    return 0;
}

double Image::getFloat(int x, int y, int channel, Border border) const {
    if (!applyBorder(x, y, border)) return 0.0;
    return getFloat(x, y, channel);
}

// single channel shorthands (double API)
void Image::setFloat(int x, int y, double value) {
    aa_assert(channels == 1);
    setFloat(x, y, 0, value);
}

double Image::getFloat(int x, int y) const {
    aa_assert(channels == 1);
    return getFloat(x, y, 0);
}

double Image::getFloat(int x, int y, Border border) const {
    aa_assert(channels == 1);
    return getFloat(x, y, 0, border);
}

ImageTypeSpec Image::getSpec(int channels, DataType dtype) {
    return ImageTypeSpec {
        channels,
        dtype,
        ImageTypeSpec::StorageType::CPU
    };
}

std::unique_ptr<Image::Factory> Image::createFactory(Processor &p) {
    return std::unique_ptr<Image::Factory>(new ImageFactory(p));
}

Image::Image(int w, int h, int ch, DataType dtype) : ::accelerated::Image(w, h, getSpec(ch, dtype)) {}

}
}
