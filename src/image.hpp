#pragma once

#include <cassert>
#include <cstdint>
#include <future>
#include <memory>
#include <vector>

namespace accelerated {

struct ImageTypeSpec {
    /**
     * Number of "channels" per pixel. Possible values range from 1 to 4.
     * May be interpreted freely by the user, but often labeled as R,G,B,A
     * in a 4-channel image.
     */
    const int channels;

    /** Channel data type */
    const enum class DataType {
        UINT8,
        // TODO: should specify big vs little endian
        UINT16,
        SINT16,
        UINT32,
        SINT32,
        FLOAT32
    } dataType;

    /**
     * Where is the image stored
     */
    const enum class StorageType {
        CPU,
        GPU_OPENGL,
        GPU_OPENGL_EXTERNAL
    } storageType;

    inline std::size_t bytesPerChannel() const {
        switch (dataType) {
            case DataType::UINT8: return 1;
            case DataType::UINT16: return 2;
            case DataType::SINT16: return 2;
            case DataType::UINT32: return 4;
            case DataType::SINT32: return 4;
            case DataType::FLOAT32: return 4;
        }
        assert(false && "invalid data type");
        return 0;
    }

    inline std::size_t bytesPerPixel() const { return bytesPerChannel() * channels; }

    template <class T> bool isType() const;
    template <class T> void checkType() const;
};

/**
 * An abstraction for images that may be processed on multiple types of
 * hardware: both CPUs or GPUs. They represent image-like arrays but do not
 * offer direct pixel level access.
 */
struct Image : ImageTypeSpec {
    /**
     * Fixed 2D image dimensions whose product is the number of pixels
     */
    const int width, height;

    virtual ~Image();

    Image(int w, int h, const ImageTypeSpec &spec) :
        ImageTypeSpec(spec),
        width(w),
        height(h)
    {};

    inline std::size_t numberOfPixels() const { return width * height; }

    inline std::size_t size() const { return numberOfPixels() * bytesPerPixel(); }

    // add some type safety wrappers
    template <class T> std::future<void> read(T *outputData);
    template <class T> std::future<void> write(const T *inputData);

    template <class T> inline std::future<void> read(std::vector<T> &output) {
        output.resize(numberOfPixels() * channels);
        return read<T>(output.data());
    }

    template <class T> inline std::future<void> write(const std::vector<T> &input) {
        assert(input.size() == numberOfPixels() * channels);
        return write<T>(input.data());
    }

    class Factory {
    public:
        virtual ~Factory();
        template <class T, int Channels> std::future<std::unique_ptr<Image>> create(int w, int h);

    protected:
        virtual std::future<std::unique_ptr<Image>> create(int w, int h, int channels, DataType dtype) = 0;
    };

protected:
    /** Asynchronous read operation */
    virtual std::future<void> readRaw(std::uint8_t *outputData) = 0;
    /** Asyncronous write operation */
    virtual std::future<void> writeRaw(const std::uint8_t *inputData) = 0;
};

#define Y(dtype, n) \
    template <> std::future<std::unique_ptr<Image>> Image::Factory::create<dtype, n>(int w, int h)
#define X(dtype) \
    template <> void ImageTypeSpec::checkType<dtype>() const; \
    template <> bool ImageTypeSpec::isType<dtype>() const; \
    template <> std::future<void> Image::read(dtype *out); \
    template <> std::future<void> Image::write(const dtype *in); \
    Y(dtype, 1); \
    Y(dtype, 2); \
    Y(dtype, 3); \
    Y(dtype, 4);
X(std::uint8_t)
X(std::uint16_t)
X(std::int16_t)
X(std::uint32_t)
X(std::int32_t)
X(float)
#undef X
#undef Y
}
