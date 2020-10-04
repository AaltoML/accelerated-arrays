#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

#include "future.hpp"

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
    template <class T> static DataType getType();

    inline bool operator==(const ImageTypeSpec &other) const {
        return channels == other.channels &&
            dataType == other.dataType &&
            storageType == other.storageType;
    }

    inline bool operator!=(const ImageTypeSpec &other) const {
        return !(*this == other);
    }
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

    // border behavior
    enum class Border {
        UNDEFINED, // do not allow out-of-bounds reads
        ZERO,
        REPEAT,
        MIRROR,
        WRAP
    };

    // add some type safety wrappers
    template <class T> std::shared_ptr<Future> read(T *outputData);
    template <class T> std::shared_ptr<Future> write(const T *inputData);

    template <class T> inline std::shared_ptr<Future> read(std::vector<T> &output) {
        output.resize(numberOfPixels() * channels);
        return read<T>(output.data());
    }

    template <class T> inline std::shared_ptr<Future> write(const std::vector<T> &input) {
        assert(input.size() == numberOfPixels() * channels);
        return write<T>(input.data());
    }

    class Factory {
    public:
        virtual ~Factory();
        template <class T, int Channels> std::unique_ptr<Image> create(int w, int h);
        std::unique_ptr<Image> createLike(const Image &image);
        virtual std::unique_ptr<Image> create(int w, int h, int channels, DataType dtype) = 0;
    };

    typedef std::function< std::shared_ptr<Future>(std::vector<Image*> &inputs, Image &output) > Function;

protected:
    /** Asynchronous read operation */
    virtual std::shared_ptr<Future> readRaw(std::uint8_t *outputData) = 0;
    /** Asyncronous write operation */
    virtual std::shared_ptr<Future> writeRaw(const std::uint8_t *inputData) = 0;
};

#define ACCELERATED_IMAGE_FOR_EACH_TYPE(x) \
    x(std::uint8_t) \
    x(std::uint16_t) \
    x(std::int16_t) \
    x(std::uint32_t) \
    x(std::int32_t) \
    x(float)

#define ACCELERATED_IMAGE_FOR_EACH_NAMED_TYPE(x) \
    x(std::uint8_t, ImageTypeSpec::DataType::UINT8) \
    x(std::uint16_t, ImageTypeSpec::DataType::UINT16) \
    x(std::int16_t, ImageTypeSpec::DataType::SINT16) \
    x(std::uint32_t, ImageTypeSpec::DataType::UINT32) \
    x(std::int32_t, ImageTypeSpec::DataType::SINT32) \
    x(float, ImageTypeSpec::DataType::FLOAT32)

#define Y(dtype, n) \
    template <> std::unique_ptr<Image> Image::Factory::create<dtype, n>(int w, int h)
#define X(dtype) \
    template <> void ImageTypeSpec::checkType<dtype>() const; \
    template <> bool ImageTypeSpec::isType<dtype>() const; \
    template <> std::shared_ptr<Future> Image::read(dtype *out); \
    template <> std::shared_ptr<Future> Image::write(const dtype *in); \
    template <> ImageTypeSpec::DataType ImageTypeSpec::getType<dtype>(); \
    Y(dtype, 1); \
    Y(dtype, 2); \
    Y(dtype, 3); \
    Y(dtype, 4);
ACCELERATED_IMAGE_FOR_EACH_TYPE(X)
#undef X
#undef Y
}
