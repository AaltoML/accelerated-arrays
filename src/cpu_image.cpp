#include "cpu_image.hpp"

namespace accelerated {

namespace {
ImageTypeSpec getSpec(int channels, ImageTypeSpec::DataType dtype) {
    return ImageTypeSpec {
        channels,
        dtype,
        ImageTypeSpec::StorageType::CPU
    };
}
}

class CpuImageImplementation final : public CpuImage {
private:
    std::vector<std::uint8_t> data;
    std::promise<void> instantPromise;

    inline int index(int x, int y) const {
        return (y * width + x) * channels;
    }

    std::future<void> instantlyResolved() {
        instantPromise = {};
        instantPromise.set_value();
        return instantPromise.get_future();
    }

protected:
    virtual void get(int x, int y, std::uint8_t *targetArray) const final {
        const auto offset = index(x, y);
        for (int i = 0; i < channels; ++i) targetArray[i] = data[i + offset];
    }

    virtual void set(int x, int y, const std::uint8_t *srcArray) final {
        const auto offset = index(x, y);
        for (int i = 0; i < channels; ++i) data[i + offset] = srcArray[i];
    }

    virtual std::future<void> readRaw(std::uint8_t *outputData) final {
        std::memcpy(outputData, data.data(), size());
        return instantlyResolved();
    }

    virtual std::future<void> writeRaw(const std::uint8_t *inputData) final {
        std::memcpy(data.data(), inputData, size());
        return instantlyResolved();
    }

public:
    CpuImageImplementation(int w, int h, int channels, DataType dtype) :
        CpuImage(w, h, channels, dtype)
    {
        data.resize(size());
    }
};

class CpuImageFactory final : public Image::Factory {
private:
    std::promise<std::unique_ptr<Image>> p;

public:
    std::future<std::unique_ptr<Image>> create(int w, int h, int channels, ImageTypeSpec::DataType dtype) final {
        p = {};
        p.set_value(std::unique_ptr<Image>(new CpuImageImplementation(w, h, channels, dtype)));
        return p.get_future();
    }
};

std::unique_ptr<Image::Factory> CpuImage::createFactory() {
    return std::unique_ptr<Image::Factory>(new CpuImageFactory);
}

CpuImage::CpuImage(int w, int h, int ch, DataType dtype) : Image(w, h, getSpec(ch, dtype)) {}

}
